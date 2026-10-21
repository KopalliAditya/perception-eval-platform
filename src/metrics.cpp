#include "metrics.hpp"

#include <algorithm>
#include <cmath>

namespace pev {

double Counts::precision() const {
    const long denom = tp + fp;
    return denom > 0 ? static_cast<double>(tp) / static_cast<double>(denom)
                     : 0.0;
}

double Counts::recall() const {
    const long denom = tp + fn;
    return denom > 0 ? static_cast<double>(tp) / static_cast<double>(denom)
                     : 0.0;
}

double Counts::f1() const {
    const double p = precision();
    const double r = recall();
    const double denom = p + r;
    return denom > 0.0 ? (2.0 * p * r) / denom : 0.0;
}

double percentile(std::vector<double> samples, double p) {
    if (samples.empty()) {
        return 0.0;
    }
    std::sort(samples.begin(), samples.end());
    if (p <= 0.0) {
        return samples.front();
    }
    if (p >= 100.0) {
        return samples.back();
    }
    const double n = static_cast<double>(samples.size());
    long rank = static_cast<long>(std::ceil((p / 100.0) * n));
    if (rank < 1) rank = 1;
    if (rank > static_cast<long>(samples.size())) {
        rank = static_cast<long>(samples.size());
    }
    return samples[static_cast<std::size_t>(rank - 1)];
}

LatencyStats compute_latency_stats(std::vector<double> samples) {
    LatencyStats stats;
    stats.count = static_cast<long>(samples.size());
    if (samples.empty()) {
        return stats;
    }
    double sum = 0.0;
    for (double v : samples) {
        sum += v;
    }
    stats.mean = sum / static_cast<double>(samples.size());
    stats.p50 = percentile(samples, 50.0);
    stats.p95 = percentile(samples, 95.0);
    return stats;
}

namespace {

// Area under a precision–recall curve given parallel arrays of recall (strictly
// non-decreasing) and precision, using the interpolated (monotonic-to-the-
// right) envelope and integrating from recall 0 to the maximum achieved recall.
double integrate_pr(const std::vector<double>& recall,
                    std::vector<double> precision) {
    const std::size_t n = recall.size();
    if (n == 0) {
        return 0.0;
    }
    // Interpolated precision: p'[i] = max_{j >= i} precision[j].
    for (std::size_t i = n - 1; i-- > 0;) {
        precision[i] = std::max(precision[i], precision[i + 1]);
    }
    double area = 0.0;
    double prev_recall = 0.0;
    for (std::size_t i = 0; i < n; ++i) {
        const double dr = recall[i] - prev_recall;
        if (dr > 0.0) {
            area += dr * precision[i];
        }
        prev_recall = recall[i];
    }
    return area;
}

}  // namespace

ApResult average_precision(std::vector<DetEntry> entries, long num_gt) {
    ApResult out;
    out.num_gt = num_gt;

    // Sort by descending score; break ties by placing true positives first so
    // the curve is reproducible across runs and thread counts.
    std::sort(entries.begin(), entries.end(),
              [](const DetEntry& a, const DetEntry& b) {
                  if (a.score != b.score) return a.score > b.score;
                  return a.is_tp && !b.is_tp;
              });

    long raw_tp = 0;
    long raw_fp = 0;
    double w_tp = 0.0;  // heading-weighted true positives (for APH)

    std::vector<double> rec_ap, prec_ap;
    std::vector<double> rec_aph, prec_aph;
    rec_ap.reserve(entries.size());
    prec_ap.reserve(entries.size());
    rec_aph.reserve(entries.size());
    prec_aph.reserve(entries.size());

    for (const DetEntry& e : entries) {
        if (e.is_tp) {
            ++raw_tp;
            w_tp += e.heading_acc;
        } else {
            ++raw_fp;
        }
        const double processed = static_cast<double>(raw_tp + raw_fp);

        if (num_gt > 0) {
            const double gt = static_cast<double>(num_gt);
            rec_ap.push_back(static_cast<double>(raw_tp) / gt);
            prec_ap.push_back(static_cast<double>(raw_tp) / processed);
            rec_aph.push_back(w_tp / gt);
            prec_aph.push_back(w_tp / processed);
        }
    }

    out.num_tp = raw_tp;
    out.num_fp = raw_fp;
    if (num_gt > 0) {
        out.ap = integrate_pr(rec_ap, prec_ap);
        out.aph = integrate_pr(rec_aph, prec_aph);
    }
    return out;
}

}  // namespace pev
