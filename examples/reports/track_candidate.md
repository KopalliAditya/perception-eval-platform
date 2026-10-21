# Tracking evaluation: `candidate`

- IoU threshold: **0.50**
- Threads: **4**

| type | MOTA | MOTP | IDSW | FRAG | MT | PT | ML | GT tracks | matches | misses | FP |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| TYPE_VEHICLE | 0.753 | 0.813 | 25 | 6 | 33 | 10 | 0 | 43 | 142 | 12 | 1 |
| TYPE_PEDESTRIAN | 0.082 | 0.659 | 0 | 4 | 1 | 10 | 4 | 15 | 23 | 26 | 19 |
| TYPE_CYCLIST | 0.459 | 0.652 | 0 | 0 | 4 | 5 | 1 | 10 | 25 | 12 | 8 |
| TYPE_SIGN | -0.324 | 0.602 | 0 | 2 | 0 | 8 | 1 | 9 | 11 | 23 | 22 |
| **ALL** | 0.460 | 0.764 | 25 | 12 | 38 | 33 | 6 | 77 | 201 | 73 | 50 |
