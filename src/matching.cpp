#include "matching.hpp"

#include <algorithm>
#include <numeric>

#include "geometry.hpp"

namespace pev {

ClassMatchResult match_class(const std::vector<Detection>& dets,
                             const std::vector<GtObject>& gts,
                             double iou_threshold) {
    ClassMatchResult result;

    // Visit detections in descending score order. Ties are broken by the
    // original index so the outcome is fully deterministic.
    std::vector<std::size_t> order(dets.size());
    std::iota(order.begin(), order.end(), std::size_t{0});
    std::sort(order.begin(), order.end(),
              [&dets](std::size_t a, std::size_t b) {
                  if (dets[a].score != dets[b].score) {
                      return dets[a].score > dets[b].score;
                  }
                  return a < b;
              });

    std::vector<bool> gt_used(gts.size(), false);

    for (std::size_t det_idx : order) {
        double best_iou = iou_threshold;  // must meet or exceed the threshold
        bool found = false;
        std::size_t best_gt = 0;

        for (std::size_t g = 0; g < gts.size(); ++g) {
            if (gt_used[g]) {
                continue;
            }
            const double score = iou_3d(dets[det_idx].box, gts[g].box);
            // Strictly-greater keeps the lowest gt index on ties (determinism).
            if (score >= best_iou && (!found || score > best_iou)) {
                best_iou = score;
                best_gt = g;
                found = true;
            }
        }

        if (found) {
            gt_used[best_gt] = true;
            const double hacc = heading_accuracy(dets[det_idx].box.heading,
                                                 gts[best_gt].box.heading);
            result.true_positives.push_back(
                Match{det_idx, best_gt, best_iou, hacc});
        } else {
            result.false_positive_dets.push_back(det_idx);
        }
    }

    for (std::size_t g = 0; g < gts.size(); ++g) {
        if (!gt_used[g]) {
            result.false_negative_gts.push_back(g);
        }
    }

    return result;
}

}  // namespace pev
