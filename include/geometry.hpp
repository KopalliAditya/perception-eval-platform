// geometry.hpp — 3D oriented bounding boxes and Intersection-over-Union.
//
// The platform works in the ego-vehicle frame: the ego sits at the origin,
// +x points forward, +y points left, +z points up (right-handed), matching the
// Waymo Open Dataset vehicle frame convention.
//
// A Box3D is parameterised the same way as a WOD label: a center (cx, cy, cz),
// axis-aligned dimensions (length along the heading, width perpendicular to it,
// height along +z) and a heading (yaw) in radians about the +z axis.
//
// 3D IoU for two oriented boxes is computed as:
//     intersection_volume = bev_intersection_area * z_overlap
//     union_volume        = vol(a) + vol(b) - intersection_volume
//     iou_3d              = intersection_volume / union_volume
// where `bev_intersection_area` is the area of the intersection of the two
// rotated rectangles in the bird's-eye-view (x, y) plane, obtained by
// Sutherland–Hodgman polygon clipping, and `z_overlap` is the overlap of the
// two [z_min, z_max] intervals.
//
// Author: Aditya Kopalli, 2026. MIT licensed.

#ifndef PEV_GEOMETRY_HPP
#define PEV_GEOMETRY_HPP

#include <array>
#include <vector>

namespace pev {

// A 2D point in the bird's-eye-view (x, y) plane.
struct Vec2 {
    double x = 0.0;
    double y = 0.0;
};

// A 3D oriented bounding box in the ego/vehicle frame.
struct Box3D {
    double cx = 0.0;       // center x (forward), metres
    double cy = 0.0;       // center y (left), metres
    double cz = 0.0;       // center z (up), metres
    double length = 0.0;   // extent along heading, metres (>= 0)
    double width = 0.0;    // extent perpendicular to heading, metres (>= 0)
    double height = 0.0;   // extent along +z, metres (>= 0)
    double heading = 0.0;  // yaw about +z, radians

    // Planar distance from the ego (origin) to the box center, metres.
    double range() const;

    // Signed vertical extents of the box.
    double z_min() const { return cz - height / 2.0; }
    double z_max() const { return cz + height / 2.0; }

    // Bird's-eye-view footprint area (length * width), m^2.
    double bev_area() const;

    // Full box volume (length * width * height), m^3.
    double volume() const;

    // The four BEV corners in counter-clockwise order (for the un-rotated box
    // the order is front-left, front-right, rear-right, rear-left rotated by
    // `heading`).
    std::array<Vec2, 4> bev_corners() const;
};

// --- Low-level polygon primitives (exposed so they can be unit-tested) -------

// Signed area of a polygon via the shoelace formula; positive for a
// counter-clockwise winding. Returns 0 for fewer than three vertices.
double signed_polygon_area(const std::vector<Vec2>& poly);

// Absolute area of a (possibly concave) simple polygon.
double polygon_area(const std::vector<Vec2>& poly);

// Sutherland–Hodgman clipping of a `subject` polygon against a CONVEX `clip`
// polygon (vertices counter-clockwise). Returns the clipped polygon, which may
// be empty if there is no overlap.
std::vector<Vec2> clip_polygon(const std::vector<Vec2>& subject,
                               const std::vector<Vec2>& clip);

// Area of the intersection of two convex polygons (each counter-clockwise).
double convex_intersection_area(const std::vector<Vec2>& a,
                                const std::vector<Vec2>& b);

// --- Box-level IoU -----------------------------------------------------------

// Area of the intersection of the two boxes' BEV footprints, m^2.
double bev_intersection_area(const Box3D& a, const Box3D& b);

// Bird's-eye-view (2D, ignores z) IoU in [0, 1].
double iou_bev(const Box3D& a, const Box3D& b);

// Full 3D IoU in [0, 1].
double iou_3d(const Box3D& a, const Box3D& b);

// --- Heading helpers ---------------------------------------------------------

// Smallest absolute angular difference between two headings, wrapped into
// [0, pi]; i.e. min(|d|, 2pi - |d|) with d = a - b reduced modulo 2pi.
double heading_error(double a, double b);

// Heading accuracy in [0, 1] used by the APH metric: 1 at a perfect heading
// match, 0 when the headings are anti-parallel. Defined as
// 1 - heading_error / pi.
double heading_accuracy(double a, double b);

}  // namespace pev

#endif  // PEV_GEOMETRY_HPP
