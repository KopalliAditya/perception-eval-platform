// test_main.cpp — the platform's unit + integration test suite.
//
// A tiny self-contained harness (no third-party framework). Every CHECK adds to
// a global tally; main() prints "<passed>/<total> checks passed" and returns a
// non-zero exit code if anything failed.
//
// Coverage:
//   * 3D IoU of oriented boxes (axis-aligned, 45-degree, 90-degree, disjoint,
//     fully-contained, z-disjoint) and the BEV polygon primitives
//   * heading error / accuracy
//   * Average Precision and heading-weighted APH integration
//   * greedy per-class detection matching
//   * the Hungarian assignment solver
//   * multi-object-tracking MOTA / MOTP / IDSW
//   * taxonomy bucketing (range bins, difficulty)
//   * latency percentiles
//   * dataset JSON round-trip
//   * slice-based regression triage on a generated dataset
//   * evaluation determinism across thread counts
//
// Author: Aditya Kopalli, 2026. MIT licensed.

#include <cmath>
#include <cstddef>
#include <iostream>
#include <string>
#include <vector>

#include "assignment.hpp"
#include "dataset.hpp"
#include "evaluator.hpp"
#include "generator.hpp"
#include "geometry.hpp"
#include "matching.hpp"
#include "metrics.hpp"
#include "report.hpp"
#include "taxonomy.hpp"
#include "tracking.hpp"
#include "triage.hpp"

