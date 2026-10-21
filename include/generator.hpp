// generator.hpp — deterministic synthetic, WOD-shaped dataset generation.
//
// IMPORTANT: the data produced here is SYNTHETIC and only *shaped* like the
// Waymo Open Dataset (same taxonomy, difficulty levels, range bins, 3D oriented
// boxes with heading, per-object point counts and persistent track IDs). It is
// NOT the real Waymo Open Dataset. The real dataset is license-gated and must
// be downloaded and converted separately (see tools/wod_to_platform.py).
//
// The generator produces a ground-truth set of multi-frame scenes plus two
// model runs:
//   * baseline  — a strong 3D detector/tracker
//   * candidate — the same system with regressions deliberately injected into
//                 specific slices (misses more far-range LEVEL_2 pedestrians,
//                 worse heading on cyclists, slower under low visibility, and
//                 more identity switches on urban vehicles)
// so that `compare` surfaces real, localised findings.
//
// Everything is driven by a seeded std::mt19937, so a given (scenes,
// frames-per-scene, seed) triple always yields byte-identical output.
//
// Author: Aditya Kopalli, 2026. MIT licensed.

#ifndef PEV_GENERATOR_HPP
#define PEV_GENERATOR_HPP

#include "dataset.hpp"

namespace pev {

struct GenerateOptions {
    int scenes = 40;
    int frames_per_scene = 8;
    unsigned seed = 42;
};

struct GeneratedDataset {
    GroundTruth ground_truth;
    Run baseline;
    Run candidate;
};

GeneratedDataset generate_dataset(const GenerateOptions& opts);

}  // namespace pev

#endif  // PEV_GENERATOR_HPP
