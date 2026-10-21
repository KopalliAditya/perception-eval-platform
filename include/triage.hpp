// triage.hpp — slice-based regression triage, the platform's headline feature.
//
// Given a baseline and a candidate EvaluationReport, this computes per-slice
// metric deltas (AP, APH, recall, precision and p95 latency) across every slice
// = {type} x {range bin} x {difficulty} x {scene tag}, assigns each a severity
// score (recall and heading regressions weighted most, scaled by the slice's
// ground-truth support so tiny slices do not dominate), ranks them, and surfaces
// the worst-regressing slices as prioritised, human-readable findings.
//
// Author: Aditya Kopalli, 2026. MIT licensed.

#ifndef PEV_TRIAGE_HPP
#define PEV_TRIAGE_HPP

#include <cstddef>
#include <string>
#include <vector>

#include "evaluator.hpp"

namespace pev {

struct TriageConfig {
    double metric_tolerance = 0.03;   // absolute AP/APH/recall/prec drop to flag
    double latency_tolerance = 0.15;  // fractional p95 latency rise to flag
    long min_support = 8;             // minimum baseline GT count for a slice
    std::size_t top_k = 15;           // worst-slice findings to surface
};

// Per-slice comparison of baseline vs candidate.
struct SliceDelta {
    SliceKey key;
    long support = 0;  // baseline ground-truth count in the slice

    double ap_base = 0.0, ap_cand = 0.0, d_ap = 0.0;
    double aph_base = 0.0, aph_cand = 0.0, d_aph = 0.0;
    double recall_base = 0.0, recall_cand = 0.0, d_recall = 0.0;
    double precision_base = 0.0, precision_cand = 0.0, d_precision = 0.0;
    double lat_base = 0.0, lat_cand = 0.0, d_lat = 0.0;

    double severity = 0.0;
    bool is_regression = false;
};

struct TriageReport {
    TriageConfig config;
    EvaluationReport baseline;
    EvaluationReport candidate;
    std::vector<SliceDelta> deltas;       // every eligible slice, severity desc
    std::vector<SliceDelta> regressions;  // flagged subset, severity desc

    bool has_regressions() const { return !regressions.empty(); }
};

TriageReport triage(const EvaluationReport& baseline,
                    const EvaluationReport& candidate,
                    const TriageConfig& config);

// One prioritised, human-readable finding line, e.g.
// "TYPE_PEDESTRIAN @ [30,50)m, LEVEL_2, low_visibility: recall -0.14, "
// "APH -0.11, AP -0.09, p95 +14.2ms (n=37)".
std::string finding_line(const SliceDelta& d);

}  // namespace pev

#endif  // PEV_TRIAGE_HPP
