#include "triage.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>

namespace pev {

namespace {

double neg(double x) { return x < 0.0 ? -x : 0.0; }

double lat_frac_increase(double base, double cand) {
    if (cand <= base) return 0.0;
    return (cand - base) / std::max(base, 1e-6);
}

std::string fmt(double v, int dec) {
    char buf[64];
    std::snprintf(buf, sizeof(buf), "%+.*f", dec, v);
    return std::string(buf);
}

}  // namespace

TriageReport triage(const EvaluationReport& baseline,
                    const EvaluationReport& candidate,
                    const TriageConfig& config) {
    TriageReport report;
    report.config = config;
    report.baseline = baseline;
    report.candidate = candidate;

    for (const auto& kv : baseline.slices) {
        const SliceKey& key = kv.first;
        const SliceMetrics& b = kv.second;
        if (b.num_gt < config.min_support) {
            continue;
        }
        auto it = candidate.slices.find(key);
        if (it == candidate.slices.end()) {
            continue;  // slice absent in candidate; nothing to compare
        }
        const SliceMetrics& c = it->second;

        SliceDelta d;
        d.key = key;
        d.support = b.num_gt;
        d.ap_base = b.ap;
        d.ap_cand = c.ap;
        d.d_ap = c.ap - b.ap;
        d.aph_base = b.aph;
        d.aph_cand = c.aph;
        d.d_aph = c.aph - b.aph;
        d.recall_base = b.counts.recall();
        d.recall_cand = c.counts.recall();
        d.d_recall = d.recall_cand - d.recall_base;
        d.precision_base = b.counts.precision();
        d.precision_cand = c.counts.precision();
        d.d_precision = d.precision_cand - d.precision_base;
        d.lat_base = b.latency.p95;
        d.lat_cand = c.latency.p95;
        d.d_lat = d.lat_cand - d.lat_base;

        // Severity: recall and heading regressions weighted most, scaled by the
        // log of the slice support so well-supported slices rank above noise.
        const double det_sev = 0.45 * neg(d.d_recall) + 0.30 * neg(d.d_aph) +
                               0.15 * neg(d.d_ap) + 0.10 * neg(d.d_precision);
        const double lat_sev =
            0.40 * lat_frac_increase(d.lat_base, d.lat_cand);
        const double support_w =
            std::log1p(static_cast<double>(d.support)) / std::log(10.0);
        d.severity = (det_sev + lat_sev) * support_w;

        const double mt = config.metric_tolerance;
        const bool metric_reg = (d.d_recall <= -mt) || (d.d_aph <= -mt) ||
                                (d.d_ap <= -mt) || (d.d_precision <= -mt);
        const bool lat_reg =
            (d.lat_cand > d.lat_base * (1.0 + config.latency_tolerance)) &&
            (d.lat_cand - d.lat_base > 0.5);
        d.is_regression = metric_reg || lat_reg;

        report.deltas.push_back(d);
    }

    // Deterministic ordering: severity desc, then slice label for ties.
    std::sort(report.deltas.begin(), report.deltas.end(),
              [](const SliceDelta& a, const SliceDelta& b) {
                  if (a.severity != b.severity) return a.severity > b.severity;
                  return slice_label(a.key) < slice_label(b.key);
              });

    for (const SliceDelta& d : report.deltas) {
        if (d.is_regression) {
            report.regressions.push_back(d);
        }
    }
    return report;
}

std::string finding_line(const SliceDelta& d) {
    std::string out = slice_label(d.key) + ": ";
    out += "recall " + fmt(d.d_recall, 3);
    out += ", APH " + fmt(d.d_aph, 3);
    out += ", AP " + fmt(d.d_ap, 3);
    out += ", prec " + fmt(d.d_precision, 3);
    if (std::fabs(d.d_lat) >= 0.05) {
        char buf[64];
        std::snprintf(buf, sizeof(buf), ", p95 %+.1fms", d.d_lat);
        out += buf;
    }
    out += " (n=" + std::to_string(d.support) + ")";
    return out;
}

}  // namespace pev
