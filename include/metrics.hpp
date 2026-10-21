// metrics.hpp — detection count bookkeeping, precision / recall / F1, latency
// percentiles, and the Average-Precision / Average-Precision-weighted-by-
// Heading (AP / APH) integrators.
//
// Count metrics:
//   precision = TP / (TP + FP)      (0 when there are no predictions)
//   recall    = TP / (TP + FN)      (0 when there is no ground truth)
//   F1        = 2PR / (P + R)       (0 when P + R == 0)
//
// AP integrates the precision–recall curve obtained by sweeping the score
// threshold over all detections. APH is the same integration except each true
// positive contributes its heading accuracy in [0, 1] rather than a full 1,
// which is the signature Waymo metric. See DESIGN.md for the worked formula.
//
// Author: Aditya Kopalli, 2026. MIT licensed.

#ifndef PEV_METRICS_HPP
#define PEV_METRICS_HPP

#include <vector>

namespace pev {

// True-positive / false-positive / false-negative tallies.
struct Counts {
    long tp = 0;
    long fp = 0;
    long fn = 0;

    void add(const Counts& other) {
        tp += other.tp;
        fp += other.fp;
        fn += other.fn;
    }

    double precision() const;
    double recall() const;
    double f1() const;
};

// Latency summary over a set of per-frame samples (milliseconds).
struct LatencyStats {
    long count = 0;
    double mean = 0.0;
    double p50 = 0.0;
    double p95 = 0.0;
};

// Nearest-rank percentile of `samples` for p in [0, 100]. Returns 0 for an
// empty input. The input is copied and sorted internally so callers need not
// pre-sort; the computation is therefore order-independent (deterministic).
double percentile(std::vector<double> samples, double p);

// Mean / p50 / p95 over `samples` (milliseconds). Input copied and sorted.
LatencyStats compute_latency_stats(std::vector<double> samples);

// --- Average Precision / APH ------------------------------------------------

// One scored detection labelled as a true or false positive. For true
// positives `heading_acc` carries the heading accuracy in [0, 1]; it is ignored
// for false positives.
struct DetEntry {
    double score = 0.0;
    bool is_tp = false;
    double heading_acc = 0.0;
};

// Result of an AP / APH integration over one set of detection entries.
struct ApResult {
    double ap = 0.0;    // area under the precision–recall curve
    double aph = 0.0;   // heading-weighted AP (<= ap)
    long num_gt = 0;    // number of ground-truth positives (recall denom)
    long num_tp = 0;    // raw true-positive count (at any score)
    long num_fp = 0;    // raw false-positive count (at any score)

    // Reference counts at score threshold 0 (i.e. using every detection).
    Counts counts() const { return Counts{num_tp, num_fp, num_gt - num_tp}; }
};

// Integrate AP and APH over `entries` given `num_gt` ground-truth positives.
// `entries` is taken by value and sorted internally (deterministic).
ApResult average_precision(std::vector<DetEntry> entries, long num_gt);

}  // namespace pev

#endif  // PEV_METRICS_HPP
