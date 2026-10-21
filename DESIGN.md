# Design

This document explains the metric definitions and the key engineering choices
behind the AV Perception Evaluation Platform (`pev`). It is the companion to
`README.md` (how to run it) and `docs/architecture.md` (how modules fit).

## What is real and what is synthetic

- **Real:** every metric (3D IoU, AP, APH, CLEAR-MOT), the matching and
  assignment algorithms, the slice-based triage, the multithreaded evaluator,
  the JSON/Markdown/HTML reporting, and the format compatibility with the Waymo
  Open Dataset (WOD) label schema.
- **Synthetic:** the bundled dataset. `pev generate` produces data that is
  *shaped* like WOD (same taxonomy, difficulty levels, range bins, 3D oriented
  boxes with heading, per-object point counts, persistent track IDs, scene
  condition tags) but is **not** the real Waymo Open Dataset. The real dataset
  is license-gated and requires a TensorFlow/GPU toolchain to read; see
  `tools/wod_to_platform.py` for the documented conversion path that lets the
  exact same C++ run on real data.

No results in this repository were produced from the real Waymo Open Dataset.

## Coordinate frame and the 3D box

All geometry is in the ego-vehicle frame used by WOD: the ego sits at the
origin, **+x forward, +y left, +z up** (right-handed). A `Box3D` is a WOD-style
label: center `(cx, cy, cz)`, dimensions `(length, width, height)` where length
is along the heading and width perpendicular to it, and a `heading` (yaw) in
radians about +z.

## 3D IoU

For two oriented boxes the 3D intersection-over-union is

```
intersection_volume = bev_intersection_area(a, b) * z_overlap(a, b)
union_volume         = volume(a) + volume(b) - intersection_volume
iou_3d               = intersection_volume / union_volume
```

- `bev_intersection_area` is the area of the intersection of the two rotated
  rectangles projected onto the ground plane. Each rectangle's four corners are
  computed from its center, dimensions and heading, and the intersection is
  obtained by **Sutherland-Hodgman polygon clipping** of one rectangle against
  the other (both are convex), followed by the shoelace area of the clipped
  polygon.
- `z_overlap` is the overlap length of the two `[z_min, z_max]` intervals,
  clamped at zero.

This is exact for convex boxes (no sampling/voxelization) and is unit-tested
against axis-aligned, 45-degree, 90-degree, fully-contained, BEV-disjoint and
z-disjoint cases (e.g. two unit-square footprints at 45 degrees yield a regular
octagon of area `4 - 4*0.5*(2 - sqrt2)^2 = 3.3137`, giving `iou_3d = 1/sqrt2`).

## Heading accuracy and APH

Heading error folds the angular difference into `[0, pi]`:
`heading_error = min(|d|, 2pi - |d|)` with `d = a - b mod 2pi`. Heading accuracy
is `1 - heading_error / pi`, so it is 1 at a perfect match and 0 when the boxes
point in opposite directions.

**AP** integrates the precision-recall curve. Detections are sorted by
descending score (ties: true positives first, for reproducibility). Sweeping
the score threshold yields `(recall, precision)` points; the precision is
replaced by its right-monotonic envelope `p'[i] = max_{j>=i} p[j]` (interpolated
precision) and the area is integrated from recall 0 to the maximum achieved
recall. AP is **not** forced to recall 1.0 -- a detector that never reaches full
recall is not credited for recall it did not achieve.

**APH** (the signature Waymo metric) is the same integration except each true
positive contributes its heading accuracy instead of a full 1 in the numerator
of both precision and recall. Because every TP's contribution is scaled by a
factor in `[0, 1]`, **APH <= AP** always -- verified by a unit test where a
single TP with heading accuracy 0.5 gives `AP = 1.0, APH = 0.25`.

## Taxonomy and slicing

- **Types:** `TYPE_VEHICLE`, `TYPE_PEDESTRIAN`, `TYPE_CYCLIST`, `TYPE_SIGN`.
- **Difficulty:** derived from a per-object point-count/visibility proxy exactly
  as WOD derives difficulty from LiDAR point counts. `num_points >=
  kLevel1MinPoints` (50) is `LEVEL_1` (well observed); fewer is `LEVEL_2`.
- **Range bins:** `[0,30)m` Near, `[30,50)m` Mid, `[50,inf)m` Far.

### Difficulty semantics (a deliberate deviation from WOD)

