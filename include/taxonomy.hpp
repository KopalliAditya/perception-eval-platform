// taxonomy.hpp — the Waymo-Open-Dataset-aligned label taxonomy and the
// bucketing rules used to slice metrics.
//
// Object types mirror the WOD label types (TYPE_VEHICLE, TYPE_PEDESTRIAN,
// TYPE_CYCLIST, TYPE_SIGN). Difficulty levels (LEVEL_1 / LEVEL_2) are derived
// from a per-object point-count / visibility proxy, exactly as the real WOD
// derives difficulty from the number of LiDAR points on an object. Range bins
// partition detections by planar distance from the ego vehicle.
//
// NOTE on difficulty semantics: the real WOD treats LEVEL_2 as a *superset*
// that also contains the LEVEL_1 objects. For slice-localised regression
// triage this platform instead treats LEVEL_1 and LEVEL_2 as *disjoint* strata
// (well-observed vs. sparsely-observed objects) so that a regression can be
// attributed to the hard stratum specifically. This choice is documented in
// DESIGN.md.
//
// Author: Aditya Kopalli, 2026. MIT licensed.

#ifndef PEV_TAXONOMY_HPP
#define PEV_TAXONOMY_HPP

#include <array>
#include <string>

namespace pev {

// --- Object types -----------------------------------------------------------

enum class ObjectType { Vehicle, Pedestrian, Cyclist, Sign };

inline const std::array<ObjectType, 4>& all_types() {
    static const std::array<ObjectType, 4> kTypes = {
        ObjectType::Vehicle, ObjectType::Pedestrian, ObjectType::Cyclist,
        ObjectType::Sign};
    return kTypes;
}

// WOD-style spelling, e.g. "TYPE_VEHICLE".
std::string to_string(ObjectType t);
ObjectType object_type_from_string(const std::string& s);

// --- Difficulty levels ------------------------------------------------------

enum class Difficulty { Level1, Level2 };

inline const std::array<Difficulty, 2>& all_difficulties() {
    static const std::array<Difficulty, 2> kLevels = {Difficulty::Level1,
                                                       Difficulty::Level2};
    return kLevels;
}

std::string to_string(Difficulty d);
Difficulty difficulty_from_string(const std::string& s);

// An object with at least this many measured points is LEVEL_1 (well
// observed); fewer makes it LEVEL_2 (hard / sparsely observed).
constexpr int kLevel1MinPoints = 50;

Difficulty difficulty_from_points(int num_points);

// --- Range bins -------------------------------------------------------------

enum class RangeBin { Near, Mid, Far };

inline const std::array<RangeBin, 3>& all_range_bins() {
    static const std::array<RangeBin, 3> kBins = {RangeBin::Near, RangeBin::Mid,
                                                  RangeBin::Far};
    return kBins;
}

// Bin edges in metres: Near [0, 30), Mid [30, 50), Far [50, inf).
constexpr double kRangeNearMax = 30.0;
constexpr double kRangeMidMax = 50.0;

RangeBin range_bin_for(double range_m);

// Human-readable bin label, e.g. "[30,50)m".
std::string to_string(RangeBin b);

}  // namespace pev

#endif  // PEV_TAXONOMY_HPP
