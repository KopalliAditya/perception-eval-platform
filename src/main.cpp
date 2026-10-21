// main.cpp — the `pev` command-line interface for the AV Perception Evaluation
// Platform.
//
// Subcommands:
//   pev generate  --out DIR [--scenes N] [--frames-per-scene F] [--seed S]
//   pev evaluate  --data DIR --run NAME [--iou T] [--threads K] [--reports DIR]
//   pev track     --data DIR --run NAME [--iou T] [--threads K] [--reports DIR]
//   pev compare   --data DIR --baseline NAME --candidate NAME
//                 [--iou T] [--threads K] [--metric-tol V] [--latency-tol V]
//                 [--min-support N] [--top-k K] [--reports DIR]
//                 [--fail-on-regression]
//   pev dashboard --report FILE.json --out FILE.html
//
// Exit codes: 0 success, 2 usage error, 1 runtime error, 3 regressions found
// (only with --fail-on-regression).
//
// Author: Aditya Kopalli, 2026. MIT licensed.

#include <cstdlib>
#include <exception>
#include <iostream>
#include <map>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

#include "dataset.hpp"
#include "evaluator.hpp"
#include "generator.hpp"
#include "io_util.hpp"
#include "report.hpp"
#include "thread_pool.hpp"
#include "tracking.hpp"
#include "triage.hpp"

namespace {

constexpr int kExitOk = 0;
constexpr int kExitRuntime = 1;
constexpr int kExitUsage = 2;
constexpr int kExitRegression = 3;

void print_usage(std::ostream& os) {
    os <<
        "pev — AV Perception Evaluation Platform (Waymo-Open-Dataset-aligned)\n"
        "\n"
        "NOTE: the bundled dataset is SYNTHETIC and WOD-shaped (same taxonomy,\n"
        "difficulty levels, range bins and 3D oriented boxes), not the real\n"
        "Waymo Open Dataset. The metrics and code are real. See README.md.\n"
        "\n"
        "USAGE\n"
        "  pev <command> [options]\n"
        "\n"
        "COMMANDS\n"
        "  generate   Create a synthetic WOD-shaped dataset (ground truth +\n"
        "             baseline + candidate runs with injected regressions).\n"
        "  evaluate   Detection evaluation: AP / APH per type / range /\n"
        "             difficulty + overall; writes json/md/html.\n"
        "  track      Multi-object tracking evaluation: MOTA / MOTP / IDSW /\n"
        "             FRAG / MT / ML.\n"
        "  compare    Slice-based regression triage between two runs; writes\n"
        "             json/md + a self-contained HTML dashboard.\n"
        "  dashboard  Re-render the HTML dashboard from a saved compare JSON.\n"
        "  help       Show this help (also: -h, --help).\n"
        "\n"
        "generate --out DIR [--scenes N] [--frames-per-scene F] [--seed S]\n"
        "  --out DIR            Output data directory (required).\n"
        "  --scenes N          Number of scenes/segments (default 40).\n"
        "  --frames-per-scene F  Frames per scene (default 8).\n"
        "  --seed S            RNG seed for reproducibility (default 42).\n"
        "\n"
        "evaluate --data DIR --run NAME [--iou T] [--threads K] [--reports D]\n"
        "  --data DIR          Dataset directory (required).\n"
        "  --run NAME          Run name, e.g. baseline (required).\n"
        "  --iou T             3D IoU match threshold in [0,1] (default 0.5).\n"
        "  --threads K         Worker threads (default: hardware concurrency).\n"
        "  --reports D         Where to write reports (default reports).\n"
        "\n"
        "track --data DIR --run NAME [--iou T] [--threads K] [--reports D]\n"
        "  (same options as evaluate)\n"
        "\n"
        "compare --data DIR --baseline NAME --candidate NAME [options]\n"
        "  --data DIR          Dataset directory (required).\n"
        "  --baseline NAME     Baseline run name (required).\n"
        "  --candidate NAME    Candidate run name (required).\n"
        "  --iou T             3D IoU match threshold (default 0.5).\n"
        "  --threads K         Worker threads (default: hardware concurrency).\n"
        "  --metric-tol V      Absolute AP/APH/recall/prec drop to flag "
        "(default 0.03).\n"
        "  --latency-tol V     Fractional p95 latency rise to flag (default "
        "0.15).\n"
        "  --min-support N     Minimum GT count for a slice (default 8).\n"
        "  --top-k K           Worst slices to surface (default 15).\n"
        "  --reports D         Where to write reports (default reports).\n"
        "  --fail-on-regression  Exit with code 3 if any regression is found.\n"
        "\n"
        "dashboard --report FILE.json --out FILE.html\n"
        "  --report FILE.json  A compare_*.json produced by `pev compare`.\n"
        "  --out FILE.html     Output HTML path (required).\n";
}

// Minimal option parser: '--key value' pairs plus known boolean flags.
class Options {
public:
    Options(const std::vector<std::string>& args,
            const std::set<std::string>& boolean_flags) {
        for (std::size_t i = 0; i < args.size(); ++i) {
            const std::string& tok = args[i];
            if (tok.rfind("--", 0) != 0) {
                throw std::runtime_error("unexpected argument: '" + tok + "'");
            }
            const std::string key = tok.substr(2);
            if (boolean_flags.count(key) != 0) {
                flags_.insert(key);
                continue;
            }
            if (i + 1 >= args.size()) {
                throw std::runtime_error("missing value for option --" + key);
            }
            values_[key] = args[++i];
        }
    }

