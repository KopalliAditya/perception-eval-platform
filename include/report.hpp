// report.hpp — rendering of the detection-evaluation, tracking-evaluation and
// slice-triage reports to console, Markdown, JSON, and a self-contained HTML
// dashboard (inline CSS/SVG only; no external resources).
//
// Author: Aditya Kopalli, 2026. MIT licensed.

#ifndef PEV_REPORT_HPP
#define PEV_REPORT_HPP

#include <string>

#include "evaluator.hpp"
#include "json.hpp"
#include "tracking.hpp"
#include "triage.hpp"

namespace pev {

// --- Detection evaluation ---------------------------------------------------

std::string render_console(const EvaluationReport& report);
std::string render_markdown(const EvaluationReport& report);
json::Json to_json(const EvaluationReport& report);
std::string render_eval_html(const EvaluationReport& report);

// --- Tracking evaluation ----------------------------------------------------

std::string render_console(const TrackingReport& report);
std::string render_markdown(const TrackingReport& report);
json::Json to_json(const TrackingReport& report);

// --- Slice triage / regression comparison -----------------------------------

std::string render_console(const TriageReport& report);
std::string render_markdown(const TriageReport& report);
json::Json to_json(const TriageReport& report);

// The self-contained HTML dashboard is rendered from the compare JSON so that
// the `dashboard` subcommand can regenerate it from a saved report file.
std::string render_compare_html_from_json(const json::Json& compare_json);

}  // namespace pev

#endif  // PEV_REPORT_HPP
