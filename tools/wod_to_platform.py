#!/usr/bin/env python3
"""wod_to_platform.py -- convert the real Waymo Open Dataset into the JSON
format consumed by the `pev` C++ platform.

====================================================================
THIS IS A DOCUMENTED STUB. It is intentionally NOT run as part of the
platform's build or tests, and the C++ code does not depend on it.
====================================================================

The platform ships with a SYNTHETIC, WOD-shaped dataset produced by
`pev generate`. That synthetic data is NOT the real Waymo Open Dataset -- it
only mirrors its taxonomy, difficulty levels, range bins and 3D oriented-box
label shape. The real dataset is license-gated (https://waymo.com/open/) and
is distributed as TFRecord shards of protobuf messages that require TensorFlow
and the `waymo-open-dataset` Python package to read; that toolchain is
Linux/GPU-oriented and is not available in this project's build environment.

This script documents EXACTLY how a user who HAS downloaded the real dataset
would convert it so that `pev evaluate|track|compare` can run on it unchanged.
The bodies that touch TensorFlow are left as clearly-marked TODOs so the file
imports and `--help`s even where `waymo-open-dataset` is not installed.

Target output layout (identical to `pev generate`):

    <out>/ground_truth.json      # pev.ground_truth.v1
    <out>/runs/<model>.json      # pev.run.v1

Ground-truth schema (pev.ground_truth.v1):
    {
      "schema": "pev.ground_truth.v1",
      "scenes": [
        {
          "id": "<segment context name>",
          "tags": ["day_clear"|"night"|"rain"|"fog"|"low_visibility"|
                   "urban"|"highway"|"suburban", ...],
          "frames": [
            {
              "index": <int>,
              "objects": [
                {
                  "type": "TYPE_VEHICLE"|"TYPE_PEDESTRIAN"|
                          "TYPE_CYCLIST"|"TYPE_SIGN",
                  "num_points": <int>,     # LiDAR points in box -> difficulty
                  "track_id": <int>,       # stable id across frames in a scene
                  "box": {"cx","cy","cz","length","width","height","heading"}
                }, ...
              ]
            }, ...
          ]
        }, ...
      ]
    }

Run schema (pev.run.v1) mirrors the above but each detection also carries a
"score" (float) and omits "num_points"; each frame carries a "latency_ms".

Usage (on a machine with the real data + waymo-open-dataset installed):

    python3 tools/wod_to_platform.py \
        --segments /path/to/waymo/*.tfrecord \
        --out data/ \
        --predictions /path/to/your/model_predictions.bin \
        --model my_detector

Author: Aditya Kopalli, 2026. MIT licensed.
"""

import argparse
import json
import os
import sys

# Map Waymo label type enum -> platform type string. The WOD Label.Type enum is
# {0: UNKNOWN, 1: VEHICLE, 2: PEDESTRIAN, 3: SIGN, 4: CYCLIST}.
WOD_TYPE_TO_PEV = {
    1: "TYPE_VEHICLE",
    2: "TYPE_PEDESTRIAN",
    3: "TYPE_SIGN",
    4: "TYPE_CYCLIST",
}

# The LEVEL_1 / LEVEL_2 split the platform derives from num_points. Kept in sync
# with pev::kLevel1MinPoints in include/taxonomy.hpp.
LEVEL1_MIN_POINTS = 50


def derive_scene_tags(frame) -> list:
    """Map a WOD frame's context.stats (weather, time_of_day, location) onto the
    platform's closed tag set. TODO: read frame.context.stats once TensorFlow
    and the waymo-open-dataset protos are available."""
    # Reference implementation sketch:
    #   stats = frame.context.stats
    #   tags = []
    #   tod = stats.time_of_day          # "Day" | "Night" | "Dawn/Dusk"
    #   weather = stats.weather          # "sunny" | "rain" | ...
    #   tags.append("night" if tod == "Night" else "day_clear")
    #   if weather == "rain": tags.append("rain")
    #   ... + location_type -> urban/highway/suburban
    #   if {"night","rain","fog"} & set(tags): tags.append("low_visibility")
    raise NotImplementedError(
        "derive_scene_tags requires the waymo-open-dataset protos")


