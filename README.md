# AV Perception Evaluation Platform (`pev`)

A from-scratch C++17 platform for evaluating 3D object-detection and
multi-object-tracking models the way an autonomous-vehicle perception team
does: **AP / APH** detection metrics and **CLEAR-MOT** tracking metrics computed
over the full `{class} x {range} x {difficulty} x {scene condition}` slice
cross-product, plus a **slice-based regression triage** that turns two model
runs into a prioritised, human-readable worklist of exactly where the candidate
got worse. Format-compatible with the Waymo Open Dataset (WOD) label schema.

---

## Important: synthetic vs. real data

This repository is fully runnable **today, offline, with no dataset download**.
It does that by shipping a generator (`pev generate`) that produces a
**synthetic, WOD-shaped** dataset:

- **Synthetic** = the data is *shaped* like the Waymo Open Dataset (same
  taxonomy `TYPE_VEHICLE/PEDESTRIAN/CYCLIST/SIGN`, the `LEVEL_1/LEVEL_2`
  difficulty split, range bins, 3D oriented boxes with heading, per-object point
  counts, persistent track IDs and scene-condition tags) but it is **not** the
  real Waymo Open Dataset. No numbers in this repo came from real Waymo data.
- **Real** = the metrics, the matching/assignment algorithms, the triage, the
  multithreaded evaluator and the reporting are all real, production-style code.

The real Waymo Open Dataset is license-gated and needs a TensorFlow/GPU
toolchain to read. To run this exact C++ on real data, download the dataset and
use the documented converter stub `tools/wod_to_platform.py`, which emits the
same JSON schema the platform already consumes. See `DESIGN.md` for the full
"what is real vs synthetic" statement.

---

## Features

- **Exact 3D IoU** for oriented boxes: BEV rotated-rectangle intersection via
  Sutherland-Hodgman polygon clipping, times vertical z-overlap, over union.
- **AP** (interpolated precision-recall integration) and **APH**
  (heading-weighted AP, the signature Waymo metric; APH <= AP by construction).
- **Multi-object tracking**: MOTA / MOTP / IDSW / FRAG / MT / PT / ML with
  optimal per-frame association via the **Hungarian** algorithm.
- **Slice-based regression triage** across
  `{class} x {range} x {difficulty} x {scene tag}` with a severity ranking that
  weights recall and heading regressions most and scales by slice support.
- **Self-contained HTML dashboard** (inline CSS only, zero external resources).
- **Deterministic** output: byte-identical reports for any thread count.
- **Multithreaded** per-scene evaluation on an RAII thread pool.
- **Zero third-party dependencies**: C++17 standard library only, including a
  self-contained JSON implementation. No network access.

See `docs/architecture.md` for the module graph and data-flow, and `DESIGN.md`
for the precise metric definitions.

---

## Build

The reference build is a plain `Makefile` + `clang++` (the development machine
has no CMake). Compiles cleanly with `-std=c++17 -Wall -Wextra -Wpedantic` and
**zero warnings**.

```sh
make            # builds build/pev
make test       # builds and runs the unit + integration test suite
make all        # builds pev and the test runner
make clean
```

A `CMakeLists.txt` is included for portability:

```sh
cmake -S . -B build && cmake --build build && ctest --test-dir build
```

### Test output

```
85/85 checks passed
All tests passed.
```

The suite covers 3D IoU (axis-aligned / 45-degree / 90-degree / disjoint /
contained / z-disjoint), polygon clipping, heading accuracy, AP + APH
integration, greedy matching, the Hungarian solver, MOTA/MOTP/IDSW, taxonomy
bucketing, latency percentiles, JSON round-trip, the full triage pipeline, and
evaluation determinism across thread counts.

---

## Quickstart

