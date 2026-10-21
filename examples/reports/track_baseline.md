# Tracking evaluation: `baseline`

- IoU threshold: **0.50**
- Threads: **4**

| type | MOTA | MOTP | IDSW | FRAG | MT | PT | ML | GT tracks | matches | misses | FP |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| TYPE_VEHICLE | 0.935 | 0.812 | 0 | 5 | 35 | 8 | 0 | 43 | 145 | 9 | 1 |
| TYPE_PEDESTRIAN | 0.041 | 0.665 | 0 | 4 | 1 | 11 | 3 | 15 | 24 | 25 | 22 |
| TYPE_CYCLIST | 0.703 | 0.722 | 0 | 1 | 6 | 4 | 0 | 10 | 31 | 6 | 5 |
| TYPE_SIGN | -0.294 | 0.598 | 0 | 3 | 0 | 8 | 1 | 9 | 12 | 22 | 22 |
| **ALL** | 0.591 | 0.770 | 0 | 13 | 42 | 31 | 4 | 77 | 212 | 62 | 50 |
