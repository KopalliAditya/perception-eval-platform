# Slice regression triage

Baseline **`baseline`** vs candidate **`candidate`**

- IoU threshold: **0.50**
- Metric tolerance: **0.030**, latency tolerance: **15%**
- Slices compared: **38**, regressions flagged: **27**

## Aggregate

| metric | baseline | candidate | delta |
|---|---:|---:|---:|
| AP | 0.681 | 0.645 | -0.037 |
| APH | 0.667 | 0.623 | -0.044 |
| recall | 0.774 | 0.734 | -0.040 |
| precision | 0.809 | 0.801 | -0.008 |
| p95 latency (ms) | 28.47 | 48.40 | +19.93 |

## Top regressing slices

| # | slice | n | dRecall | dAPH | dAP | dPrec | dp95 ms |
|---:|---|---:|---:|---:|---:|---:|---:|
| 1 | TYPE_CYCLIST @ [30,50)m, LEVEL_1, night | 12 | -0.167 | -0.274 | -0.224 | -0.109 | +19.9 |
| 2 | TYPE_VEHICLE @ [30,50)m, LEVEL_1, low_visibility | 35 | -0.057 | -0.056 | -0.057 | +0.000 | +20.4 |
| 3 | TYPE_VEHICLE @ [30,50)m, LEVEL_1, urban | 35 | -0.057 | -0.056 | -0.057 | +0.000 | +20.4 |
| 4 | TYPE_CYCLIST @ [30,50)m, LEVEL_1, low_visibility | 16 | -0.125 | -0.168 | -0.132 | -0.086 | +20.4 |
| 5 | TYPE_CYCLIST @ [30,50)m, LEVEL_1, urban | 16 | -0.125 | -0.168 | -0.132 | -0.086 | +20.4 |
| 6 | TYPE_VEHICLE @ [30,50)m, LEVEL_1, night | 14 | -0.143 | -0.140 | -0.143 | +0.000 | +19.9 |
| 7 | TYPE_VEHICLE @ [0,30)m, LEVEL_1, low_visibility | 29 | -0.034 | -0.034 | -0.034 | -0.001 | +20.4 |
| 8 | TYPE_VEHICLE @ [0,30)m, LEVEL_1, urban | 29 | -0.034 | -0.034 | -0.034 | -0.001 | +20.4 |
| 9 | TYPE_VEHICLE @ [50,inf)m, LEVEL_1, low_visibility | 39 | +0.000 | +0.000 | +0.000 | +0.000 | +20.4 |
| 10 | TYPE_VEHICLE @ [50,inf)m, LEVEL_1, urban | 39 | +0.000 | +0.000 | +0.000 | +0.000 | +20.4 |
| 11 | TYPE_VEHICLE @ [0,30)m, LEVEL_1, night | 9 | -0.111 | -0.111 | -0.111 | -0.014 | +19.9 |
| 12 | TYPE_VEHICLE @ [50,inf)m, LEVEL_1, night | 20 | +0.000 | +0.000 | +0.000 | +0.000 | +19.9 |
| 13 | TYPE_PEDESTRIAN @ [50,inf)m, LEVEL_2, low_visibility | 12 | -0.083 | -0.024 | -0.025 | -0.033 | +20.4 |
| 14 | TYPE_PEDESTRIAN @ [50,inf)m, LEVEL_2, urban | 12 | -0.083 | -0.024 | -0.025 | -0.033 | +20.4 |
| 15 | TYPE_PEDESTRIAN @ [50,inf)m, LEVEL_2, night | 10 | -0.100 | -0.014 | -0.014 | -0.125 | +19.9 |
