#include "generator.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <random>
#include <string>
#include <vector>

namespace pev {

namespace {

constexpr double kPi = 3.14159265358979323846;

// Thin wrapper over std::mt19937 with the small distributions we need.
class Rng {
public:
    explicit Rng(unsigned seed) : engine_(seed) {}

    double uniform(double lo, double hi) {
        std::uniform_real_distribution<double> d(lo, hi);
        return d(engine_);
    }
    int randint(int lo, int hi) {  // inclusive
        std::uniform_int_distribution<int> d(lo, hi);
        return d(engine_);
    }
    double unit() { return uniform(0.0, 1.0); }
    double normal(double mean, double sd) {
        std::normal_distribution<double> d(mean, sd);
        return d(engine_);
    }

private:
    std::mt19937 engine_;
};

double round_to(double v, int decimals) {
    const double f = std::pow(10.0, decimals);
    return std::round(v * f) / f;
}

std::string scene_id(int i) {
    char buf[32];
    std::snprintf(buf, sizeof(buf), "seg_%04d", i);
    return std::string(buf);
}

bool has(const std::vector<std::string>& v, const char* t) {
    return std::find(v.begin(), v.end(), std::string(t)) != v.end();
}

// --- scene tags -------------------------------------------------------------

std::vector<std::string> assign_tags(int i, Rng& rng) {
    std::vector<std::string> out;
    // Weather / time of day, spread deterministically across scenes.
    switch (i % 4) {
        case 0: out.push_back(tags::kDayClear); break;
        case 1: out.push_back(tags::kNight); break;
        case 2: out.push_back(tags::kRain); break;
        default: out.push_back(tags::kFog); break;
    }
    // Location.
    const double r = rng.unit();
    if (r < 0.45) {
        out.push_back(tags::kUrban);
    } else if (r < 0.80) {
        out.push_back(tags::kHighway);
    } else {
        out.push_back(tags::kSuburban);
    }
    // Derived low-visibility condition.
    if (has(out, tags::kNight) || has(out, tags::kRain) ||
        has(out, tags::kFog)) {
        out.push_back(tags::kLowVisibility);
    }
    return out;
}

double visibility_factor(const std::vector<std::string>& tags) {
    if (has(tags, tags::kFog)) return 0.45;
    if (has(tags, tags::kRain)) return 0.75;
    if (has(tags, tags::kNight)) return 0.65;
    return 1.0;  // day_clear
}

ObjectType sample_type(Rng& rng) {
    const double r = rng.unit();
    if (r < 0.55) return ObjectType::Vehicle;
    if (r < 0.80) return ObjectType::Pedestrian;
    if (r < 0.92) return ObjectType::Cyclist;
    return ObjectType::Sign;
}

struct Dims {
    double length, width, height;
};

Dims sample_dims(Rng& rng, ObjectType t) {
    switch (t) {
        case ObjectType::Vehicle:
            return {rng.uniform(4.0, 5.5), rng.uniform(1.8, 2.1),
                    rng.uniform(1.4, 1.8)};
        case ObjectType::Pedestrian:
            return {rng.uniform(0.6, 1.0), rng.uniform(0.5, 0.9),
                    rng.uniform(1.5, 1.9)};
        case ObjectType::Cyclist:
            return {rng.uniform(1.6, 2.0), rng.uniform(0.6, 0.9),
                    rng.uniform(1.5, 1.8)};
        case ObjectType::Sign:
        default:
            return {rng.uniform(0.3, 0.6), rng.uniform(0.3, 0.6),
                    rng.uniform(1.5, 2.5)};
    }
}

double type_point_base(ObjectType t) {
    switch (t) {
        case ObjectType::Vehicle: return 2500.0;
        case ObjectType::Pedestrian: return 420.0;
        case ObjectType::Cyclist: return 650.0;
        case ObjectType::Sign: return 320.0;
    }
    return 500.0;
}

// A persistent ground-truth track within a scene.
struct Track {
    int id = 0;
    ObjectType type = ObjectType::Vehicle;
    Dims dims;
    double x0 = 0.0, y0 = 0.0;  // initial center (metres)
    double vx = 0.0, vy = 0.0;  // ego-relative velocity (m per frame)
    double heading0 = 0.0;
    double yaw_rate = 0.0;      // heading change per frame
    double cz = 0.9;
    int start = 0, end = 0;     // inclusive frame span
};

// Build the set of tracks for one scene.
std::vector<Track> build_tracks(int num_tracks, int frames, Rng& rng) {
    std::vector<Track> tracks;
    tracks.reserve(static_cast<std::size_t>(num_tracks));
    for (int k = 0; k < num_tracks; ++k) {
        Track t;
        t.id = k;
        t.type = sample_type(rng);
        t.dims = sample_dims(rng, t.type);
        t.cz = round_to(t.dims.height / 2.0, 3);

        const double range0 = rng.uniform(4.0, 78.0);
        // Bias azimuth toward the forward field of view.
        const double az = rng.normal(0.0, 0.7);
        t.x0 = range0 * std::cos(az);
        t.y0 = range0 * std::sin(az);

        double speed;  // metres per frame
        switch (t.type) {
            case ObjectType::Vehicle: speed = rng.uniform(0.3, 2.5); break;
            case ObjectType::Cyclist: speed = rng.uniform(0.2, 1.2); break;
            case ObjectType::Pedestrian: speed = rng.uniform(0.0, 0.4); break;
            case ObjectType::Sign:
            default: speed = 0.0; break;
        }
        t.heading0 = (t.type == ObjectType::Sign) ? rng.uniform(-kPi, kPi)
                                                   : rng.normal(0.0, 1.2);
        const double dir = t.heading0 + rng.normal(0.0, 0.2);
        t.vx = speed * std::cos(dir);
        t.vy = speed * std::sin(dir);
        t.yaw_rate = (t.type == ObjectType::Sign) ? 0.0 : rng.normal(0.0, 0.03);

        // Lifespan: most tracks span the whole scene, some appear/leave.
        if (rng.unit() < 0.65) {
            t.start = 0;
            t.end = frames - 1;
        } else {
            t.start = rng.randint(0, std::max(0, frames / 2));
            t.end = rng.randint(t.start + 1, frames - 1);
        }
        tracks.push_back(t);
    }
    return tracks;
}

GtObject object_at(const Track& t, int frame,
                   const std::vector<std::string>& tags, Rng& rng) {
    GtObject o;
    o.type = t.type;
    o.track_id = t.id;
    const double step = static_cast<double>(frame - t.start);
    Box3D b;
    b.cx = round_to(t.x0 + t.vx * step, 3);
    b.cy = round_to(t.y0 + t.vy * step, 3);
    b.cz = t.cz;
    b.length = round_to(t.dims.length, 3);
    b.width = round_to(t.dims.width, 3);
    b.height = round_to(t.dims.height, 3);
    b.heading = round_to(t.heading0 + t.yaw_rate * step, 4);
    o.box = b;

    const double range = b.range();
    const double vis = visibility_factor(tags);
    const double range_factor = std::exp(-range / 26.0);
    const double jitter = 0.7 + 0.6 * rng.unit();
    const double pts =
        type_point_base(t.type) * range_factor * vis * jitter;
    o.num_points = static_cast<int>(std::max(0.0, std::round(pts)));
    return o;
}

GroundTruth generate_ground_truth(const GenerateOptions& opts, Rng& rng) {
    GroundTruth gt;
    for (int i = 0; i < opts.scenes; ++i) {
        Scene s;
        s.id = scene_id(i);
        s.tags = assign_tags(i, rng);
        const int num_tracks = rng.randint(8, 16);
        const std::vector<Track> tracks =
            build_tracks(num_tracks, opts.frames_per_scene, rng);

        for (int f = 0; f < opts.frames_per_scene; ++f) {
            GtFrame frame;
            frame.index = f;
            for (const Track& t : tracks) {
                if (f < t.start || f > t.end) continue;
                frame.objects.push_back(object_at(t, f, s.tags, rng));
            }
            s.frames.push_back(frame);
        }
        gt.scenes.push_back(s);
    }
    return gt;
}

// --- run generation ---------------------------------------------------------

enum class RunKind { Baseline, Candidate };

struct DetectProfile {
    double detect_prob = 0.97;
    double center_noise = 0.12;   // metres (per-axis stddev scale)
    double dim_jitter = 0.04;     // fractional
    double heading_noise = 0.04;  // radians stddev scale
    double score_lo = 0.80;
    double score_hi = 0.98;
    double id_switch_prob = 0.0;  // per matched track per frame
};

DetectProfile profile_for(RunKind kind, const std::vector<std::string>& tags,
                          const GtObject& o) {
    DetectProfile p;
    const Difficulty diff = o.difficulty();
    const RangeBin rb = o.range_bin();

    // Hard (sparsely observed) objects are inherently harder for both runs.
    if (diff == Difficulty::Level2) {
        p.detect_prob = 0.88;
        p.score_lo = 0.70;
        p.score_hi = 0.93;
        p.center_noise = 0.18;
    }

    if (kind == RunKind::Baseline) {
        return p;
    }

    // --- injected candidate regressions, localised to slices ---------------

    // 1) Misses more far-range LEVEL_2 pedestrians.
    if (o.type == ObjectType::Pedestrian && diff == Difficulty::Level2 &&
        rb == RangeBin::Far) {
        p.detect_prob *= 0.52;
        p.score_lo = 0.55;
        p.score_hi = 0.80;
    }
    // ...and somewhat more at mid range too.
    if (o.type == ObjectType::Pedestrian && diff == Difficulty::Level2 &&
        rb == RangeBin::Mid) {
        p.detect_prob *= 0.78;
    }

    // 2) Worse heading estimation on cyclists.
    if (o.type == ObjectType::Cyclist) {
        p.heading_noise = 0.42;
    }

    // 3) General low-visibility degradation.
    if (has(tags, tags::kLowVisibility)) {
        p.detect_prob *= 0.93;
    }

    // 4) More identity switches for urban vehicles (tracking regression).
    if (o.type == ObjectType::Vehicle && has(tags, tags::kUrban)) {
        p.id_switch_prob = 0.22;
    }

    return p;
}

// Per-frame RNG seed, identical for both runs so their random draws line up and
// the candidate diverges from the baseline only where a regression is injected.
unsigned frame_seed(unsigned salt, int scene_idx, int frame_idx) {
    unsigned h = salt * 2654435761u;
    h ^= static_cast<unsigned>(scene_idx + 1) * 40503u;
    h ^= static_cast<unsigned>(frame_idx + 1) * 2246822519u;
    h ^= (h >> 15);
    h *= 2246822519u;
    h ^= (h >> 13);
    return h == 0 ? 0x9E3779B9u : h;
}

double frame_latency(Rng& rng, RunKind kind,
                     const std::vector<std::string>& tags) {
    double l = rng.normal(14.0, 2.2);
    if (rng.unit() < 0.08) {
        l = rng.normal(26.0, 4.0);  // occasional spike, shared by both runs
    }
    if (l < 1.0) l = 1.0;
    if (kind == RunKind::Candidate && has(tags, tags::kLowVisibility)) {
        l *= 1.7;  // slower inference under low visibility
    }
    return round_to(l, 2);
}

Detection detect_from_gt(const GtObject& o, const DetectProfile& p, int frame,
                         bool id_switch, double nx, double ny, double nz,
                         double nd, double nh, double st) {
    Detection d;
    d.type = o.type;
    Box3D b = o.box;
    b.cx = round_to(b.cx + nx * p.center_noise, 3);
    b.cy = round_to(b.cy + ny * p.center_noise, 3);
    b.cz = round_to(b.cz + nz * p.center_noise * 0.3, 3);
    const double scale = 1.0 + nd * p.dim_jitter;
    b.length = round_to(std::max(0.2, b.length * scale), 3);
    b.width = round_to(std::max(0.2, b.width * scale), 3);
    b.height = round_to(std::max(0.2, b.height * scale), 3);
    b.heading = round_to(b.heading + nh * p.heading_noise, 4);
    d.box = b;
    d.score = round_to(p.score_lo + st * (p.score_hi - p.score_lo), 3);
    if (id_switch) {
        d.track_id = o.track_id + 700000 + frame;  // a different, shifting id
    } else {
        d.track_id = o.track_id;
    }
    return d;
}

RunFrame generate_frame(RunKind kind, const std::vector<std::string>& tags,
                        const GtFrame& frame, unsigned salt, int scene_idx) {
    Rng rng(frame_seed(salt, scene_idx, frame.index));
    RunFrame rf;
    rf.index = frame.index;
    rf.latency_ms = frame_latency(rng, kind, tags);

    // One decision per ground-truth object. The SAME number of random values is
    // drawn for every object regardless of run kind, so the two runs stay in
    // lockstep and diverge only through their differing profile thresholds.
    for (const GtObject& o : frame.objects) {
        const double roll = rng.unit();
        const double nx = rng.normal(0.0, 1.0);
        const double ny = rng.normal(0.0, 1.0);
        const double nz = rng.normal(0.0, 1.0);
        const double nd = rng.normal(0.0, 1.0);
        const double nh = rng.normal(0.0, 1.0);
        const double st = rng.unit();
        const double id_roll = rng.unit();

        const DetectProfile p = profile_for(kind, tags, o);
        if (roll < p.detect_prob) {
            const bool id_switch = id_roll < p.id_switch_prob;
            rf.detections.push_back(
                detect_from_gt(o, p, frame.index, id_switch, nx, ny, nz, nd, nh,
                               st));
        }
    }

    // Up to three potential false positives per frame; shared rolls so common
    // FPs are identical across runs and only the injected extras differ.
    const double base_fp_rate = 0.07;
    double fp_rate = base_fp_rate;
    if (kind == RunKind::Candidate && has(tags, tags::kLowVisibility)) {
        fp_rate = base_fp_rate * 1.8;  // more hallucinations in poor visibility
    }
    for (int k = 0; k < 3; ++k) {
        const double roll = rng.unit();
        Detection d;
        d.type = sample_type(rng);
        const double range = rng.uniform(5.0, 75.0);
        const double az = rng.normal(0.0, 0.8);
        Box3D b;
        b.cx = round_to(range * std::cos(az), 3);
        b.cy = round_to(range * std::sin(az), 3);
        const Dims dims = sample_dims(rng, d.type);
        b.cz = round_to(dims.height / 2.0, 3);
        b.length = round_to(dims.length, 3);
        b.width = round_to(dims.width, 3);
        b.height = round_to(dims.height, 3);
        b.heading = round_to(rng.uniform(-kPi, kPi), 4);
        d.box = b;
        d.score = round_to(rng.uniform(0.30, 0.58), 3);
        d.track_id = 900000 + frame.index * 100 + k;  // transient false track
        const double threshold = (k == 0) ? fp_rate : fp_rate * 0.5;
        if (roll < threshold) {
            rf.detections.push_back(d);
        }
    }

    return rf;
}

Run generate_run(const std::string& name, RunKind kind, const GroundTruth& gt,
                 unsigned salt) {
    Run run;
    run.name = name;
    int scene_idx = 0;
    for (const Scene& s : gt.scenes) {
        RunScene rs;
        rs.scene_id = s.id;
        for (const GtFrame& frame : s.frames) {
            rs.frames.push_back(
                generate_frame(kind, s.tags, frame, salt, scene_idx));
        }
        run.scenes.push_back(rs);
        ++scene_idx;
    }
    return run;
}

}  // namespace

GeneratedDataset generate_dataset(const GenerateOptions& opts) {
    GeneratedDataset out;

    Rng gt_rng(opts.seed);
    out.ground_truth = generate_ground_truth(opts, gt_rng);

    const unsigned run_salt = opts.seed + 1000u;
    out.baseline =
        generate_run("baseline", RunKind::Baseline, out.ground_truth, run_salt);
    out.candidate = generate_run("candidate", RunKind::Candidate,
                                 out.ground_truth, run_salt);
    return out;
}

}  // namespace pev