def convert_segments(segment_paths, out_dir):
    """Read WOD TFRecord segments and emit <out>/ground_truth.json.

    TODO (requires `import tensorflow as tf` and
    `from waymo_open_dataset import dataset_pb2`):

        scenes = []
        for path in segment_paths:
            ds = tf.data.TFRecordDataset(path, compression_type="")
            frames = []
            context_name = None
            for i, record in enumerate(ds):
                frame = dataset_pb2.Frame()
                frame.ParseFromString(bytes(record.numpy()))
                context_name = frame.context.name
                objects = []
                for label in frame.laser_labels:
                    t = WOD_TYPE_TO_PEV.get(label.type)
                    if t is None:
                        continue
                    b = label.box
                    objects.append({
                        "type": t,
                        "num_points": label.num_lidar_points_in_box,
                        "track_id": _stable_track_id(label.id),
                        "box": {
                            "cx": b.center_x, "cy": b.center_y, "cz": b.center_z,
                            "length": b.length, "width": b.width,
                            "height": b.height, "heading": b.heading,
                        },
                    })
                frames.append({"index": i, "objects": objects})
            scenes.append({
                "id": context_name,
                "tags": derive_scene_tags(frame),
                "frames": frames,
            })
        _write_json(os.path.join(out_dir, "ground_truth.json"),
                    {"schema": "pev.ground_truth.v1", "scenes": scenes})
    """
    raise NotImplementedError(
        "convert_segments requires TensorFlow + waymo-open-dataset; this is a "
        "documented stub. See the module docstring for the target JSON schema.")


def convert_predictions(prediction_path, gt_scenes, out_dir, model_name):
    """Convert a model's predictions (e.g. a WOD submission .bin of
    Objects protos) into <out>/runs/<model>.json, aligned to gt_scenes.

    TODO (requires `from waymo_open_dataset.protos import metrics_pb2`):
        objects = metrics_pb2.Objects()
        objects.ParseFromString(open(prediction_path, "rb").read())
        # group objects by (context_name, frame_timestamp), emit detections
        # with box + score + track_id, then write pev.run.v1 JSON.
    """
    raise NotImplementedError(
        "convert_predictions requires the waymo-open-dataset protos; stub.")


def _write_json(path, obj):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "w", encoding="utf-8") as fh:
        json.dump(obj, fh, sort_keys=True, separators=(",", ":"))


def main(argv=None) -> int:
    parser = argparse.ArgumentParser(
        description="Convert the real Waymo Open Dataset to the pev JSON "
                    "format (documented stub; not used by the build).")
    parser.add_argument("--segments", nargs="+", default=[],
                        help="TFRecord segment files (the real WOD).")
    parser.add_argument("--predictions",
                        help="Optional model-prediction file to convert.")
    parser.add_argument("--model", default="candidate",
                        help="Run name for the converted predictions.")
    parser.add_argument("--out", required=True,
                        help="Output data directory.")
    args = parser.parse_args(argv)

    print("wod_to_platform.py is a DOCUMENTED STUB.", file=sys.stderr)
    print("The platform ships synthetic, WOD-shaped data via `pev generate`.",
          file=sys.stderr)
    print("Converting the REAL Waymo Open Dataset requires TensorFlow and the "
          "waymo-open-dataset package (Linux/GPU).", file=sys.stderr)
    print("See the module docstring for the exact target JSON schema.",
          file=sys.stderr)

    try:
        convert_segments(args.segments, args.out)
        if args.predictions:
            convert_predictions(args.predictions, None, args.out, args.model)
    except NotImplementedError as exc:
        print(f"\nnot implemented: {exc}", file=sys.stderr)
        return 2
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
