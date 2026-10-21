// evaluator.hpp — turns a ground-truth dataset + a model run into a structured
// detection-evaluation report with AP / APH computed for the full slice cross
// product {type} x {range bin} x {difficulty} x {scene tag}, plus the
// tag-independent aggregates (per type, per type x range, per type x
// difficulty and overall).
//
// Determinism: scenes are evaluated independently in a thread pool; their
// contributions are merged in sorted scene-id order and every downstream
// computation (AP integration over score-sorted detections, integer count
// sums, nearest-rank latency percentiles) is order-independent, so the report
// is identical for any thread count.
//
// Author: Aditya Kopalli, 2026. MIT licensed.

#ifndef PEV_EVALUATOR_HPP
#define PEV_EVALUATOR_HPP

#include <cstddef>
#include <map>
#include <string>
#include <utility>

#include "dataset.hpp"
#include "metrics.hpp"
#include "taxonomy.hpp"

namespace pev {

// Identifies one slice of the evaluation.
struct SliceKey {
    ObjectType type = ObjectType::Vehicle;
    RangeBin range = RangeBin::Near;
    Difficulty difficulty = Difficulty::Level1;
    std::string tag;  // a scene tag (real tags only in the slice map)

    bool operator<(const SliceKey& o) const;
    bool operator==(const SliceKey& o) const;
};

// Human-readable slice label, e.g.
// "TYPE_PEDESTRIAN @ [30,50)m, LEVEL_2, low_visibility".
std::string slice_label(const SliceKey& key);

// Metrics for a slice (or aggregate).
struct SliceMetrics {
    long num_gt = 0;   // ground-truth positives (recall denominator)
    double ap = 0.0;
    double aph = 0.0;
    Counts counts;         // TP/FP/FN at the reference IoU (all detections)
    LatencyStats latency;  // latency of the owning scene-tag (if applicable)
};

// The full detection evaluation of one run.
struct EvaluationReport {
    std::string run_name;
    double iou_threshold = 0.5;
    std::size_t thread_count = 1;
    std::size_t num_scenes = 0;
    std::size_t num_frames = 0;

    SliceMetrics overall;
    std::map<ObjectType, SliceMetrics> per_type;
    std::map<std::pair<ObjectType, RangeBin>, SliceMetrics> per_type_range;
    std::map<std::pair<ObjectType, Difficulty>, SliceMetrics>
        per_type_difficulty;

    std::map<SliceKey, SliceMetrics> slices;        // full cross product
    std::map<std::string, LatencyStats> latency_by_tag;
    LatencyStats latency;  // over every frame in the run
};

// Evaluate `run` against `gt` at the given IoU threshold using `thread_count`
// workers. Throws IoError if the run references an unknown scene id.
EvaluationReport evaluate(const GroundTruth& gt, const Run& run,
                          double iou_threshold, std::size_t thread_count);

}  // namespace pev

#endif  // PEV_EVALUATOR_HPP