namespace {

int g_checks = 0;
int g_failures = 0;

void check(bool cond, const std::string& what, int line) {
    ++g_checks;
    if (!cond) {
        ++g_failures;
        std::cerr << "FAIL [line " << line << "]: " << what << "\n";
    }
}

void check_near(double a, double b, double eps, const std::string& what,
                int line) {
    ++g_checks;
    if (std::fabs(a - b) > eps) {
        ++g_failures;
        std::cerr << "FAIL [line " << line << "]: " << what << " (" << a
                  << " vs " << b << ", eps " << eps << ")\n";
    }
}

#define CHECK(cond) check((cond), #cond, __LINE__)
#define CHECK_NEAR(a, b, eps) check_near((a), (b), (eps), #a " ~= " #b, __LINE__)

using pev::Box3D;

Box3D box(double cx, double cy, double cz, double l, double w, double h,
          double heading) {
    Box3D b;
    b.cx = cx;
    b.cy = cy;
    b.cz = cz;
    b.length = l;
    b.width = w;
    b.height = h;
    b.heading = heading;
    return b;
}

constexpr double kPi = 3.14159265358979323846;

// --- geometry ---------------------------------------------------------------

void test_geometry() {
    // Identical boxes: IoU == 1.
    const Box3D a = box(0, 0, 0, 2, 2, 2, 0);
    CHECK_NEAR(pev::iou_3d(a, a), 1.0, 1e-9);

    // Axis-aligned half-overlap in x: intersection vol 4, union 12 -> 1/3.
    const Box3D b = box(1, 0, 0, 2, 2, 2, 0);
    CHECK_NEAR(pev::iou_3d(a, b), 1.0 / 3.0, 1e-9);

    // Disjoint in BEV: IoU == 0.
    const Box3D far = box(100, 0, 0, 2, 2, 2, 0);
    CHECK_NEAR(pev::iou_3d(a, far), 0.0, 1e-12);

    // Disjoint in z only: IoU == 0.
    const Box3D high = box(0, 0, 10, 2, 2, 2, 0);
    CHECK_NEAR(pev::iou_3d(a, high), 0.0, 1e-12);

    // Fully contained: small vol 1 inside large vol 8 -> 1/8.
    const Box3D small = box(0, 0, 0, 1, 1, 1, 0);
    CHECK_NEAR(pev::iou_3d(a, small), 1.0 / 8.0, 1e-9);

    // 90-degree rotation of a square footprint maps onto itself -> IoU 1.
    const Box3D rot90 = box(0, 0, 0, 2, 2, 2, kPi / 2.0);
    CHECK_NEAR(pev::iou_3d(a, rot90), 1.0, 1e-9);

    // 45-degree rotation: BEV intersection is a regular octagon of area
    // 4 - 4*0.5*(2-sqrt2)^2 = 3.313708..., giving iou_3d = 1/sqrt(2).
    const Box3D rot45 = box(0, 0, 0, 2, 2, 2, kPi / 4.0);
    CHECK_NEAR(pev::bev_intersection_area(a, rot45), 3.3137085, 1e-5);
    CHECK_NEAR(pev::iou_3d(a, rot45), 1.0 / std::sqrt(2.0), 1e-6);

    // Polygon primitives.
    const std::vector<pev::Vec2> ccw_unit = {
        {0, 0}, {1, 0}, {1, 1}, {0, 1}};
    CHECK_NEAR(pev::signed_polygon_area(ccw_unit), 1.0, 1e-12);
    CHECK_NEAR(pev::polygon_area(ccw_unit), 1.0, 1e-12);

    const std::vector<pev::Vec2> subject = {
        {-1, -1}, {1, -1}, {1, 1}, {-1, 1}};
    const std::vector<pev::Vec2> clip = {
        {0, -2}, {2, -2}, {2, 2}, {0, 2}};
    const std::vector<pev::Vec2> clipped = pev::clip_polygon(subject, clip);
    CHECK_NEAR(pev::polygon_area(clipped), 2.0, 1e-9);

    // Heading error folds into [0, pi]; accuracy is 1 - err/pi.
    CHECK_NEAR(pev::heading_error(0.0, 0.0), 0.0, 1e-12);
    CHECK_NEAR(pev::heading_error(0.1, -0.1), 0.2, 1e-9);
    CHECK_NEAR(pev::heading_error(0.0, 2.0 * kPi), 0.0, 1e-9);
    CHECK_NEAR(pev::heading_error(0.0, kPi), kPi, 1e-9);
    CHECK_NEAR(pev::heading_accuracy(0.0, 0.0), 1.0, 1e-12);
    CHECK_NEAR(pev::heading_accuracy(0.0, kPi), 0.0, 1e-9);
    CHECK_NEAR(pev::heading_accuracy(0.0, kPi / 2.0), 0.5, 1e-9);
}

// --- taxonomy ---------------------------------------------------------------

void test_taxonomy() {
    CHECK(pev::range_bin_for(0.0) == pev::RangeBin::Near);
    CHECK(pev::range_bin_for(29.9) == pev::RangeBin::Near);
    CHECK(pev::range_bin_for(30.0) == pev::RangeBin::Mid);
    CHECK(pev::range_bin_for(49.9) == pev::RangeBin::Mid);
    CHECK(pev::range_bin_for(50.0) == pev::RangeBin::Far);
    CHECK(pev::range_bin_for(500.0) == pev::RangeBin::Far);

    CHECK(pev::difficulty_from_points(0) == pev::Difficulty::Level2);
    CHECK(pev::difficulty_from_points(49) == pev::Difficulty::Level2);
    CHECK(pev::difficulty_from_points(50) == pev::Difficulty::Level1);
    CHECK(pev::difficulty_from_points(5000) == pev::Difficulty::Level1);

    CHECK(pev::to_string(pev::ObjectType::Vehicle) == "TYPE_VEHICLE");
    CHECK(pev::to_string(pev::ObjectType::Pedestrian) == "TYPE_PEDESTRIAN");
    CHECK(pev::object_type_from_string("TYPE_CYCLIST") ==
          pev::ObjectType::Cyclist);
    CHECK(pev::to_string(pev::Difficulty::Level2) == "LEVEL_2");
}

// --- metrics: AP / APH / percentiles ----------------------------------------

void test_metrics() {
    using pev::DetEntry;

    // Perfect detector: 3 TPs, 3 GT, heading accurate -> AP == APH == 1.
    {
        std::vector<DetEntry> e = {
            {0.9, true, 1.0}, {0.8, true, 1.0}, {0.7, true, 1.0}};
        const pev::ApResult r = pev::average_precision(e, 3);
        CHECK_NEAR(r.ap, 1.0, 1e-9);
        CHECK_NEAR(r.aph, 1.0, 1e-9);
        CHECK(r.num_tp == 3 && r.num_fp == 0);
    }

    // TP, FP, TP with 2 GT -> interpolated AP == 5/6.
    {
        std::vector<DetEntry> e = {
            {0.9, true, 1.0}, {0.8, false, 0.0}, {0.7, true, 1.0}};
        const pev::ApResult r = pev::average_precision(e, 2);
        CHECK_NEAR(r.ap, 5.0 / 6.0, 1e-9);
        CHECK_NEAR(r.aph, 5.0 / 6.0, 1e-9);  // heading perfect -> equal
    }

    // Heading-penalised TP: AP == 1 but APH == 0.25.
    {
        std::vector<DetEntry> e = {{0.9, true, 0.5}};
        const pev::ApResult r = pev::average_precision(e, 1);
        CHECK_NEAR(r.ap, 1.0, 1e-9);
        CHECK_NEAR(r.aph, 0.25, 1e-9);
        CHECK(r.aph <= r.ap + 1e-12);
    }

    // Counts helpers.
    pev::Counts c{3, 1, 2};
    CHECK_NEAR(c.precision(), 3.0 / 4.0, 1e-12);
    CHECK_NEAR(c.recall(), 3.0 / 5.0, 1e-12);

    // Nearest-rank percentiles.
    const std::vector<double> s = {10, 20, 30, 40, 50, 60, 70, 80, 90, 100};
    CHECK_NEAR(pev::percentile(s, 0.0), 10.0, 1e-12);
    CHECK_NEAR(pev::percentile(s, 100.0), 100.0, 1e-12);
    CHECK_NEAR(pev::percentile(s, 50.0), 50.0, 1e-12);
    CHECK_NEAR(pev::percentile(s, 95.0), 100.0, 1e-12);
    const pev::LatencyStats ls = pev::compute_latency_stats(s);
    CHECK_NEAR(ls.mean, 55.0, 1e-9);
    CHECK(ls.count == 10);
}

// --- matching ---------------------------------------------------------------

void test_matching() {
    std::vector<pev::GtObject> gts(2);
    gts[0].type = pev::ObjectType::Vehicle;
    gts[0].box = box(10, 0, 1, 4, 2, 1.6, 0);
    gts[1].type = pev::ObjectType::Vehicle;
    gts[1].box = box(20, 0, 1, 4, 2, 1.6, 0);

    std::vector<pev::Detection> dets(3);
    dets[0].type = pev::ObjectType::Vehicle;
    dets[0].box = box(10, 0, 1, 4, 2, 1.6, 0);  // matches gt0
    dets[0].score = 0.9;
    dets[1].type = pev::ObjectType::Vehicle;
    dets[1].box = box(20, 0, 1, 4, 2, 1.6, 0);  // matches gt1
    dets[1].score = 0.8;
    dets[2].type = pev::ObjectType::Vehicle;
    dets[2].box = box(60, 0, 1, 4, 2, 1.6, 0);  // no overlap -> FP
    dets[2].score = 0.95;

    const pev::ClassMatchResult r = pev::match_class(dets, gts, 0.5);
    CHECK(r.tp() == 2);
    CHECK(r.fp() == 1);
    CHECK(r.fn() == 0);
}

// --- Hungarian assignment ---------------------------------------------------

void test_assignment() {
    const std::vector<std::vector<double>> cost = {
        {4, 1, 3}, {2, 0, 5}, {3, 2, 2}};
    const std::vector<int> a = pev::solve_assignment(cost);
    CHECK(a.size() == 3);
    double total = 0.0;
    std::vector<int> seen(3, 0);
    for (std::size_t r = 0; r < a.size(); ++r) {
        CHECK(a[r] >= 0 && a[r] < 3);
        seen[static_cast<std::size_t>(a[r])] += 1;
        total += cost[r][static_cast<std::size_t>(a[r])];
    }
    CHECK(seen[0] == 1 && seen[1] == 1 && seen[2] == 1);  // a permutation
    CHECK_NEAR(total, 5.0, 1e-9);
    CHECK(a[0] == 1 && a[1] == 0 && a[2] == 2);  // unique optimum
}

// --- tracking ---------------------------------------------------------------

pev::GtFrame gt_frame(int idx, int track_id, const Box3D& b) {
    pev::GtFrame f;
    f.index = idx;
    pev::GtObject o;
    o.type = pev::ObjectType::Vehicle;
    o.box = b;
    o.num_points = 2000;
    o.track_id = track_id;
    f.objects.push_back(o);
    return f;
}

pev::RunFrame run_frame(int idx, int track_id, const Box3D& b) {
    pev::RunFrame f;
    f.index = idx;
    f.latency_ms = 12.0;
    pev::Detection d;
    d.type = pev::ObjectType::Vehicle;
    d.box = b;
    d.score = 0.9;
    d.track_id = track_id;
    f.detections.push_back(d);
    return f;
}

void test_tracking() {
    const Box3D b = box(10, 0, 1, 4, 2, 1.6, 0);

    // Perfect, stable track across two frames.
    {
        pev::GroundTruth gt;
        pev::Scene s;
        s.id = "seg_0000";
        s.frames.push_back(gt_frame(0, 0, b));
        s.frames.push_back(gt_frame(1, 0, b));
        gt.scenes.push_back(s);

        pev::Run run;
        run.name = "t";
        pev::RunScene rs;
        rs.scene_id = "seg_0000";
        rs.frames.push_back(run_frame(0, 5, b));
        rs.frames.push_back(run_frame(1, 5, b));
        run.scenes.push_back(rs);

        const pev::TrackingReport rep =
            pev::evaluate_tracking(gt, run, pev::TrackingConfig{}, 1);
        const pev::TrackingCounts& tc = rep.total;
        CHECK(tc.total_gt == 2);
        CHECK(tc.matches == 2);
        CHECK(tc.misses == 0);
        CHECK(tc.false_positives == 0);
        CHECK(tc.id_switches == 0);
        CHECK_NEAR(tc.mota(), 1.0, 1e-12);
        CHECK_NEAR(tc.motp(), 1.0, 1e-9);
        CHECK(tc.num_gt_tracks == 1);
        CHECK(tc.mostly_tracked == 1);
    }

    // Same geometry but the hypothesis ID flips between frames -> 1 IDSW.
    {
        pev::GroundTruth gt;
        pev::Scene s;
        s.id = "seg_0000";
        s.frames.push_back(gt_frame(0, 0, b));
        s.frames.push_back(gt_frame(1, 0, b));
        gt.scenes.push_back(s);

        pev::Run run;
        run.name = "t";
        pev::RunScene rs;
        rs.scene_id = "seg_0000";
        rs.frames.push_back(run_frame(0, 5, b));
        rs.frames.push_back(run_frame(1, 6, b));  // different hyp id
        run.scenes.push_back(rs);

        const pev::TrackingReport rep =
            pev::evaluate_tracking(gt, run, pev::TrackingConfig{}, 1);
        const pev::TrackingCounts& tc = rep.total;
        CHECK(tc.matches == 2);
        CHECK(tc.id_switches == 1);
        CHECK_NEAR(tc.mota(), 0.5, 1e-12);  // 1 - 1/2
    }
}

// --- dataset JSON round-trip ------------------------------------------------

void test_json_roundtrip() {
    pev::GenerateOptions opts;
    opts.scenes = 4;
    opts.frames_per_scene = 4;
    opts.seed = 7;
    const pev::GeneratedDataset ds = pev::generate_dataset(opts);

    const pev::json::Json gj = pev::to_json(ds.ground_truth);
    const pev::GroundTruth gt2 =
        pev::ground_truth_from_json(pev::json::Json::parse(gj.dump()));
    CHECK(gt2.scenes.size() == ds.ground_truth.scenes.size());

    std::size_t objs1 = 0, objs2 = 0;
    for (const pev::Scene& s : ds.ground_truth.scenes)
        for (const pev::GtFrame& f : s.frames) objs1 += f.objects.size();
    for (const pev::Scene& s : gt2.scenes)
        for (const pev::GtFrame& f : s.frames) objs2 += f.objects.size();
    CHECK(objs1 == objs2);

    const pev::json::Json rj = pev::to_json(ds.candidate);
    const pev::Run run2 = pev::run_from_json(pev::json::Json::parse(rj.dump()));
    CHECK(run2.name == ds.candidate.name);
    CHECK(run2.scenes.size() == ds.candidate.scenes.size());

    // Box round-trip preserves fields.
    const Box3D orig = box(1.5, -2.25, 0.8, 4.2, 1.9, 1.6, 0.73);
    const Box3D rt = pev::box3d_from_json(pev::box3d_to_json(orig));
    CHECK_NEAR(rt.cx, orig.cx, 1e-9);
    CHECK_NEAR(rt.heading, orig.heading, 1e-9);
    CHECK_NEAR(rt.length, orig.length, 1e-9);
}

// --- triage (integration) ---------------------------------------------------

void test_triage() {
    pev::GenerateOptions opts;
    opts.scenes = 30;
    opts.frames_per_scene = 8;
    opts.seed = 42;
    const pev::GeneratedDataset ds = pev::generate_dataset(opts);

    const pev::EvaluationReport base =
        pev::evaluate(ds.ground_truth, ds.baseline, 0.5, 1);
    const pev::EvaluationReport cand =
        pev::evaluate(ds.ground_truth, ds.candidate, 0.5, 1);

    pev::TriageConfig cfg;
    cfg.min_support = 5;
    const pev::TriageReport tri = pev::triage(base, cand, cfg);

    // The generator injects real regressions, so triage must surface them.
    CHECK(tri.has_regressions());
    CHECK(!tri.regressions.empty());

    // The candidate is strictly worse overall (injected misses lower recall).
    CHECK(cand.overall.ap <= base.overall.ap + 1e-9);

    // Deltas are ranked by severity, descending.
    bool ordered = true;
    for (std::size_t i = 1; i < tri.deltas.size(); ++i) {
        if (tri.deltas[i - 1].severity + 1e-12 < tri.deltas[i].severity) {
            ordered = false;
        }
    }
    CHECK(ordered);

    // Findings render to non-empty human-readable lines.
    if (!tri.regressions.empty()) {
        CHECK(!pev::finding_line(tri.regressions.front()).empty());
        CHECK(!pev::slice_label(tri.regressions.front().key).empty());
    }
}

// --- determinism ------------------------------------------------------------

void test_determinism() {
    pev::GenerateOptions opts;
    opts.scenes = 20;
    opts.frames_per_scene = 8;
    opts.seed = 123;
    const pev::GeneratedDataset ds = pev::generate_dataset(opts);

    pev::EvaluationReport r1 =
        pev::evaluate(ds.ground_truth, ds.candidate, 0.5, 1);
    pev::EvaluationReport r8 =
        pev::evaluate(ds.ground_truth, ds.candidate, 0.5, 8);

    // Reports must be byte-identical apart from the thread-count metadata.
    r8.thread_count = r1.thread_count;
    const std::string j1 = pev::to_json(r1).dump();
    const std::string j8 = pev::to_json(r8).dump();
    CHECK(j1 == j8);

    // The generator itself is reproducible for a fixed seed.
    const pev::GeneratedDataset ds2 = pev::generate_dataset(opts);
    CHECK(pev::to_json(ds2.candidate).dump() ==
          pev::to_json(ds.candidate).dump());
}

}  // namespace

int main() {
    test_geometry();
    test_taxonomy();
    test_metrics();
    test_matching();
    test_assignment();
    test_tracking();
    test_json_roundtrip();
    test_triage();
    test_determinism();

    const int passed = g_checks - g_failures;
    std::cout << passed << "/" << g_checks << " checks passed\n";
    if (g_failures != 0) {
        std::cout << g_failures << " checks FAILED\n";
        return 1;
    }
    std::cout << "All tests passed.\n";
    return 0;
}
