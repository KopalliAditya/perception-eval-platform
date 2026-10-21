# Detection evaluation: `baseline`

- IoU threshold: **0.50**
- Threads: **4**
- Scenes / frames: **6** / **24**

## Overall

| metric | value |
|---|---:|
| AP | 0.681 |
| APH | 0.667 |
| precision | 0.809 |
| recall | 0.774 |
| F1 | 0.791 |
| TP / FP / FN | 212 / 50 / 62 |
| latency p95 (ms) | 28.47 |

## AP / APH by type

| type | numGT | AP | APH | precision | recall | F1 |
|---|---:|---:|---:|---:|---:|---:|
| TYPE_VEHICLE | 154 | 0.942 | 0.922 | 0.993 | 0.942 | 0.967 |
| TYPE_PEDESTRIAN | 49 | 0.344 | 0.337 | 0.522 | 0.490 | 0.505 |
| TYPE_CYCLIST | 37 | 0.725 | 0.714 | 0.861 | 0.838 | 0.849 |
| TYPE_SIGN | 34 | 0.175 | 0.171 | 0.353 | 0.353 | 0.353 |

## AP / APH by type x range

| type | range | numGT | AP | APH | recall |
|---|---|---:|---:|---:|---:|
| TYPE_VEHICLE | [0,30)m | 60 | 0.933 | 0.916 | 0.933 |
| TYPE_VEHICLE | [30,50)m | 51 | 0.961 | 0.941 | 0.961 |
| TYPE_VEHICLE | [50,inf)m | 43 | 0.930 | 0.911 | 0.930 |
| TYPE_PEDESTRIAN | [0,30)m | 20 | 0.597 | 0.585 | 0.650 |
| TYPE_PEDESTRIAN | [30,50)m | 13 | 0.353 | 0.344 | 0.538 |
| TYPE_PEDESTRIAN | [50,inf)m | 16 | 0.083 | 0.082 | 0.250 |
| TYPE_CYCLIST | [0,30)m | 10 | 0.810 | 0.798 | 0.900 |
| TYPE_CYCLIST | [30,50)m | 16 | 0.613 | 0.605 | 0.750 |
| TYPE_CYCLIST | [50,inf)m | 11 | 0.868 | 0.850 | 0.909 |
| TYPE_SIGN | [0,30)m | 12 | 0.333 | 0.326 | 0.500 |
| TYPE_SIGN | [30,50)m | 12 | 0.214 | 0.209 | 0.333 |
| TYPE_SIGN | [50,inf)m | 10 | 0.072 | 0.071 | 0.200 |

## AP / APH by type x difficulty

| type | difficulty | numGT | AP | APH | recall |
|---|---|---:|---:|---:|---:|
| TYPE_VEHICLE | LEVEL_1 | 152 | 0.941 | 0.922 | 0.941 |
| TYPE_VEHICLE | LEVEL_2 | 2 | 1.000 | 0.988 | 1.000 |
| TYPE_PEDESTRIAN | LEVEL_1 | 33 | 0.430 | 0.420 | 0.606 |
| TYPE_PEDESTRIAN | LEVEL_2 | 16 | 0.042 | 0.041 | 0.250 |
| TYPE_CYCLIST | LEVEL_1 | 30 | 0.718 | 0.708 | 0.833 |
| TYPE_CYCLIST | LEVEL_2 | 7 | 0.468 | 0.454 | 0.857 |
| TYPE_SIGN | LEVEL_1 | 23 | 0.217 | 0.212 | 0.435 |
| TYPE_SIGN | LEVEL_2 | 11 | 0.018 | 0.018 | 0.182 |

## Latency by scene tag

| tag | frames | mean | p50 | p95 |
|---|---:|---:|---:|---:|
| (any) | 24 | 15.46 | 13.95 | 28.47 |
| day_clear | 8 | 13.26 | 13.37 | 15.17 |
| fog | 4 | 18.78 | 15.51 | 29.14 |
| highway | 4 | 13.77 | 13.68 | 15.17 |
| low_visibility | 16 | 16.56 | 14.62 | 29.14 |
| night | 8 | 16.82 | 14.18 | 28.47 |
| rain | 4 | 13.81 | 12.64 | 15.54 |
| suburban | 4 | 12.75 | 12.44 | 13.50 |
| urban | 16 | 16.56 | 14.62 | 29.14 |
