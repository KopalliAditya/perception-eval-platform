#include "evaluator.hpp"

#include <algorithm>
#include <future>
#include <tuple>
#include <unordered_map>
#include <vector>

#include "io_util.hpp"
#include "matching.hpp"
#include "thread_pool.hpp"

namespace pev {

bool SliceKey::operator<(const SliceKey& o) const {
    return std::tie(type, range, difficulty, tag) <
           std::tie(o.type, o.range, o.difficulty, o.tag);
}

bool SliceKey::operator==(const SliceKey& o) const {
    return type == o.type && range == o.range && difficulty == o.difficulty &&
           tag == o.tag;
}

std::string slice_label(const SliceKey& key) {
    return to_string(key.type) + " @ " + to_string(key.range) + ", " +
           to_string(key.difficulty) + ", " + key.tag;
}

namespace {

// A matched/unmatched detection record carried through aggregation.
struct DetRecord {
    double score = 0.0;
    bool is_tp = false;
    double heading_acc = 0.0;
    Difficulty gt_difficulty = Difficulty::Level1;  // valid when is_tp
};

// Raw contributions for a (type, range, tag) group.
struct GroupData {
    std::vector<DetRecord> dets;
    long npos_l1 = 0;
    long npos_l2 = 0;

    void merge(const GroupData& o) {
        dets.insert(dets.end(), o.dets.begin(), o.dets.end());
        npos_l1 += o.npos_l1;
        npos_l2 += o.npos_l2;
    }
};

using GroupKey = std::tuple<ObjectType, RangeBin, std::string>;

// Per-scene evaluation output.
struct SceneEval {
    std::string scene_id;
    std::map<GroupKey, GroupData> groups;
    std::map<std::string, std::vector<double>> latency_by_tag;
};

// The tags a scene contributes to: its real tags plus the synthetic "(any)".
std::vector<std::string> contribution_tags(const Scene& scene) {
    std::vector<std::string> out = scene.tags;
    out.push_back(kAnyTag);
    return out;
}

SceneEval evaluate_scene(const Scene& scene, const RunScene* run_scene,
                         double iou_threshold) {
    SceneEval ev;
    ev.scene_id = scene.id;
    const std::vector<std::string> tags = contribution_tags(scene);

    std::unordered_map<int, const RunFrame*> run_frames;
    if (run_scene != nullptr) {
        for (const RunFrame& rf : run_scene->frames) {
            run_frames[rf.index] = &rf;
        }
    }

    for (const GtFrame& frame : scene.frames) {
        const RunFrame* rf = nullptr;
        auto it = run_frames.find(frame.index);
        if (it != run_frames.end()) {
            rf = it->second;
            for (const std::string& t : tags) {
                ev.latency_by_tag[t].push_back(rf->latency_ms);
            }
        }

        for (ObjectType type : all_types()) {
            std::vector<GtObject> gts;
            for (const GtObject& o : frame.objects) {
                if (o.type == type) gts.push_back(o);
            }
            std::vector<Detection> dets;
            if (rf != nullptr) {
                for (const Detection& d : rf->detections) {
                    if (d.type == type) dets.push_back(d);
                }
            }
            if (gts.empty() && dets.empty()) {
                continue;
            }

            // Count ground-truth positives into their (range, difficulty, tag)
            // buckets.
            for (const GtObject& o : gts) {
                const RangeBin rb = o.range_bin();
                const Difficulty diff = o.difficulty();
                for (const std::string& t : tags) {
                    GroupData& gd = ev.groups[GroupKey{type, rb, t}];
                    if (diff == Difficulty::Level1) {
                        ++gd.npos_l1;
                    } else {
                        ++gd.npos_l2;
                    }
                }
            }

            const ClassMatchResult mr = match_class(dets, gts, iou_threshold);

            // True positives land in the matched ground-truth's range bin and
            // carry its difficulty.
            for (const Match& m : mr.true_positives) {
                const GtObject& g = gts[m.gt_index];
                DetRecord rec;
                rec.score = dets[m.det_index].score;
                rec.is_tp = true;
                rec.heading_acc = m.heading_acc;
                rec.gt_difficulty = g.difficulty();
                const RangeBin rb = g.range_bin();
                for (const std::string& t : tags) {
                    ev.groups[GroupKey{type, rb, t}].dets.push_back(rec);
                }
            }
            // False positives land in the detection's own range bin.
            for (std::size_t di : mr.false_positive_dets) {
                DetRecord rec;
                rec.score = dets[di].score;
                rec.is_tp = false;
                const RangeBin rb = dets[di].range_bin();
                for (const std::string& t : tags) {
                    ev.groups[GroupKey{type, rb, t}].dets.push_back(rec);
                }
            }
        }
    }
    return ev;
}

// Build slice metrics from a set of groups, filtering to a difficulty
// (`diff_any` selects both levels).
SliceMetrics metrics_from_groups(const std::vector<const GroupData*>& groups,
                                 Difficulty diff, bool diff_any,
                                 const LatencyStats& latency) {
    long npos = 0;
    std::vector<DetEntry> entries;
    for (const GroupData* gd : groups) {
        npos += diff_any ? (gd->npos_l1 + gd->npos_l2)
                         : (diff == Difficulty::Level1 ? gd->npos_l1
                                                       : gd->npos_l2);
        for (const DetRecord& r : gd->dets) {
            if (r.is_tp) {
                if (diff_any || r.gt_difficulty == diff) {
                    entries.push_back(DetEntry{r.score, true, r.heading_acc});
                }
            } else {
                entries.push_back(DetEntry{r.score, false, 0.0});
            }
        }
    }
    const ApResult ap = average_precision(std::move(entries), npos);
    SliceMetrics m;
    m.num_gt = npos;
    m.ap = ap.ap;
    m.aph = ap.aph;
    m.counts = ap.counts();
    m.latency = latency;
    return m;
}

}  // namespace

EvaluationReport evaluate(const GroundTruth& gt, const Run& run,
                          double iou_threshold, std::size_t thread_count) {
    EvaluationReport report;
    report.run_name = run.name;
    report.iou_threshold = iou_threshold;
    if (thread_count == 0) thread_count = 1;
    report.thread_count = thread_count;
    report.num_scenes = gt.scenes.size();

    std::unordered_map<std::string, const RunScene*> run_by_id;
    for (const RunScene& rs : run.scenes) {
        run_by_id[rs.scene_id] = &rs;
    }
    std::unordered_map<std::string, bool> gt_ids;
    for (const Scene& s : gt.scenes) {
        gt_ids[s.id] = true;
    }
    for (const RunScene& rs : run.scenes) {
        if (gt_ids.find(rs.scene_id) == gt_ids.end()) {
            throw IoError("run '" + run.name +
                          "' references unknown scene id: '" + rs.scene_id +
                          "'");
        }
    }

    // Evaluate scenes in parallel.
    std::vector<std::future<SceneEval>> futures;
    futures.reserve(gt.scenes.size());
    {
        ThreadPool pool(thread_count);
        for (const Scene& s : gt.scenes) {
            const RunScene* rs = nullptr;
            auto it = run_by_id.find(s.id);
            if (it != run_by_id.end()) rs = it->second;
            futures.push_back(pool.enqueue(evaluate_scene, std::cref(s), rs,
                                           iou_threshold));
        }
        std::vector<SceneEval> evals;
        evals.reserve(futures.size());
        for (auto& f : futures) {
            evals.push_back(f.get());
        }
        // Merge in sorted scene-id order for determinism.
        std::sort(evals.begin(), evals.end(),
                  [](const SceneEval& a, const SceneEval& b) {
                      return a.scene_id < b.scene_id;
                  });

        std::map<GroupKey, GroupData> groups;
        std::map<std::string, std::vector<double>> latency_by_tag;
        for (const SceneEval& ev : evals) {
            report.num_frames += ev.latency_by_tag.count(kAnyTag)
                                     ? ev.latency_by_tag.at(kAnyTag).size()
                                     : 0;
            for (const auto& kv : ev.groups) {
                groups[kv.first].merge(kv.second);
            }
            for (const auto& kv : ev.latency_by_tag) {
                std::vector<double>& dst = latency_by_tag[kv.first];
                dst.insert(dst.end(), kv.second.begin(), kv.second.end());
            }
        }

        // Latency per tag + overall.
        for (const auto& kv : latency_by_tag) {
            report.latency_by_tag[kv.first] = compute_latency_stats(kv.second);
        }
        report.latency = report.latency_by_tag.count(kAnyTag)
                             ? report.latency_by_tag[kAnyTag]
                             : LatencyStats{};

        // Collect the set of real tags present.
        std::vector<std::string> real_tags;
        for (const auto& kv : report.latency_by_tag) {
            if (kv.first != kAnyTag) real_tags.push_back(kv.first);
        }

        // Helper to fetch a group pointer or nullptr.
        auto group_ptr = [&groups](const GroupKey& k) -> const GroupData* {
            auto it = groups.find(k);
            return it == groups.end() ? nullptr : &it->second;
        };

        const LatencyStats& any_lat = report.latency;

        // Overall + per-type / per-range / per-difficulty aggregates use the
        // "(any)" tag groups (each object counted once).
        {
            std::vector<const GroupData*> all;
            for (ObjectType t : all_types()) {
                for (RangeBin rb : all_range_bins()) {
                    if (const GroupData* g = group_ptr({t, rb, kAnyTag})) {
                        all.push_back(g);
                    }
                }
            }
            report.overall = metrics_from_groups(all, Difficulty::Level1, true,
                                                 any_lat);
        }
        for (ObjectType t : all_types()) {
            std::vector<const GroupData*> gs;
            for (RangeBin rb : all_range_bins()) {
                if (const GroupData* g = group_ptr({t, rb, kAnyTag})) {
                    gs.push_back(g);
                }
            }
            report.per_type[t] =
                metrics_from_groups(gs, Difficulty::Level1, true, any_lat);
        }
        for (ObjectType t : all_types()) {
            for (RangeBin rb : all_range_bins()) {
                const GroupData* g = group_ptr({t, rb, kAnyTag});
                if (g == nullptr) continue;
                report.per_type_range[{t, rb}] = metrics_from_groups(
                    {g}, Difficulty::Level1, true, any_lat);
            }
        }
        for (ObjectType t : all_types()) {
            for (Difficulty diff : all_difficulties()) {
                std::vector<const GroupData*> gs;
                for (RangeBin rb : all_range_bins()) {
                    if (const GroupData* g = group_ptr({t, rb, kAnyTag})) {
                        gs.push_back(g);
                    }
                }
                SliceMetrics m =
                    metrics_from_groups(gs, diff, false, any_lat);
                if (m.num_gt > 0) {
                    report.per_type_difficulty[{t, diff}] = m;
                }
            }
        }

        // Full slice cross product over the real tags.
        for (const std::string& tag : real_tags) {
            const LatencyStats tag_lat = report.latency_by_tag[tag];
            for (ObjectType t : all_types()) {
                for (RangeBin rb : all_range_bins()) {
                    const GroupData* g = group_ptr({t, rb, tag});
                    if (g == nullptr) continue;
                    for (Difficulty diff : all_difficulties()) {
                        SliceMetrics m =
                            metrics_from_groups({g}, diff, false, tag_lat);
                        if (m.num_gt <= 0) continue;  // no support; skip
                        SliceKey key{t, rb, diff, tag};
                        report.slices[key] = m;
                    }
                }
            }
        }
    }  // pool joins here

    return report;
}

}  // namespace pev
