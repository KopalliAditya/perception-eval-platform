// matching.hpp — greedy highest-IoU association of detections to ground truth,
// performed independently per object type within a single frame.
//
// Within a type the detections are visited in descending confidence order
// (ties broken by input index for determinism) and each detection claims the
// still-available ground-truth box with the highest 3D IoU that meets the
// threshold. This is the standard association used to turn a set of scored
// detections into the per-detection TP/FP labels that the AP / APH integrator
// consumes.
//
// Author: Aditya Kopalli, 2026. MIT licensed.

#ifndef PEV_MATCHING_HPP
#define PEV_MATCHING_HPP

#include <cstddef>
#include <vector>

#include "dataset.hpp"

namespace pev {

// A matched (detection, ground-truth) pair, the 3D IoU that linked them and the
// heading accuracy in [0, 1] of the detection against that ground-truth box.
struct Match {
    std::size_t det_index = 0;  // index into the type-filtered detections
    std::size_t gt_index = 0;   // index into the type-filtered ground truth
    double iou = 0.0;
    double heading_acc = 0.0;
};

// Result of matching one type's worth of detections to ground truth.
struct ClassMatchResult {
    std::vector<Match> true_positives;
    std::vector<std::size_t> false_positive_dets;  // unmatched detections
    std::vector<std::size_t> false_negative_gts;   // unmatched ground truth

    long tp() const { return static_cast<long>(true_positives.size()); }
    long fp() const { return static_cast<long>(false_positive_dets.size()); }
    long fn() const { return static_cast<long>(false_negative_gts.size()); }
};

// Greedy highest-IoU matching for a single type. `dets` and `gts` must each
// already be filtered to the SAME type. The result's indices refer to the
// positions within those filtered vectors. Uses 3D IoU of oriented boxes.
ClassMatchResult match_class(const std::vector<Detection>& dets,
                             const std::vector<GtObject>& gts,
                             double iou_threshold);

}  // namespace pev

#endif  // PEV_MATCHING_HPP
