// dataset.hpp — the WOD-shaped data model (scenes / frames / 3D labels and
// detections with track IDs) plus JSON (de)serialization and the on-disk
// layout helpers.
//
// On-disk layout produced/consumed by the platform:
//
//   <data>/ground_truth.json      ground-truth scenes (tags + 3D labels)
//   <data>/runs/<name>.json       one model run (3D detections + latency)
//
// A "scene" is the analogue of a WOD segment: an ordered list of frames that
// share a set of scene-condition tags. Ground-truth objects and detections
// both carry a track_id so the tracking evaluator can follow identities across
// frames.
//
// Author: Aditya Kopalli, 2026. MIT licensed.

#ifndef PEV_DATASET_HPP
#define PEV_DATASET_HPP

#include <string>
#include <vector>

#include "geometry.hpp"
#include "json.hpp"
#include "taxonomy.hpp"

namespace pev {

// Scene-condition tags a scene can carry. Kept as a small closed set so the
// slice machinery can enumerate them deterministically.
namespace tags {
constexpr const char* kDayClear = "day_clear";
constexpr const char* kNight = "night";
constexpr const char* kRain = "rain";
constexpr const char* kFog = "fog";
constexpr const char* kLowVisibility = "low_visibility";
constexpr const char* kUrban = "urban";
constexpr const char* kHighway = "highway";
constexpr const char* kSuburban = "suburban";
}  // namespace tags

// Every scene also implicitly belongs to this synthetic "all scenes" tag so
// that tag-independent aggregates can be computed without double-counting
// objects that carry several real tags.
constexpr const char* kAnyTag = "(any)";

// --- Ground truth -----------------------------------------------------------

struct GtObject {
    ObjectType type = ObjectType::Vehicle;
    Box3D box;
    int num_points = 0;   // LiDAR-point / visibility proxy for difficulty
    int track_id = -1;    // persistent identity across frames within a scene

    Difficulty difficulty() const { return difficulty_from_points(num_points); }
    RangeBin range_bin() const { return range_bin_for(box.range()); }
};

struct GtFrame {
    int index = 0;
    std::vector<GtObject> objects;
};

struct Scene {
    std::string id;
    std::vector<std::string> tags;
    std::vector<GtFrame> frames;

    bool has_tag(const std::string& t) const;
};

struct GroundTruth {
    std::vector<Scene> scenes;
};

// --- Model runs -------------------------------------------------------------

struct Detection {
    ObjectType type = ObjectType::Vehicle;
    Box3D box;
    double score = 0.0;
    int track_id = -1;  // predicted identity (for tracking evaluation)

    RangeBin range_bin() const { return range_bin_for(box.range()); }
};

struct RunFrame {
    int index = 0;
    double latency_ms = 0.0;
    std::vector<Detection> detections;
};

struct RunScene {
    std::string scene_id;
    std::vector<RunFrame> frames;
};

struct Run {
    std::string name;
    std::vector<RunScene> scenes;
};

// --- JSON (de)serialization -------------------------------------------------

json::Json box3d_to_json(const Box3D& b);
Box3D box3d_from_json(const json::Json& j);

json::Json to_json(const GroundTruth& gt);
GroundTruth ground_truth_from_json(const json::Json& j);

json::Json to_json(const Run& run);
Run run_from_json(const json::Json& j);

// --- On-disk layout + load/save ---------------------------------------------

std::string ground_truth_path(const std::string& data_dir);
std::string run_path(const std::string& data_dir, const std::string& run_name);

void save_ground_truth(const std::string& data_dir, const GroundTruth& gt);
GroundTruth load_ground_truth(const std::string& data_dir);

void save_run(const std::string& data_dir, const Run& run);
Run load_run(const std::string& data_dir, const std::string& run_name);

}  // namespace pev

#endif  // PEV_DATASET_HPP
