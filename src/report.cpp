#include "report.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <sstream>
#include <string>
#include <vector>

namespace pev {

using json::Json;

namespace {

// --- small formatting helpers ----------------------------------------------

std::string fixed(double value, int decimals) {
    char buf[64];
    std::snprintf(buf, sizeof(buf), "%.*f", decimals, value);
    return std::string(buf);
}

std::string signed_fixed(double value, int decimals) {
    char buf[64];
    std::snprintf(buf, sizeof(buf), "%+.*f", decimals, value);
    return std::string(buf);
}

std::string pad_right(const std::string& s, std::size_t width) {
    if (s.size() >= width) return s;
    return s + std::string(width - s.size(), ' ');
}

std::string pad_left(const std::string& s, std::size_t width) {
    if (s.size() >= width) return s;
    return std::string(width - s.size(), ' ') + s;
}

std::string rule(std::size_t width) { return std::string(width, '-'); }

std::string html_escape(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (char c : s) {
        switch (c) {
            case '&': out += "&amp;"; break;
            case '<': out += "&lt;"; break;
            case '>': out += "&gt;"; break;
            case '"': out += "&quot;"; break;
            default: out.push_back(c);
        }
    }
    return out;
}

// A light background tint for a "higher is better" delta. Negative deltas tint
// red, positive deltas tint green; magnitude scaled by `scale`.
std::string heat_css(double delta, double scale) {
    double t = delta / scale;
    if (t > 1.0) t = 1.0;
    if (t < -1.0) t = -1.0;
    int r, g, b;
    if (t >= 0.0) {
        const double k = t;
        r = static_cast<int>(235 - 70 * k);
        g = 250;
        b = static_cast<int>(235 - 70 * k);
    } else {
        const double k = -t;
        r = 250;
        g = static_cast<int>(235 - 95 * k);
        b = static_cast<int>(235 - 95 * k);
    }
    char buf[32];
    std::snprintf(buf, sizeof(buf), "rgb(%d,%d,%d)", r, g, b);
    return std::string(buf);
}

// --- JSON helpers -----------------------------------------------------------

Json latency_to_json(const LatencyStats& l) {
    Json j = Json::object();
    j["count"] = l.count;
    j["mean_ms"] = l.mean;
    j["p50_ms"] = l.p50;
    j["p95_ms"] = l.p95;
    return j;
}

Json slice_metrics_to_json(const SliceMetrics& m) {
    Json j = Json::object();
    j["num_gt"] = m.num_gt;
    j["ap"] = m.ap;
    j["aph"] = m.aph;
    j["tp"] = m.counts.tp;
    j["fp"] = m.counts.fp;
    j["fn"] = m.counts.fn;
    j["precision"] = m.counts.precision();
    j["recall"] = m.counts.recall();
    j["f1"] = m.counts.f1();
    j["latency"] = latency_to_json(m.latency);
    return j;
}

}  // namespace

// ===========================================================================
// Detection evaluation
// ===========================================================================

std::string render_console(const EvaluationReport& report) {
    std::ostringstream os;
    os << "============================================================\n";
    os << "Detection evaluation: run '" << report.run_name << "'\n";
    os << "IoU threshold: " << fixed(report.iou_threshold, 2)
       << "   threads: " << report.thread_count
       << "   scenes: " << report.num_scenes
       << "   frames: " << report.num_frames << "\n";
    os << "============================================================\n\n";

    const SliceMetrics& o = report.overall;
    os << "OVERALL\n";
    os << "  AP " << fixed(o.ap, 3) << "   APH " << fixed(o.aph, 3)
       << "   precision " << fixed(o.counts.precision(), 3) << "   recall "
       << fixed(o.counts.recall(), 3) << "   F1 " << fixed(o.counts.f1(), 3)
       << "\n";
    os << "  TP " << o.counts.tp << "  FP " << o.counts.fp << "  FN "
       << o.counts.fn << "   (num_gt " << o.num_gt << ")\n";
    os << "  latency (ms): mean " << fixed(report.latency.mean, 2) << "  p50 "
       << fixed(report.latency.p50, 2) << "  p95 "
       << fixed(report.latency.p95, 2) << "  (" << report.latency.count
       << " frames)\n\n";

    const std::size_t wt = 18, wn = 8, wm = 8;
    os << "AP / APH BY TYPE\n";
    os << pad_right("type", wt) << pad_left("numGT", wn) << pad_left("AP", wm)
       << pad_left("APH", wm) << pad_left("prec", wm) << pad_left("recall", wm)
       << pad_left("F1", wm) << "\n";
    os << rule(wt + wn + wm * 5) << "\n";
    for (ObjectType t : all_types()) {
        auto it = report.per_type.find(t);
        if (it == report.per_type.end()) continue;
        const SliceMetrics& m = it->second;
        os << pad_right(to_string(t), wt)
           << pad_left(std::to_string(m.num_gt), wn)
           << pad_left(fixed(m.ap, 3), wm) << pad_left(fixed(m.aph, 3), wm)
           << pad_left(fixed(m.counts.precision(), 3), wm)
           << pad_left(fixed(m.counts.recall(), 3), wm)
           << pad_left(fixed(m.counts.f1(), 3), wm) << "\n";
    }
    os << "\n";

    os << "AP / APH BY TYPE x RANGE\n";
    os << pad_right("type", wt) << pad_right("range", 12)
       << pad_left("numGT", wn) << pad_left("AP", wm) << pad_left("APH", wm)
       << pad_left("recall", wm) << "\n";
    os << rule(wt + 12 + wn + wm * 3) << "\n";
    for (ObjectType t : all_types()) {
        for (RangeBin rb : all_range_bins()) {
            auto it = report.per_type_range.find({t, rb});
            if (it == report.per_type_range.end()) continue;
            const SliceMetrics& m = it->second;
            os << pad_right(to_string(t), wt) << pad_right(to_string(rb), 12)
               << pad_left(std::to_string(m.num_gt), wn)
               << pad_left(fixed(m.ap, 3), wm) << pad_left(fixed(m.aph, 3), wm)
               << pad_left(fixed(m.counts.recall(), 3), wm) << "\n";
        }
    }
    os << "\n";

    os << "AP / APH BY TYPE x DIFFICULTY\n";
    os << pad_right("type", wt) << pad_right("difficulty", 12)
       << pad_left("numGT", wn) << pad_left("AP", wm) << pad_left("APH", wm)
       << pad_left("recall", wm) << "\n";
    os << rule(wt + 12 + wn + wm * 3) << "\n";
    for (ObjectType t : all_types()) {
        for (Difficulty diff : all_difficulties()) {
            auto it = report.per_type_difficulty.find({t, diff});
            if (it == report.per_type_difficulty.end()) continue;
            const SliceMetrics& m = it->second;
            os << pad_right(to_string(t), wt)
               << pad_right(to_string(diff), 12)
               << pad_left(std::to_string(m.num_gt), wn)
               << pad_left(fixed(m.ap, 3), wm) << pad_left(fixed(m.aph, 3), wm)
               << pad_left(fixed(m.counts.recall(), 3), wm) << "\n";
        }
    }
    os << "\n";

    os << "LATENCY BY SCENE TAG\n";
    os << pad_right("tag", 20) << pad_left("frames", 10) << pad_left("mean", 10)
       << pad_left("p50", 10) << pad_left("p95", 10) << "\n";
    os << rule(20 + 10 * 4) << "\n";
    for (const auto& kv : report.latency_by_tag) {
        os << pad_right(kv.first, 20)
           << pad_left(std::to_string(kv.second.count), 10)
           << pad_left(fixed(kv.second.mean, 2), 10)
           << pad_left(fixed(kv.second.p50, 2), 10)
           << pad_left(fixed(kv.second.p95, 2), 10) << "\n";
    }
    return os.str();
}

namespace {

void md_type_table(std::ostringstream& os, const EvaluationReport& report) {
    os << "| type | numGT | AP | APH | precision | recall | F1 |\n";
    os << "|---|---:|---:|---:|---:|---:|---:|\n";
    for (ObjectType t : all_types()) {
        auto it = report.per_type.find(t);
        if (it == report.per_type.end()) continue;
        const SliceMetrics& m = it->second;
        os << "| " << to_string(t) << " | " << m.num_gt << " | "
           << fixed(m.ap, 3) << " | " << fixed(m.aph, 3) << " | "
           << fixed(m.counts.precision(), 3) << " | "
           << fixed(m.counts.recall(), 3) << " | " << fixed(m.counts.f1(), 3)
           << " |\n";
    }
}

}  // namespace

std::string render_markdown(const EvaluationReport& report) {
    std::ostringstream os;
    os << "# Detection evaluation: `" << report.run_name << "`\n\n";
    os << "- IoU threshold: **" << fixed(report.iou_threshold, 2) << "**\n";
    os << "- Threads: **" << report.thread_count << "**\n";
    os << "- Scenes / frames: **" << report.num_scenes << "** / **"
       << report.num_frames << "**\n\n";

    const SliceMetrics& o = report.overall;
    os << "## Overall\n\n";
    os << "| metric | value |\n|---|---:|\n";
    os << "| AP | " << fixed(o.ap, 3) << " |\n";
    os << "| APH | " << fixed(o.aph, 3) << " |\n";
    os << "| precision | " << fixed(o.counts.precision(), 3) << " |\n";
    os << "| recall | " << fixed(o.counts.recall(), 3) << " |\n";
    os << "| F1 | " << fixed(o.counts.f1(), 3) << " |\n";
    os << "| TP / FP / FN | " << o.counts.tp << " / " << o.counts.fp << " / "
       << o.counts.fn << " |\n";
    os << "| latency p95 (ms) | " << fixed(report.latency.p95, 2) << " |\n\n";

    os << "## AP / APH by type\n\n";
    md_type_table(os, report);
    os << "\n";

    os << "## AP / APH by type x range\n\n";
    os << "| type | range | numGT | AP | APH | recall |\n";
    os << "|---|---|---:|---:|---:|---:|\n";
    for (ObjectType t : all_types()) {
        for (RangeBin rb : all_range_bins()) {
            auto it = report.per_type_range.find({t, rb});
            if (it == report.per_type_range.end()) continue;
            const SliceMetrics& m = it->second;
            os << "| " << to_string(t) << " | " << to_string(rb) << " | "
               << m.num_gt << " | " << fixed(m.ap, 3) << " | "
               << fixed(m.aph, 3) << " | " << fixed(m.counts.recall(), 3)
               << " |\n";
        }
    }
    os << "\n";

    os << "## AP / APH by type x difficulty\n\n";
    os << "| type | difficulty | numGT | AP | APH | recall |\n";
    os << "|---|---|---:|---:|---:|---:|\n";
    for (ObjectType t : all_types()) {
        for (Difficulty diff : all_difficulties()) {
            auto it = report.per_type_difficulty.find({t, diff});
            if (it == report.per_type_difficulty.end()) continue;
            const SliceMetrics& m = it->second;
            os << "| " << to_string(t) << " | " << to_string(diff) << " | "
               << m.num_gt << " | " << fixed(m.ap, 3) << " | "
               << fixed(m.aph, 3) << " | " << fixed(m.counts.recall(), 3)
               << " |\n";
        }
    }
    os << "\n";

    os << "## Latency by scene tag\n\n";
    os << "| tag | frames | mean | p50 | p95 |\n";
    os << "|---|---:|---:|---:|---:|\n";
    for (const auto& kv : report.latency_by_tag) {
        os << "| " << kv.first << " | " << kv.second.count << " | "
           << fixed(kv.second.mean, 2) << " | " << fixed(kv.second.p50, 2)
           << " | " << fixed(kv.second.p95, 2) << " |\n";
    }
    return os.str();
}

json::Json to_json(const EvaluationReport& report) {
    Json root = Json::object();
    root["schema"] = "pev.evaluation.v1";
    root["run"] = report.run_name;
    root["iou_threshold"] = report.iou_threshold;
    root["threads"] = report.thread_count;
    root["num_scenes"] = report.num_scenes;
    root["num_frames"] = report.num_frames;

    root["overall"] = slice_metrics_to_json(report.overall);
    root["latency"] = latency_to_json(report.latency);

    Json per_type = Json::object();
    for (ObjectType t : all_types()) {
        auto it = report.per_type.find(t);
        if (it != report.per_type.end()) {
            per_type[to_string(t)] = slice_metrics_to_json(it->second);
        }
    }
    root["per_type"] = per_type;

    Json ptr = Json::array();
    for (const auto& kv : report.per_type_range) {
        Json j = slice_metrics_to_json(kv.second);
        j["type"] = to_string(kv.first.first);
        j["range"] = to_string(kv.first.second);
        ptr.push_back(j);
    }
    root["per_type_range"] = ptr;

    Json ptd = Json::array();
    for (const auto& kv : report.per_type_difficulty) {
        Json j = slice_metrics_to_json(kv.second);
        j["type"] = to_string(kv.first.first);
        j["difficulty"] = to_string(kv.first.second);
        ptd.push_back(j);
    }
    root["per_type_difficulty"] = ptd;

    Json slices = Json::array();
    for (const auto& kv : report.slices) {
        Json j = slice_metrics_to_json(kv.second);
        j["type"] = to_string(kv.first.type);
        j["range"] = to_string(kv.first.range);
        j["difficulty"] = to_string(kv.first.difficulty);
        j["tag"] = kv.first.tag;
        j["label"] = slice_label(kv.first);
        slices.push_back(j);
    }
    root["slices"] = slices;

    Json lbt = Json::object();
    for (const auto& kv : report.latency_by_tag) {
        lbt[kv.first] = latency_to_json(kv.second);
    }
    root["latency_by_tag"] = lbt;
    return root;
}

std::string render_eval_html(const EvaluationReport& report) {
    std::ostringstream os;
    os << "<!DOCTYPE html>\n<html lang=\"en\">\n<head>\n<meta charset=\"utf-8\">"
          "\n<meta name=\"viewport\" content=\"width=device-width, "
          "initial-scale=1\">\n<title>pev evaluation: "
       << html_escape(report.run_name) << "</title>\n<style>\n"
       << "body{font-family:-apple-system,Segoe UI,Roboto,Helvetica,Arial,"
          "sans-serif;margin:0;background:#0f1420;color:#e6e9ef}"
       << ".wrap{max-width:1000px;margin:0 auto;padding:28px}"
       << "h1{font-size:22px;margin:0 0 4px}h2{font-size:16px;margin:28px 0 8px;"
          "color:#9fb0c3}"
       << ".sub{color:#8795a8;font-size:13px;margin-bottom:8px}"
       << ".cards{display:flex;gap:12px;flex-wrap:wrap;margin:12px 0}"
       << ".card{background:#1a2234;border:1px solid #2a3550;border-radius:10px;"
          "padding:14px 18px;min-width:120px}"
       << ".card .k{font-size:12px;color:#8795a8}.card .v{font-size:24px;"
          "font-weight:600}"
       << "table{border-collapse:collapse;width:100%;font-size:13px;"
          "margin-top:6px}"
       << "th,td{padding:6px 10px;text-align:right;border-bottom:1px solid "
          "#263048}th{color:#9fb0c3;text-align:right}"
       << "td:first-child,th:first-child{text-align:left}"
       << "tr:hover td{background:#19233a}"
       << ".tag{color:#8795a8}"
       << "</style>\n</head>\n<body>\n<div class=\"wrap\">\n";
    os << "<h1>Detection evaluation &mdash; " << html_escape(report.run_name)
       << "</h1>\n";
    os << "<div class=\"sub\">Synthetic WOD-shaped dataset &middot; IoU "
       << fixed(report.iou_threshold, 2) << " &middot; " << report.num_scenes
       << " scenes &middot; " << report.num_frames << " frames &middot; "
       << report.thread_count << " threads</div>\n";

    const SliceMetrics& o = report.overall;
    os << "<div class=\"cards\">";
    auto card = [&os](const std::string& k, const std::string& v) {
        os << "<div class=\"card\"><div class=\"k\">" << k
           << "</div><div class=\"v\">" << v << "</div></div>";
    };
    card("AP", fixed(o.ap, 3));
    card("APH", fixed(o.aph, 3));
    card("Precision", fixed(o.counts.precision(), 3));
    card("Recall", fixed(o.counts.recall(), 3));
    card("F1", fixed(o.counts.f1(), 3));
    card("p95 ms", fixed(report.latency.p95, 1));
    os << "</div>\n";

    os << "<h2>AP / APH by type</h2>\n<table><tr><th>type</th><th>numGT</th>"
          "<th>AP</th><th>APH</th><th>prec</th><th>recall</th><th>F1</th></tr>\n";
    for (ObjectType t : all_types()) {
        auto it = report.per_type.find(t);
        if (it == report.per_type.end()) continue;
        const SliceMetrics& m = it->second;
        os << "<tr><td>" << to_string(t) << "</td><td>" << m.num_gt
           << "</td><td>" << fixed(m.ap, 3) << "</td><td>" << fixed(m.aph, 3)
           << "</td><td>" << fixed(m.counts.precision(), 3) << "</td><td>"
           << fixed(m.counts.recall(), 3) << "</td><td>"
           << fixed(m.counts.f1(), 3) << "</td></tr>\n";
    }
    os << "</table>\n";

    os << "<h2>AP / APH by type &times; range</h2>\n<table><tr><th>type</th>"
          "<th>range</th><th>numGT</th><th>AP</th><th>APH</th><th>recall</th>"
          "</tr>\n";
    for (ObjectType t : all_types()) {
        for (RangeBin rb : all_range_bins()) {
            auto it = report.per_type_range.find({t, rb});
            if (it == report.per_type_range.end()) continue;
            const SliceMetrics& m = it->second;
            os << "<tr><td>" << to_string(t) << "</td><td class=\"tag\">"
               << to_string(rb) << "</td><td>" << m.num_gt << "</td><td>"
               << fixed(m.ap, 3) << "</td><td>" << fixed(m.aph, 3)
               << "</td><td>" << fixed(m.counts.recall(), 3) << "</td></tr>\n";
        }
    }
    os << "</table>\n";

    os << "<p class=\"sub\">Bundled data is synthetic and WOD-shaped (not the "
          "real Waymo Open Dataset). Metrics and code are real.</p>\n";
    os << "</div>\n</body>\n</html>\n";
    return os.str();
}

// ===========================================================================
// Tracking evaluation
// ===========================================================================

namespace {

void tracking_row(std::ostringstream& os, const std::string& label,
                  const TrackingCounts& c) {
    os << pad_right(label, 18) << pad_left(fixed(c.mota(), 3), 9)
       << pad_left(fixed(c.motp(), 3), 9)
       << pad_left(std::to_string(c.id_switches), 7)
       << pad_left(std::to_string(c.fragmentations), 7)
       << pad_left(std::to_string(c.mostly_tracked), 6)
       << pad_left(std::to_string(c.partially_tracked), 6)
       << pad_left(std::to_string(c.mostly_lost), 6)
       << pad_left(std::to_string(c.num_gt_tracks), 10)
       << pad_left(std::to_string(c.matches), 9)
       << pad_left(std::to_string(c.misses), 8)
       << pad_left(std::to_string(c.false_positives), 8) << "\n";
}

Json tracking_counts_to_json(const TrackingCounts& c) {
    Json j = Json::object();
    j["mota"] = c.mota();
    j["motp"] = c.motp();
    j["total_gt"] = c.total_gt;
    j["matches"] = c.matches;
    j["misses"] = c.misses;
    j["false_positives"] = c.false_positives;
    j["id_switches"] = c.id_switches;
    j["fragmentations"] = c.fragmentations;
    j["num_gt_tracks"] = c.num_gt_tracks;
    j["mostly_tracked"] = c.mostly_tracked;
    j["partially_tracked"] = c.partially_tracked;
    j["mostly_lost"] = c.mostly_lost;
    return j;
}

}  // namespace

std::string render_console(const TrackingReport& report) {
    std::ostringstream os;
    os << "============================================================\n";
    os << "Tracking evaluation: run '" << report.run_name << "'\n";
    os << "IoU threshold: " << fixed(report.iou_threshold, 2)
       << "   threads: " << report.thread_count << "\n";
    os << "============================================================\n\n";

    os << pad_right("type", 18) << pad_left("MOTA", 9) << pad_left("MOTP", 9)
       << pad_left("IDSW", 7) << pad_left("FRAG", 7) << pad_left("MT", 6)
       << pad_left("PT", 6) << pad_left("ML", 6) << pad_left("GTtracks", 10)
       << pad_left("match", 9) << pad_left("miss", 8) << pad_left("fp", 8)
       << "\n";
    os << rule(18 + 9 * 2 + 7 * 2 + 6 * 3 + 10 + 9 + 8 * 2) << "\n";
    for (ObjectType t : all_types()) {
        auto it = report.per_type.find(t);
        if (it == report.per_type.end()) continue;
        tracking_row(os, to_string(t), it->second);
    }
    os << rule(18 + 9 * 2 + 7 * 2 + 6 * 3 + 10 + 9 + 8 * 2) << "\n";
    tracking_row(os, "ALL", report.total);
    os << "\nMOTA = 1 - (miss + fp + IDSW) / total_gt;  MOTP = mean IoU over "
          "matches.\n";
    os << "MT/PT/ML = mostly / partially / mostly-lost GT tracks (>=80% / in "
          "between / <=20% covered).\n";
    return os.str();
}

std::string render_markdown(const TrackingReport& report) {
    std::ostringstream os;
    os << "# Tracking evaluation: `" << report.run_name << "`\n\n";
    os << "- IoU threshold: **" << fixed(report.iou_threshold, 2) << "**\n";
    os << "- Threads: **" << report.thread_count << "**\n\n";
    os << "| type | MOTA | MOTP | IDSW | FRAG | MT | PT | ML | GT tracks | "
          "matches | misses | FP |\n";
    os << "|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|\n";
    auto row = [&os](const std::string& label, const TrackingCounts& c) {
        os << "| " << label << " | " << fixed(c.mota(), 3) << " | "
           << fixed(c.motp(), 3) << " | " << c.id_switches << " | "
           << c.fragmentations << " | " << c.mostly_tracked << " | "
           << c.partially_tracked << " | " << c.mostly_lost << " | "
           << c.num_gt_tracks << " | " << c.matches << " | " << c.misses
           << " | " << c.false_positives << " |\n";
    };
    for (ObjectType t : all_types()) {
        auto it = report.per_type.find(t);
        if (it == report.per_type.end()) continue;
        row(to_string(t), it->second);
    }
    row("**ALL**", report.total);
    return os.str();
}

json::Json to_json(const TrackingReport& report) {
    Json root = Json::object();
    root["schema"] = "pev.tracking.v1";
    root["run"] = report.run_name;
    root["iou_threshold"] = report.iou_threshold;
    root["threads"] = report.thread_count;
    Json per_type = Json::object();
    for (ObjectType t : all_types()) {
        auto it = report.per_type.find(t);
        if (it != report.per_type.end()) {
            per_type[to_string(t)] = tracking_counts_to_json(it->second);
        }
    }
    root["per_type"] = per_type;
    root["total"] = tracking_counts_to_json(report.total);
    return root;
}

// ===========================================================================
// Slice triage / regression comparison
// ===========================================================================

std::string render_console(const TriageReport& report) {
    std::ostringstream os;
    const EvaluationReport& b = report.baseline;
    const EvaluationReport& c = report.candidate;

    os << "============================================================\n";
    os << "Slice regression triage: baseline '" << b.run_name
       << "' vs candidate '" << c.run_name << "'\n";
    os << "IoU: " << fixed(b.iou_threshold, 2) << "   metric-tol: "
       << fixed(report.config.metric_tolerance, 3)
       << "   latency-tol: "
       << fixed(report.config.latency_tolerance * 100.0, 0)
       << "%   min-support: " << report.config.min_support << "\n";
    os << "============================================================\n\n";

    const SliceMetrics& bo = b.overall;
    const SliceMetrics& co = c.overall;
    os << "AGGREGATE DELTA (candidate - baseline)\n";
    os << "  AP        " << fixed(bo.ap, 3) << " -> " << fixed(co.ap, 3)
       << "   (" << signed_fixed(co.ap - bo.ap, 3) << ")\n";
    os << "  APH       " << fixed(bo.aph, 3) << " -> " << fixed(co.aph, 3)
       << "   (" << signed_fixed(co.aph - bo.aph, 3) << ")\n";
    os << "  recall    " << fixed(bo.counts.recall(), 3) << " -> "
       << fixed(co.counts.recall(), 3) << "   ("
       << signed_fixed(co.counts.recall() - bo.counts.recall(), 3) << ")\n";
    os << "  precision " << fixed(bo.counts.precision(), 3) << " -> "
       << fixed(co.counts.precision(), 3) << "   ("
       << signed_fixed(co.counts.precision() - bo.counts.precision(), 3)
       << ")\n";
    os << "  p95 ms    " << fixed(b.latency.p95, 2) << " -> "
       << fixed(c.latency.p95, 2) << "   ("
       << signed_fixed(c.latency.p95 - b.latency.p95, 2) << ")\n\n";

    const std::size_t shown =
        std::min(report.regressions.size(), report.config.top_k);
    os << "TOP REGRESSING SLICES (" << shown << " of "
       << report.regressions.size() << " flagged; "
       << report.deltas.size() << " slices compared)\n";
    if (report.regressions.empty()) {
        os << "  none - candidate is within tolerance across all slices.\n";
    } else {
        for (std::size_t i = 0; i < shown; ++i) {
            char num[8];
            std::snprintf(num, sizeof(num), "%2zu. ", i + 1);
            os << "  " << num << finding_line(report.regressions[i]) << "\n";
        }
    }
    return os.str();
}

namespace {

Json delta_triple(double base, double cand) {
    Json j = Json::object();
    j["baseline"] = base;
    j["candidate"] = cand;
    j["delta"] = cand - base;
    return j;
}

Json slice_delta_to_json(const SliceDelta& d) {
    Json j = Json::object();
    j["label"] = slice_label(d.key);
    j["type"] = to_string(d.key.type);
    j["range"] = to_string(d.key.range);
    j["difficulty"] = to_string(d.key.difficulty);
    j["tag"] = d.key.tag;
    j["support"] = d.support;
    j["severity"] = d.severity;
    j["is_regression"] = d.is_regression;
    j["ap"] = delta_triple(d.ap_base, d.ap_cand);
    j["aph"] = delta_triple(d.aph_base, d.aph_cand);
    j["recall"] = delta_triple(d.recall_base, d.recall_cand);
    j["precision"] = delta_triple(d.precision_base, d.precision_cand);
    j["latency_p95"] = delta_triple(d.lat_base, d.lat_cand);
    return j;
}

}  // namespace

std::string render_markdown(const TriageReport& report) {
    std::ostringstream os;
    const EvaluationReport& b = report.baseline;
    const EvaluationReport& c = report.candidate;
    os << "# Slice regression triage\n\n";
    os << "Baseline **`" << b.run_name << "`** vs candidate **`" << c.run_name
       << "`**\n\n";
    os << "- IoU threshold: **" << fixed(b.iou_threshold, 2) << "**\n";
    os << "- Metric tolerance: **" << fixed(report.config.metric_tolerance, 3)
       << "**, latency tolerance: **"
       << fixed(report.config.latency_tolerance * 100.0, 0) << "%**\n";
    os << "- Slices compared: **" << report.deltas.size()
       << "**, regressions flagged: **" << report.regressions.size()
       << "**\n\n";

    const SliceMetrics& bo = b.overall;
    const SliceMetrics& co = c.overall;
    os << "## Aggregate\n\n";
    os << "| metric | baseline | candidate | delta |\n|---|---:|---:|---:|\n";
    os << "| AP | " << fixed(bo.ap, 3) << " | " << fixed(co.ap, 3) << " | "
       << signed_fixed(co.ap - bo.ap, 3) << " |\n";
    os << "| APH | " << fixed(bo.aph, 3) << " | " << fixed(co.aph, 3) << " | "
       << signed_fixed(co.aph - bo.aph, 3) << " |\n";
    os << "| recall | " << fixed(bo.counts.recall(), 3) << " | "
       << fixed(co.counts.recall(), 3) << " | "
       << signed_fixed(co.counts.recall() - bo.counts.recall(), 3) << " |\n";
    os << "| precision | " << fixed(bo.counts.precision(), 3) << " | "
       << fixed(co.counts.precision(), 3) << " | "
       << signed_fixed(co.counts.precision() - bo.counts.precision(), 3)
       << " |\n";
    os << "| p95 latency (ms) | " << fixed(b.latency.p95, 2) << " | "
       << fixed(c.latency.p95, 2) << " | "
       << signed_fixed(c.latency.p95 - b.latency.p95, 2) << " |\n\n";

    os << "## Top regressing slices\n\n";
    if (report.regressions.empty()) {
        os << "_None — candidate is within tolerance across all slices._\n\n";
    } else {
        os << "| # | slice | n | dRecall | dAPH | dAP | dPrec | dp95 ms |\n";
        os << "|---:|---|---:|---:|---:|---:|---:|---:|\n";
        const std::size_t shown =
            std::min(report.regressions.size(), report.config.top_k);
        for (std::size_t i = 0; i < shown; ++i) {
            const SliceDelta& d = report.regressions[i];
            os << "| " << (i + 1) << " | " << slice_label(d.key) << " | "
               << d.support << " | " << signed_fixed(d.d_recall, 3) << " | "
               << signed_fixed(d.d_aph, 3) << " | " << signed_fixed(d.d_ap, 3)
               << " | " << signed_fixed(d.d_precision, 3) << " | "
               << signed_fixed(d.d_lat, 1) << " |\n";
        }
    }
    return os.str();
}

json::Json to_json(const TriageReport& report) {
    const EvaluationReport& b = report.baseline;
    const EvaluationReport& c = report.candidate;
    Json root = Json::object();
    root["schema"] = "pev.compare.v1";
    root["baseline_run"] = b.run_name;
    root["candidate_run"] = c.run_name;
    root["iou_threshold"] = b.iou_threshold;

    Json cfg = Json::object();
    cfg["metric_tolerance"] = report.config.metric_tolerance;
    cfg["latency_tolerance"] = report.config.latency_tolerance;
    cfg["min_support"] = report.config.min_support;
    cfg["top_k"] = static_cast<long long>(report.config.top_k);
    root["config"] = cfg;

    const SliceMetrics& bo = b.overall;
    const SliceMetrics& co = c.overall;
    Json agg = Json::object();
    agg["ap"] = delta_triple(bo.ap, co.ap);
    agg["aph"] = delta_triple(bo.aph, co.aph);
    agg["recall"] = delta_triple(bo.counts.recall(), co.counts.recall());
    agg["precision"] =
        delta_triple(bo.counts.precision(), co.counts.precision());
    agg["p95_ms"] = delta_triple(b.latency.p95, c.latency.p95);
    root["aggregate"] = agg;

    root["regressions_found"] =
        static_cast<long long>(report.regressions.size());
    root["slices_compared"] = static_cast<long long>(report.deltas.size());

    Json findings = Json::array();
    for (const SliceDelta& d : report.deltas) {
        findings.push_back(slice_delta_to_json(d));
    }
    root["findings"] = findings;
    return root;
}

// ===========================================================================
// Self-contained HTML dashboard (rendered from the compare JSON)
// ===========================================================================

namespace {

double jnum(const Json& j, const std::string& key) {
    return j.contains(key) ? j.at(key).as_number() : 0.0;
}

std::string jstr(const Json& j, const std::string& key) {
    return j.contains(key) ? j.at(key).as_string() : std::string();
}

void agg_card(std::ostringstream& os, const std::string& name, const Json& trip,
              int dec, bool higher_better) {
    const double base = jnum(trip, "baseline");
    const double cand = jnum(trip, "candidate");
    const double delta = jnum(trip, "delta");
    const double signed_for_heat = higher_better ? delta : -delta;
    os << "<div class=\"card\"><div class=\"k\">" << name
       << "</div><div class=\"v\">" << fixed(cand, dec)
       << "</div><div class=\"d\" style=\"color:"
       << (signed_for_heat < -1e-9 ? "#ff6b6b"
                                   : (signed_for_heat > 1e-9 ? "#51cf66"
                                                             : "#8795a8"))
       << "\">" << signed_fixed(delta, dec) << " vs " << fixed(base, dec)
       << "</div></div>";
}

void heat_cell(std::ostringstream& os, double delta, double scale, int dec,
               bool higher_better) {
    const double heat_delta = higher_better ? delta : -delta;
    os << "<td style=\"background:" << heat_css(heat_delta, scale)
       << ";color:#111\">" << signed_fixed(delta, dec) << "</td>";
}

}  // namespace

std::string render_compare_html_from_json(const json::Json& j) {
    std::ostringstream os;
    const std::string base_run = jstr(j, "baseline_run");
    const std::string cand_run = jstr(j, "candidate_run");
    const double iou = jnum(j, "iou_threshold");
    const long long reg_found =
        j.contains("regressions_found")
            ? j.at("regressions_found").as_int()
            : 0;
    const long long compared =
        j.contains("slices_compared") ? j.at("slices_compared").as_int() : 0;

    os << "<!DOCTYPE html>\n<html lang=\"en\">\n<head>\n<meta charset=\"utf-8\">"
          "\n<meta name=\"viewport\" content=\"width=device-width, "
          "initial-scale=1\">\n<title>pev regression dashboard</title>\n"
          "<style>\n"
       << "body{font-family:-apple-system,Segoe UI,Roboto,Helvetica,Arial,"
          "sans-serif;margin:0;background:#0f1420;color:#e6e9ef}"
       << ".wrap{max-width:1180px;margin:0 auto;padding:28px}"
       << "h1{font-size:23px;margin:0 0 4px}h2{font-size:16px;margin:26px 0 8px;"
          "color:#9fb0c3}"
       << ".sub{color:#8795a8;font-size:13px}"
       << ".banner{margin:14px 0;padding:12px 16px;border-radius:10px;"
          "font-weight:600}"
       << ".bad{background:#3a1d22;border:1px solid #6b2b33;color:#ffb3b3}"
       << ".ok{background:#15301f;border:1px solid #2c5e3e;color:#a9e8c0}"
       << ".cards{display:flex;gap:12px;flex-wrap:wrap;margin:12px 0}"
       << ".card{background:#1a2234;border:1px solid #2a3550;border-radius:10px;"
          "padding:12px 16px;min-width:120px}"
       << ".card .k{font-size:12px;color:#8795a8}.card .v{font-size:23px;"
          "font-weight:600}.card .d{font-size:12px;margin-top:2px}"
       << "table{border-collapse:collapse;width:100%;font-size:12.5px;"
          "margin-top:6px}"
       << "th,td{padding:5px 9px;text-align:right;border-bottom:1px solid "
          "#20283c}th{color:#9fb0c3;position:sticky;top:0;background:#0f1420}"
       << "td.l,th.l{text-align:left}"
       << ".pill{display:inline-block;padding:1px 7px;border-radius:9px;"
          "font-size:11px;background:#3a1d22;color:#ffb3b3}"
       << ".pill.ok{background:#15301f;color:#a9e8c0}"
       << "</style>\n</head>\n<body>\n<div class=\"wrap\">\n";

    os << "<h1>Perception regression dashboard</h1>\n";
    os << "<div class=\"sub\">baseline <b>" << html_escape(base_run)
       << "</b> vs candidate <b>" << html_escape(cand_run)
       << "</b> &middot; IoU " << fixed(iou, 2)
       << " &middot; synthetic WOD-shaped data (not the real Waymo Open "
          "Dataset)</div>\n";

    if (reg_found > 0) {
        os << "<div class=\"banner bad\">" << reg_found
           << " regressing slice(s) detected out of " << compared
           << " compared.</div>\n";
    } else {
        os << "<div class=\"banner ok\">No regressions: candidate is within "
              "tolerance across all "
           << compared << " slices.</div>\n";
    }

    if (j.contains("aggregate")) {
        const Json& agg = j.at("aggregate");
        os << "<h2>Aggregate</h2>\n<div class=\"cards\">";
        if (agg.contains("ap")) agg_card(os, "AP", agg.at("ap"), 3, true);
        if (agg.contains("aph")) agg_card(os, "APH", agg.at("aph"), 3, true);
        if (agg.contains("recall"))
            agg_card(os, "Recall", agg.at("recall"), 3, true);
        if (agg.contains("precision"))
            agg_card(os, "Precision", agg.at("precision"), 3, true);
        if (agg.contains("p95_ms"))
            agg_card(os, "p95 ms", agg.at("p95_ms"), 1, false);
        os << "</div>\n";
    }

    // Per-slice heatmap table over all compared slices.
    os << "<h2>Per-slice heatmap (red = regression, green = improvement)</h2>\n";
    os << "<table><tr><th class=\"l\">slice</th><th>n</th><th>dRecall</th>"
          "<th>dAPH</th><th>dAP</th><th>dPrec</th><th>dp95 ms</th>"
          "<th>flag</th></tr>\n";
    if (j.contains("findings")) {
        for (const Json& f : j.at("findings").as_array()) {
            const std::string label = jstr(f, "label");
            const long long n =
                f.contains("support") ? f.at("support").as_int() : 0;
            const double d_rec = jnum(f.at("recall"), "delta");
            const double d_aph = jnum(f.at("aph"), "delta");
            const double d_ap = jnum(f.at("ap"), "delta");
            const double d_prec = jnum(f.at("precision"), "delta");
            const double d_lat = jnum(f.at("latency_p95"), "delta");
            const bool reg =
                f.contains("is_regression") && f.at("is_regression").as_bool();
            os << "<tr><td class=\"l\">" << html_escape(label) << "</td><td>"
               << n << "</td>";
            heat_cell(os, d_rec, 0.15, 3, true);
            heat_cell(os, d_aph, 0.15, 3, true);
            heat_cell(os, d_ap, 0.15, 3, true);
            heat_cell(os, d_prec, 0.15, 3, true);
            heat_cell(os, d_lat, 8.0, 1, false);
            os << "<td><span class=\"pill" << (reg ? "" : " ok") << "\">"
               << (reg ? "FLAG" : "ok") << "</span></td></tr>\n";
        }
    }
    os << "</table>\n";

    os << "<p class=\"sub\" style=\"margin-top:22px\">Generated by <code>pev"
          "</code> &middot; the metrics and code are real; the dataset is "
          "synthetic and WOD-shaped. Running on the real Waymo Open Dataset "
          "requires downloading it and running "
          "<code>tools/wod_to_platform.py</code> on a Linux/GPU machine.</p>\n";
    os << "</div>\n</body>\n</html>\n";
    return os.str();
}

}  // namespace pev
