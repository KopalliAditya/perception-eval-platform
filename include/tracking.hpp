// tracking.hpp — multi-object tracking evaluation (CLEAR-MOT style).
//
// Given ground-truth tracks and a run's detections, both carrying track IDs,
// this computes the standard multi-object-tracking metrics per object type and
// overall:
//
//   MOTA  = 1 - (misses + false_positives + id_switches) / total_gt
//   MOTP  = mean 3D IoU over all matched pairs
//   IDSW  = identity switches (a GT track re-associated to a different hyp ID)
//   FRAG  = fragmentations (a GT track's coverage is interrupted then resumed)
//   MT/ML = mostly-tracked / mostly-lost GT tracks (>= 80% / <= 20% covered)
//
// Per-frame association uses the Hungarian algorithm over 3D IoU, keeping only
// pairs at or above the IoU threshold. Track IDs are scene-local; metrics are
// computed per scene and summed, which makes the result independent of thread
// scheduling.
//
// Author: Aditya Kopalli, 2026. MIT licensed.

#ifndef PEV_TRACKING_HPP
#define PEV_TRACKING_HPP

#include <cstddef>
#include <map>
#include <string>

#include "dataset.hpp"
#include "taxonomy.hpp"

namespace pev {

struct TrackingConfig {
    double iou_threshold = 0.5;
};

struct TrackingCounts {
    long total_gt = 0;          // GT instances summed over all frames
    long matches = 0;           // matched (GT, hyp) pairs over all frames
    long misses = 0;            // unmatched GT instances (false negatives)
    long false_positives = 0;   // unmatched detections
    long id_switches = 0;
    long fragmentations = 0;
    double sum_iou = 0.0;       // sum of IoU over matched pairs (for MOTP)

    long num_gt_tracks = 0;
    long mostly_tracked = 0;    // >= 80% of life covered
    long partially_tracked = 0;
    long mostly_lost = 0;       // <= 20% of life covered

    double mota() const;
    double motp() const;

    void add(const TrackingCounts& o);
};

struct TrackingReport {
    std::string run_name;
    double iou_threshold = 0.5;
    std::size_t thread_count = 1;
    std::map<ObjectType, TrackingCounts> per_type;
    TrackingCounts total;
};

TrackingReport evaluate_tracking(const GroundTruth& gt, const Run& run,
                                 const TrackingConfig& config,
                                 std::size_t thread_count);

}  // namespace pev

#endif  // PEV_TRACKING_HPP
