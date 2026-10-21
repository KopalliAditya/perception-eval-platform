# Detection evaluation: `candidate`

- IoU threshold: **0.50**
- Threads: **4**
- Scenes / frames: **6** / **24**

## Overall

| metric | value |
|---|---:|
| AP | 0.645 |
| APH | 0.623 |
| precision | 0.801 |
| recall | 0.734 |
| F1 | 0.766 |
| TP / FP / FN | 201 / 50 / 73 |
| latency p95 (ms) | 48.40 |

## AP / APH by type

| type | numGT | AP | APH | precision | recall | F1 |
|---|---:|---:|---:|---:|---:|---:|
| TYPE_VEHICLE | 154 | 0.922 | 0.903 | 0.993 | 0.922 | 0.956 |
| TYPE_PEDESTRIAN | 49 | 0.341 | 0.334 | 0.548 | 0.469 | 0.505 |
| TYPE_CYCLIST | 37 | 0.557 | 0.488 | 0.758 | 0.676 | 0.714 |
| TYPE_SIGN | 34 | 0.157 | 0.154 | 0.333 | 0.324 | 0.328 |

## AP / APH by type x range

| type | range | numGT | AP | APH | recall |
|---|---|---:|---:|---:|---:|
| TYPE_VEHICLE | [0,30)m | 60 | 0.917 | 0.900 | 0.917 |
| TYPE_VEHICLE | [30,50)m | 51 | 0.922 | 0.902 | 0.922 |
| TYPE_VEHICLE | [50,inf)m | 43 | 0.930 | 0.911 | 0.930 |
| TYPE_PEDESTRIAN | [0,30)m | 20 | 0.597 | 0.585 | 0.650 |
| TYPE_PEDESTRIAN | [30,50)m | 13 | 0.353 | 0.344 | 0.538 |
| TYPE_PEDESTRIAN | [50,inf)m | 16 | 0.077 | 0.076 | 0.188 |
| TYPE_CYCLIST | [0,30)m | 10 | 0.810 | 0.686 | 0.900 |
| TYPE_CYCLIST | [30,50)m | 16 | 0.480 | 0.437 | 0.625 |
| TYPE_CYCLIST | [50,inf)m | 11 | 0.477 | 0.410 | 0.545 |
| TYPE_SIGN | [0,30)m | 12 | 0.269 | 0.263 | 0.417 |
| TYPE_SIGN | [30,50)m | 12 | 0.214 | 0.209 | 0.333 |
| TYPE_SIGN | [50,inf)m | 10 | 0.072 | 0.071 | 0.200 |

## AP / APH by type x difficulty

| type | difficulty | numGT | AP | APH | recall |
|---|---|---:|---:|---:|---:|
| TYPE_VEHICLE | LEVEL_1 | 152 | 0.921 | 0.902 | 0.921 |
| TYPE_VEHICLE | LEVEL_2 | 2 | 1.000 | 0.988 | 1.000 |
| TYPE_PEDESTRIAN | LEVEL_1 | 33 | 0.453 | 0.444 | 0.606 |
| TYPE_PEDESTRIAN | LEVEL_2 | 16 | 0.027 | 0.026 | 0.188 |
| TYPE_CYCLIST | LEVEL_1 | 30 | 0.574 | 0.505 | 0.700 |
| TYPE_CYCLIST | LEVEL_2 | 7 | 0.208 | 0.168 | 0.571 |
| TYPE_SIGN | LEVEL_1 | 23 | 0.194 | 0.190 | 0.391 |
| TYPE_SIGN | LEVEL_2 | 11 | 0.018 | 0.018 | 0.182 |

## Latency by scene tag

| tag | frames | mean | p50 | p95 |
|---|---:|---:|---:|---:|
| (any) | 24 | 23.19 | 23.46 | 48.40 |
| day_clear | 8 | 13.26 | 13.37 | 15.17 |
| fog | 4 | 31.93 | 26.37 | 49.53 |
| highway | 4 | 13.77 | 13.68 | 15.17 |
| low_visibility | 16 | 28.15 | 24.85 | 49.53 |
| night | 8 | 28.60 | 24.11 | 48.40 |
| rain | 4 | 23.48 | 21.49 | 26.42 |
| suburban | 4 | 12.75 | 12.44 | 13.50 |
| urban | 16 | 28.15 | 24.85 | 49.53 |