    bool has_flag(const std::string& key) const {
        return flags_.count(key) != 0;
    }
    bool has(const std::string& key) const { return values_.count(key) != 0; }
    std::string get(const std::string& key, const std::string& fallback) const {
        auto it = values_.find(key);
        return it == values_.end() ? fallback : it->second;
    }
    std::string require(const std::string& key) const {
        auto it = values_.find(key);
        if (it == values_.end()) {
            throw std::runtime_error("missing required option --" + key);
        }
        return it->second;
    }

private:
    std::map<std::string, std::string> values_;
    std::set<std::string> flags_;
};

double parse_double(const std::string& key, const std::string& value) {
    try {
        std::size_t used = 0;
        const double v = std::stod(value, &used);
        if (used != value.size()) throw std::invalid_argument("trailing");
        return v;
    } catch (const std::exception&) {
        throw std::runtime_error("invalid number for --" + key + ": '" + value +
                                 "'");
    }
}

long parse_long(const std::string& key, const std::string& value) {
    try {
        std::size_t used = 0;
        const long v = std::stol(value, &used);
        if (used != value.size()) throw std::invalid_argument("trailing");
        return v;
    } catch (const std::exception&) {
        throw std::runtime_error("invalid integer for --" + key + ": '" +
                                 value + "'");
    }
}

std::size_t resolve_threads(const Options& opts) {
    if (!opts.has("threads")) {
        return pev::default_thread_count();
    }
    const long k = parse_long("threads", opts.require("threads"));
    if (k < 1) throw std::runtime_error("--threads must be >= 1");
    return static_cast<std::size_t>(k);
}

double resolve_iou(const Options& opts) {
    const double iou = parse_double("iou", opts.get("iou", "0.5"));
    if (iou < 0.0 || iou > 1.0) {
        throw std::runtime_error("--iou must be within [0, 1]");
    }
    return iou;
}

int cmd_generate(const std::vector<std::string>& args) {
    const Options opts(args, {});
    pev::GenerateOptions gen;
    const std::string out = opts.require("out");
    if (opts.has("scenes")) {
        const long n = parse_long("scenes", opts.require("scenes"));
        if (n < 1) throw std::runtime_error("--scenes must be >= 1");
        gen.scenes = static_cast<int>(n);
    }
    if (opts.has("frames-per-scene")) {
        const long f =
            parse_long("frames-per-scene", opts.require("frames-per-scene"));
        if (f < 1) throw std::runtime_error("--frames-per-scene must be >= 1");
        gen.frames_per_scene = static_cast<int>(f);
    }
    if (opts.has("seed")) {
        gen.seed = static_cast<unsigned>(parse_long("seed", opts.require("seed")));
    }

    const pev::GeneratedDataset ds = pev::generate_dataset(gen);
    pev::save_ground_truth(out, ds.ground_truth);
    pev::save_run(out, ds.baseline);
    pev::save_run(out, ds.candidate);

    std::size_t frames = 0, gt_objects = 0;
    for (const pev::Scene& s : ds.ground_truth.scenes) {
        frames += s.frames.size();
        for (const pev::GtFrame& f : s.frames) gt_objects += f.objects.size();
    }

    std::cout << "Generated synthetic WOD-shaped dataset in '" << out << "'\n";
    std::cout << "  scenes:               " << ds.ground_truth.scenes.size()
              << "\n";
    std::cout << "  frames:               " << frames << "\n";
    std::cout << "  ground-truth objects: " << gt_objects << "\n";
    std::cout << "  seed:                 " << gen.seed << "\n";
    std::cout << "  files:\n";
    std::cout << "    " << pev::ground_truth_path(out) << "\n";
    std::cout << "    " << pev::run_path(out, "baseline") << "\n";
    std::cout << "    " << pev::run_path(out, "candidate") << "\n";
    return kExitOk;
}

int cmd_evaluate(const std::vector<std::string>& args) {
    const Options opts(args, {});
    const std::string data = opts.require("data");
    const std::string run_name = opts.require("run");
    const double iou = resolve_iou(opts);
    const std::size_t threads = resolve_threads(opts);
    const std::string reports_dir = opts.get("reports", "reports");

    const pev::GroundTruth gt = pev::load_ground_truth(data);
    const pev::Run run = pev::load_run(data, run_name);
    const pev::EvaluationReport report = pev::evaluate(gt, run, iou, threads);

    std::cout << pev::render_console(report) << "\n";

    const std::string stem = reports_dir + "/eval_" + run_name;
    pev::write_text_file(stem + ".json", pev::to_json(report).dump() + "\n");
    pev::write_text_file(stem + ".md", pev::render_markdown(report));
    pev::write_text_file(stem + ".html", pev::render_eval_html(report));
    std::cout << "Wrote reports:\n  " << stem << ".json\n  " << stem << ".md\n  "
              << stem << ".html\n";
    return kExitOk;
}

int cmd_track(const std::vector<std::string>& args) {
    const Options opts(args, {});
    const std::string data = opts.require("data");
    const std::string run_name = opts.require("run");
    const double iou = resolve_iou(opts);
    const std::size_t threads = resolve_threads(opts);
    const std::string reports_dir = opts.get("reports", "reports");

    const pev::GroundTruth gt = pev::load_ground_truth(data);
    const pev::Run run = pev::load_run(data, run_name);
    pev::TrackingConfig cfg;
    cfg.iou_threshold = iou;
    const pev::TrackingReport report =
        pev::evaluate_tracking(gt, run, cfg, threads);

    std::cout << pev::render_console(report) << "\n";

    const std::string stem = reports_dir + "/track_" + run_name;
    pev::write_text_file(stem + ".json", pev::to_json(report).dump() + "\n");
    pev::write_text_file(stem + ".md", pev::render_markdown(report));
    std::cout << "Wrote reports:\n  " << stem << ".json\n  " << stem << ".md\n";
    return kExitOk;
}

int cmd_compare(const std::vector<std::string>& args) {
    const Options opts(args, {"fail-on-regression"});
    const std::string data = opts.require("data");
    const std::string baseline_name = opts.require("baseline");
    const std::string candidate_name = opts.require("candidate");
    const double iou = resolve_iou(opts);
    const std::size_t threads = resolve_threads(opts);
    const std::string reports_dir = opts.get("reports", "reports");

    pev::TriageConfig cfg;
    if (opts.has("metric-tol")) {
        cfg.metric_tolerance =
            parse_double("metric-tol", opts.require("metric-tol"));
    }
    if (opts.has("latency-tol")) {
        cfg.latency_tolerance =
            parse_double("latency-tol", opts.require("latency-tol"));
    }
    if (opts.has("min-support")) {
        cfg.min_support = parse_long("min-support", opts.require("min-support"));
    }
    if (opts.has("top-k")) {
        const long k = parse_long("top-k", opts.require("top-k"));
        if (k < 1) throw std::runtime_error("--top-k must be >= 1");
        cfg.top_k = static_cast<std::size_t>(k);
    }

    const pev::GroundTruth gt = pev::load_ground_truth(data);
    const pev::Run baseline = pev::load_run(data, baseline_name);
    const pev::Run candidate = pev::load_run(data, candidate_name);

    const pev::EvaluationReport base_eval =
        pev::evaluate(gt, baseline, iou, threads);
    const pev::EvaluationReport cand_eval =
        pev::evaluate(gt, candidate, iou, threads);
    const pev::TriageReport triage =
        pev::triage(base_eval, cand_eval, cfg);

    std::cout << pev::render_console(triage) << "\n";

    const std::string stem =
        reports_dir + "/compare_" + baseline_name + "_vs_" + candidate_name;
    const pev::json::Json compare_json = pev::to_json(triage);
    pev::write_text_file(stem + ".json", compare_json.dump() + "\n");
    pev::write_text_file(stem + ".md", pev::render_markdown(triage));
    pev::write_text_file(stem + ".html",
                         pev::render_compare_html_from_json(compare_json));
    std::cout << "Wrote reports:\n  " << stem << ".json\n  " << stem << ".md\n  "
              << stem << ".html\n";

    if (opts.has_flag("fail-on-regression") && triage.has_regressions()) {
        std::cout << "\nRegressions detected; exiting with code "
                  << kExitRegression << " (--fail-on-regression).\n";
        return kExitRegression;
    }
    return kExitOk;
}

int cmd_dashboard(const std::vector<std::string>& args) {
    const Options opts(args, {});
    const std::string report_path = opts.require("report");
    const std::string out_path = opts.require("out");
    const pev::json::Json j = pev::json::Json::parse(
        pev::read_text_file(report_path));
    pev::write_text_file(out_path, pev::render_compare_html_from_json(j));
    std::cout << "Wrote dashboard: " << out_path << "\n";
    return kExitOk;
}

}  // namespace