```sh
# 1. Generate a synthetic, WOD-shaped dataset (ground truth + baseline + candidate)
./build/pev generate --out data --scenes 40 --frames-per-scene 8 --seed 42

# 2. Detection evaluation (AP / APH per class, range, difficulty)
./build/pev evaluate --data data --run baseline --iou 0.5

# 3. Tracking evaluation (MOTA / MOTP / IDSW / FRAG / MT / ML)
./build/pev track --data data --run candidate

# 4. Slice-based regression triage; exit code 3 if regressions found
./build/pev compare --data data --baseline baseline --candidate candidate \
    --iou 0.5 --fail-on-regression

# 5. Re-render the self-contained HTML dashboard from a saved compare report
./build/pev dashboard --report reports/compare_baseline_vs_candidate.json \
    --out dashboard.html
```

`evaluate`, `track` and `compare` each write `.json`, `.md` and (where
applicable) a self-contained `.html` report under `reports/`. Run `./build/pev
help` for the full option reference. A committed example dataset and its reports
(including the HTML dashboard) live under [`examples/`](examples/).

---

## Real output

All output below is copy-pasted from an actual run on the synthetic dataset
(`--scenes 40 --frames-per-scene 8 --seed 42`).

### `pev evaluate --data data --run baseline --iou 0.5`

```
OVERALL
  AP 0.706   APH 0.692   precision 0.820   recall 0.791   F1 0.805
  TP 2529  FP 557  FN 669   (num_gt 3198)
  latency (ms): mean 14.99  p50 14.45  p95 24.78  (320 frames)

AP / APH BY TYPE
type                 numGT      AP     APH    prec  recall      F1
------------------------------------------------------------------
TYPE_VEHICLE          1798   0.967   0.946   0.985   0.967   0.976
TYPE_PEDESTRIAN        743   0.399   0.391   0.595   0.563   0.578
TYPE_CYCLIST           350   0.760   0.745   0.857   0.820   0.838
TYPE_SIGN              307   0.104   0.102   0.303   0.280   0.291

AP / APH BY TYPE x DIFFICULTY
type              difficulty     numGT      AP     APH  recall
--------------------------------------------------------------
TYPE_VEHICLE      LEVEL_1         1797   0.967   0.946   0.967
TYPE_VEHICLE      LEVEL_2            1   1.000   0.950   1.000
TYPE_PEDESTRIAN   LEVEL_1          435   0.493   0.484   0.703
TYPE_PEDESTRIAN   LEVEL_2          308   0.107   0.105   0.364
TYPE_CYCLIST      LEVEL_1          240   0.840   0.824   0.908
TYPE_CYCLIST      LEVEL_2          110   0.394   0.386   0.627
TYPE_SIGN         LEVEL_1          174   0.143   0.141   0.402
TYPE_SIGN         LEVEL_2          133   0.009   0.009   0.120
```

(APH is consistently <= AP, as required; harder LEVEL_2 strata and farther
range bins score lower, as expected.)

### `pev track --data data --run baseline` vs `--run candidate`

```
baseline:
type                   MOTA     MOTP   IDSW   FRAG    MT    PT    ML  GTtracks
-----------------------------------------------------------------------------
TYPE_VEHICLE          0.952    0.813      0     37   264    11     0       275
TYPE_PEDESTRIAN       0.179    0.659      0     85    20    80    11       111
TYPE_CYCLIST          0.683    0.702      0     33    29    19     0        48
TYPE_SIGN            -0.365    0.649      0     19     0    29    15        44
ALL                   0.617    0.769      0    174   313   139    26       478

candidate:
type                   MOTA     MOTP   IDSW   FRAG    MT    PT    ML  GTtracks
-----------------------------------------------------------------------------
TYPE_VEHICLE          0.741    0.813    267     97   232    43     0       275
TYPE_PEDESTRIAN       0.184    0.661      0     79    14    73    24       111
TYPE_CYCLIST          0.309    0.640      0     48     9    38     1        48
TYPE_SIGN            -0.329    0.648      0     19     0    28    16        44
ALL                   0.462    0.767    267    243   255   182    41       478
```