The real WOD treats `LEVEL_2` as a **superset** that also includes the
`LEVEL_1` objects. For *regression localisation* this platform instead treats
`LEVEL_1` and `LEVEL_2` as **disjoint strata** (well-observed vs
sparsely-observed objects). This is intentional: it lets a regression be
attributed specifically to the hard stratum rather than being diluted by the
easy objects that a cumulative LEVEL_2 would also contain. The difference is
documented here and in `include/taxonomy.hpp`.

### Multi-tag aggregation

A scene carries several real tags (e.g. `night`, `urban`, `low_visibility`). To
compute tag-independent aggregates without double-counting objects that belong
to several tags, every scene also implicitly belongs to a synthetic `(any)`
tag. Aggregates (overall, per-type, per-type x range, per-type x difficulty)
are computed from the `(any)` groups; the per-tag slices in the slice map use
only the real tags.

### FP/TP difficulty attribution

A true positive inherits the difficulty and range bin of the ground-truth box
it matched. A false positive has no matched ground truth, so it is counted in
**both** difficulty strata of its type/range/tag slice (it is a real error
regardless of how the phantom object would have been graded).

## Matching

Within a frame, detections of one type are matched to ground truth of the same
type **greedily in descending score order**; each detection claims the
still-available ground-truth box with the highest 3D IoU at or above the
threshold (ties broken toward the lowest index for determinism). This is the
standard association used to turn scored detections into the per-detection
TP/FP labels the AP integrator consumes.

The tracking evaluator instead uses **optimal** assignment: per frame it builds
a square cost matrix (`cost = -IoU` for valid pairs, 0 otherwise) and solves it
with the **Hungarian (Kuhn-Munkres) O(n^3)** algorithm, then drops any assigned
pair below the IoU threshold. Optimal matching matters more for identity
continuity than for AP.

## CLEAR-MOT tracking metrics

```
MOTA = 1 - (misses + false_positives + id_switches) / total_gt
MOTP = mean 3D IoU over all matched pairs
```

- **IDSW:** a ground-truth track previously matched to hypothesis id `h` is now
  matched to a different id.
- **FRAG:** a ground-truth track's coverage is interrupted (a miss gap) and then
  resumed.
- **MT/PT/ML:** a ground-truth track is mostly-tracked if >= 80% of the frames
  it appears in are matched, mostly-lost if <= 20%, partially-tracked
  otherwise.

MOTA can be negative when `misses + fp + idsw` exceeds `total_gt` (e.g. the
`TYPE_SIGN` class in the synthetic data, which is deliberately hard).

## Slice-based regression triage (the headline feature)

Triage diffs two `EvaluationReport`s slice-by-slice over the full
`{type} x {range bin} x {difficulty} x {scene tag}` cross-product. For each
slice with at least `min_support` ground-truth objects it computes the deltas in
AP, APH, recall, precision and p95 latency, and a **severity** score:

```
severity = ( 0.45*drop(recall) + 0.30*drop(APH) + 0.15*drop(AP)
           + 0.10*drop(precision) + 0.40*latency_fraction_increase )
         * log1p(support) / log(10)
```

where `drop(x) = max(0, -delta_x)`. Recall and heading (APH) regressions are
weighted most because they are the safety-relevant failures for an AV; the
`log1p(support)` factor keeps a tiny, noisy slice from outranking a
well-supported one. A slice is **flagged as a regression** when any metric drops
by at least `metric_tolerance` (default 0.03) or p95 latency rises by more than
`latency_tolerance` (default 15%, with a 0.5 ms floor). Findings are emitted in
descending severity, giving an engineer a prioritised worklist rather than a
wall of numbers. `--fail-on-regression` turns the presence of any flagged slice
into exit code 3 for CI gating.

## Determinism

See `docs/architecture.md`. In short: per-scene parallelism with integer-sum
merges, score-sorted AP integration, nearest-rank percentiles on sorted copies,
and a seeded generator. The emitted reports are byte-identical across thread
counts except for the `thread_count`/`threads` metadata field.

## Dependencies and portability

C++17, standard library only. No third-party dependencies, no network access.
JSON is handled by a self-contained `include/json.hpp`. The reference build is a
plain `Makefile` + `clang++` (the author's macOS environment has no CMake); a
`CMakeLists.txt` is included for portability. All code compiles cleanly under
`-Wall -Wextra -Wpedantic` with zero warnings.