int main(int argc, char** argv) {
    std::vector<std::string> args(argv + 1, argv + argc);

    if (args.empty() || args[0] == "help" || args[0] == "--help" ||
        args[0] == "-h") {
        print_usage(std::cout);
        return args.empty() ? kExitUsage : kExitOk;
    }

    const std::string command = args[0];
    const std::vector<std::string> rest(args.begin() + 1, args.end());

    try {
        if (command == "generate") return cmd_generate(rest);
        if (command == "evaluate") return cmd_evaluate(rest);
        if (command == "track") return cmd_track(rest);
        if (command == "compare") return cmd_compare(rest);
        if (command == "dashboard") return cmd_dashboard(rest);

        std::cerr << "error: unknown command '" << command << "'\n\n";
        print_usage(std::cerr);
        return kExitUsage;
    } catch (const pev::IoError& e) {
        std::cerr << "error: " << e.what() << "\n";
        return kExitRuntime;
    } catch (const pev::json::ParseError& e) {
        std::cerr << "error: malformed JSON: " << e.what() << "\n";
        return kExitRuntime;
    } catch (const std::runtime_error& e) {
        std::cerr << "error: " << e.what() << "\n\n";
        print_usage(std::cerr);
        return kExitUsage;
    } catch (const std::exception& e) {
        std::cerr << "error: " << e.what() << "\n";
        return kExitRuntime;
    }
}