The injected tracking regression is obvious: urban-vehicle identity switches
jump from **0 to 267** and cyclist MOTA falls from **0.683 to 0.309**.

### `pev compare --baseline baseline --candidate candidate --fail-on-regression`

```
AGGREGATE DELTA (candidate - baseline)
  AP        0.706 -> 0.643   (-0.063)
  APH       0.692 -> 0.622   (-0.069)
  recall    0.791 -> 0.721   (-0.069)
  precision 0.820 -> 0.804   (-0.016)
  p95 ms    24.78 -> 32.81   (+8.03)

TOP REGRESSING SLICES (15 of 99 flagged; 110 slices compared)
   1. TYPE_CYCLIST @ [0,30)m, LEVEL_1, night: recall -0.351, APH -0.525, AP -0.477, prec -0.325, p95 +13.5ms (n=37)
   2. TYPE_CYCLIST @ [30,50)m, LEVEL_1, low_visibility: recall -0.221, APH -0.409, AP -0.326, prec -0.243, p95 +17.4ms (n=77)
   3. TYPE_CYCLIST @ [0,30)m, LEVEL_1, low_visibility: recall -0.219, APH -0.337, AP -0.276, prec -0.183, p95 +17.4ms (n=96)
   4. TYPE_CYCLIST @ [30,50)m, LEVEL_1, urban: recall -0.237, APH -0.450, AP -0.378, prec -0.253, p95 +14.1ms (n=59)
   5. TYPE_PEDESTRIAN @ [50,inf)m, LEVEL_2, low_visibility: recall -0.222, APH -0.113, AP -0.116, prec -0.087, p95 +17.4ms (n=158)

... exit code 3 (--fail-on-regression)
```

Every surfaced finding corresponds to a regression the generator deliberately
injected (worse cyclist heading, more missed far-range LEVEL_2 pedestrians,
low-visibility degradation and slower low-visibility latency), which is exactly
what the triage is supposed to localise.

### Determinism

Running `evaluate` with 1 thread vs. 8 threads produces reports that are
byte-identical except for the thread-count metadata field:

```sh
diff <(sed -E 's/"threads": *[0-9]+/"threads": N/' reports_t1/eval_candidate.json) \
     <(sed -E 's/"threads": *[0-9]+/"threads": N/' reports_t8/eval_candidate.json)
# (no output -> identical)
```

---

## Repository layout

```
perception-eval-platform/
  README.md                 this file
  DESIGN.md                 metric definitions and engineering choices
  ROADMAP.md                planned work
  LICENSE                   MIT
  Makefile                  reference build (clang++, no CMake needed)
  CMakeLists.txt            portable build + CTest
  include/                  headers (geometry, taxonomy, metrics, evaluator,
                            tracking, triage, report, generator, dataset,
                            thread_pool, io_util, json.hpp)
  src/                      implementations + main.cpp (the pev CLI)
  tests/                    test_main.cpp (unit + integration suite)
  examples/                 committed small dataset + reports + dashboard.html
  docs/architecture.md      module graph, data-flow, determinism model
  tools/wod_to_platform.py  documented real-WOD -> pev JSON converter stub
```

## CLI reference

| Command | Purpose |
| --- | --- |
| `generate` | Create a synthetic WOD-shaped dataset (ground truth + baseline + candidate runs with injected regressions). |
| `evaluate` | Detection AP/APH per class, range, difficulty, scene tag + overall. |
| `track` | CLEAR-MOT tracking metrics (MOTA/MOTP/IDSW/FRAG/MT/PT/ML). |
| `compare` | Slice-based regression triage between two runs. `--fail-on-regression` -> exit 3. |
| `dashboard` | Re-render the self-contained HTML dashboard from a saved compare JSON. |

Exit codes: `0` success, `2` usage error, `1` runtime error, `3` regressions
found (only with `--fail-on-regression`).

## License

MIT, (c) 2026 Aditya Kopalli. See `LICENSE`.
