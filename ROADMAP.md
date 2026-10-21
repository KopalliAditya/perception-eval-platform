# Roadmap

Planned and possible future work, roughly in priority order. This is a personal
project; the roadmap is a statement of direction, not a commitment.

## Near term

- **Real Waymo Open Dataset ingestion.** Finish `tools/wod_to_platform.py` into
  a working TFRecord -> JSON converter (behind an optional
  `waymo-open-dataset` extra) and validate the C++ metrics against the official
  WOD evaluation on a held-out segment. The C++ side already consumes the target
  schema unchanged.
- **Cumulative LEVEL_2 mode.** Add an evaluation flag to score difficulty the
  WOD-cumulative way (LEVEL_2 as a superset of LEVEL_1) alongside the current
  disjoint-strata mode used for regression localisation.
- **Breakdown by object size / num_points buckets**, not just range, to expose
  small-object failures more directly.

## Medium term

- **Confidence intervals on slice metrics** via bootstrap resampling over
  scenes, so the triage can distinguish a real regression from sampling noise
  rather than relying on a fixed support-weighted heuristic.
- **Matching cost beyond IoU** (center distance, BEV vs 3D options, per-class
  IoU thresholds like the WOD 0.7/0.5 split for vehicles vs pedestrians).
- **HOTA / IDF1 tracking metrics** in addition to CLEAR-MOT, which better
  capture association quality over long horizons.
- **Velocity / future-trajectory errors** if the dataset carries per-object
  velocity (WOD does), enabling motion-prediction-adjacent metrics.

## Longer term

- **Regression bisection:** given a series of runs, attribute a slice
  regression to the first run that introduced it.
- **Incremental / cached evaluation** keyed by run content hash so large
  sweeps only recompute changed slices.
- **Interactive dashboard:** make the self-contained HTML sortable/filterable
  by slice dimension while keeping it dependency-free (inline JS only).
- **Additional input adapters:** nuScenes and KITTI converters analogous to the
  WOD one, normalising onto the same internal schema.

## Non-goals

- Shipping or redistributing any real dataset. The platform converts data the
  user already has the rights to; it never bundles licensed data.
- Pulling in heavyweight third-party dependencies. The project intentionally
  stays C++17 standard-library-only with a self-contained JSON implementation.
