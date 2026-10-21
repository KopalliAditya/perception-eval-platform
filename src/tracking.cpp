#include "tracking.hpp"

#include <algorithm>
#include <future>
#include <map>
#include <unordered_map>
#include <vector>

#include "assignment.hpp"
#include "geometry.hpp"
#include "io_util.hpp"
#include "thread_pool.hpp"

namespace pev {

double TrackingCounts::mota() const {
    if (total_gt <= 0) {
        return 0.0;
    }
    const double errors =
        static_cast<double>(misses + false_positives + id_switches);
    return 1.0 - errors / static_cast<double>(total_gt);
}

double TrackingCounts::motp() const {
    return matches > 0 ? sum_iou / static_cast<double>(matches) : 0.0;
}

void TrackingCounts::add(const TrackingCounts& o) {
    total_gt += o.total_gt;
    matches += o.matches;
    misses += o.misses;
    false_positives += o.false_positives;
    id_switches += o.id_switches;
    fragmentations += o.fragmentations;
    sum_iou += o.sum_iou;
    num_gt_tracks += o.num_gt_tracks;
    mostly_tracked += o.mostly_tracked;
    partially_tracked += o.partially_tracked;
    mostly_lost += o.mostly_lost;
}

namespace {

// Per ground-truth track bookkeeping within a single scene.
struct TrackState {
    long present = 0;       // frames the GT track appears in
    long matched = 0;       // frames it was matched to a detection
    int last_hyp = -1;      // last matched hypothesis (detection) track id
    bool ever_matched = false;
    bool currently_gap = false;  // in a miss gap after having been matched
};

// Evaluate tracking for one object type within one scene, walking frames in
// order. Track IDs are scene-local so all per-track stats resolve here.
TrackingCounts eval_type_in_scene(const Scene& scene,
                                  const RunScene* run_scene, ObjectType type,
                                  double iou_threshold) {
    TrackingCounts tc;

    std::unordered_map<int, const RunFrame*> run_frames;
    if (run_scene != nullptr) {
        for (const RunFrame& rf : run_scene->frames) {
            run_frames[rf.index] = &rf;
        }
    }

    std::map<int, TrackState> tracks;  // gt track id -> state (ordered)

    for (const GtFrame& frame : scene.frames) {
        std::vector<const GtObject*> gts;
        for (const GtObject& o : frame.objects) {
            if (o.type == type) gts.push_back(&o);
        }
        std::vector<const Detection*> dets;
        auto it = run_frames.find(frame.index);
        if (it != run_frames.end()) {
            for (const Detection& d : it->second->detections) {
                if (d.type == type) dets.push_back(&d);
            }
        }

        const std::size_t g = gts.size();
        const std::size_t d = dets.size();
        tc.total_gt += static_cast<long>(g);

        // Mark presence for every GT track in this frame.
        for (const GtObject* o : gts) {
            tracks[o->track_id].present += 1;
        }

        // Build a square cost matrix: cost = -IoU for valid (>= threshold)
        // pairs, 0 otherwise (dummies / infeasible). Minimising the total cost
        // maximises the IoU-weighted matching; sub-threshold pairs are dropped
        // afterwards.
        std::vector<char> gt_matched(g, 0);
        if (g > 0 && d > 0) {
            const std::size_t n = std::max(g, d);
            std::vector<std::vector<double>> cost(
                n, std::vector<double>(n, 0.0));
            std::vector<std::vector<double>> iou(
                g, std::vector<double>(d, 0.0));
            for (std::size_t i = 0; i < g; ++i) {
                for (std::size_t j = 0; j < d; ++j) {
                    const double v = iou_3d(dets[j]->box, gts[i]->box);
                    iou[i][j] = v;
                    cost[i][j] = (v >= iou_threshold) ? -v : 0.0;
                }
            }
            const std::vector<int> assign = solve_assignment(cost);
            for (std::size_t i = 0; i < g; ++i) {
                const int j = assign[i];
                if (j < 0 || static_cast<std::size_t>(j) >= d) {
                    continue;
                }
                const double v = iou[i][static_cast<std::size_t>(j)];
                if (v < iou_threshold) {
                    continue;  // infeasible pair that got assigned; drop it
                }
                // Accepted match.
                gt_matched[i] = 1;
                tc.matches += 1;
                tc.sum_iou += v;
                const int gid = gts[i]->track_id;
                const int hid = dets[static_cast<std::size_t>(j)]->track_id;
                TrackState& st = tracks[gid];
                if (st.ever_matched && st.last_hyp != hid) {
                    tc.id_switches += 1;
                }
                // A resumed gap counts as a fragmentation.
                if (st.currently_gap) {
                    tc.fragmentations += 1;
                    st.currently_gap = false;
                }
                st.last_hyp = hid;
                st.ever_matched = true;
                st.matched += 1;
            }
        }

        // Unmatched GT in this frame become misses; if a track had already been
        // matched it enters a gap (which, once resumed, is a fragmentation).
        long frame_matches = 0;
        for (std::size_t i = 0; i < g; ++i) {
            if (gt_matched[i]) {
                ++frame_matches;
            } else {
                tc.misses += 1;
                TrackState& st = tracks[gts[i]->track_id];
                if (st.ever_matched) {
                    st.currently_gap = true;
                }
            }
        }
        // Unmatched detections are false positives.
        tc.false_positives += static_cast<long>(d) - frame_matches;
    }

    // Track-level summaries.
    for (const auto& kv : tracks) {
        const TrackState& st = kv.second;
        if (st.present <= 0) {
            continue;
        }
        tc.num_gt_tracks += 1;
        const double cover =
            static_cast<double>(st.matched) / static_cast<double>(st.present);
        if (cover >= 0.8) {
            tc.mostly_tracked += 1;
        } else if (cover <= 0.2) {
            tc.mostly_lost += 1;
        } else {
            tc.partially_tracked += 1;
        }
    }

    return tc;
}

// All object types for one scene, returned as a per-type map.
std::map<ObjectType, TrackingCounts> eval_scene(const Scene& scene,
                                                const RunScene* run_scene,
                                                double iou_threshold) {
    std::map<ObjectType, TrackingCounts> out;
    for (ObjectType type : all_types()) {
        out[type] = eval_type_in_scene(scene, run_scene, type, iou_threshold);
    }
    return out;
}

}  // namespace

TrackingReport evaluate_tracking(const GroundTruth& gt, const Run& run,
                                 const TrackingConfig& config,
                                 std::size_t thread_count) {
    TrackingReport report;
    report.run_name = run.name;
    report.iou_threshold = config.iou_threshold;
    if (thread_count == 0) thread_count = 1;
    report.thread_count = thread_count;

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

    std::vector<std::future<std::map<ObjectType, TrackingCounts>>> futures;
    futures.reserve(gt.scenes.size());
    {
        ThreadPool pool(thread_count);
        for (const Scene& s : gt.scenes) {
            const RunScene* rs = nullptr;
            auto it = run_by_id.find(s.id);
            if (it != run_by_id.end()) rs = it->second;
            futures.push_back(pool.enqueue(eval_scene, std::cref(s), rs,
                                           config.iou_threshold));
        }
        for (auto& f : futures) {
            const std::map<ObjectType, TrackingCounts> part = f.get();
            for (const auto& kv : part) {
                report.per_type[kv.first].add(kv.second);
            }
        }
    }

    for (const auto& kv : report.per_type) {
        report.total.add(kv.second);
    }
    return report;
}

}  // namespace pev
