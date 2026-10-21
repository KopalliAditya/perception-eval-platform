#include "geometry.hpp"

#include <algorithm>
#include <cmath>

namespace pev {

namespace {

constexpr double kPi = 3.14159265358979323846;

// Cross product of (b - a) and (p - a); its sign tells which side of the
// directed line a->b the point p lies on. > 0 means to the left.
double cross(const Vec2& a, const Vec2& b, const Vec2& p) {
    return (b.x - a.x) * (p.y - a.y) - (b.y - a.y) * (p.x - a.x);
}

// Intersection point of segment s->e with the infinite line through a->b.
// The segment is assumed to actually cross the line (guarded by the caller).
Vec2 line_intersection(const Vec2& s, const Vec2& e, const Vec2& a,
                       const Vec2& b) {
    const double a1 = b.y - a.y;
    const double b1 = a.x - b.x;
    const double c1 = a1 * a.x + b1 * a.y;

    const double a2 = e.y - s.y;
    const double b2 = s.x - e.x;
    const double c2 = a2 * s.x + b2 * s.y;

    const double det = a1 * b2 - a2 * b1;
    if (std::fabs(det) < 1e-12) {
        return e;  // (near-)parallel; caller only calls us on a real crossing
    }
    Vec2 out;
    out.x = (b2 * c1 - b1 * c2) / det;
    out.y = (a1 * c2 - a2 * c1) / det;
    return out;
}

}  // namespace

double Box3D::range() const { return std::hypot(cx, cy); }

double Box3D::bev_area() const {
    return (length <= 0.0 || width <= 0.0) ? 0.0 : length * width;
}

double Box3D::volume() const {
    return (length <= 0.0 || width <= 0.0 || height <= 0.0)
               ? 0.0
               : length * width * height;
}

std::array<Vec2, 4> Box3D::bev_corners() const {
    const double c = std::cos(heading);
    const double s = std::sin(heading);
    const double hl = length / 2.0;  // half-length along heading
    const double hw = width / 2.0;   // half-width perpendicular to heading

    // Corners in the box-local frame, counter-clockwise, then rotated by the
    // heading and translated to the center.
    const std::array<Vec2, 4> local = {Vec2{hl, hw}, Vec2{-hl, hw},
                                        Vec2{-hl, -hw}, Vec2{hl, -hw}};
    std::array<Vec2, 4> out;
    for (std::size_t i = 0; i < 4; ++i) {
        out[i].x = cx + local[i].x * c - local[i].y * s;
        out[i].y = cy + local[i].x * s + local[i].y * c;
    }
    return out;
}

double signed_polygon_area(const std::vector<Vec2>& poly) {
    if (poly.size() < 3) {
        return 0.0;
    }
    double acc = 0.0;
    for (std::size_t i = 0; i < poly.size(); ++i) {
        const Vec2& p = poly[i];
        const Vec2& q = poly[(i + 1) % poly.size()];
        acc += p.x * q.y - q.x * p.y;
    }
    return acc / 2.0;
}

double polygon_area(const std::vector<Vec2>& poly) {
    return std::fabs(signed_polygon_area(poly));
}

std::vector<Vec2> clip_polygon(const std::vector<Vec2>& subject,
                               const std::vector<Vec2>& clip) {
    if (subject.empty() || clip.size() < 3) {
        return {};
    }
    // Ensure the clip polygon is counter-clockwise so "inside" == left side.
    std::vector<Vec2> clip_ccw = clip;
    if (signed_polygon_area(clip_ccw) < 0.0) {
        std::reverse(clip_ccw.begin(), clip_ccw.end());
    }

    std::vector<Vec2> output = subject;
    const std::size_t m = clip_ccw.size();
    for (std::size_t i = 0; i < m && !output.empty(); ++i) {
        const Vec2& a = clip_ccw[i];
        const Vec2& b = clip_ccw[(i + 1) % m];
        const std::vector<Vec2> input = output;
        output.clear();

        for (std::size_t j = 0; j < input.size(); ++j) {
            const Vec2& cur = input[j];
            const Vec2& prev = input[(j + input.size() - 1) % input.size()];
            // >= 0 keeps points on the edge (robust to touching boxes).
            const bool cur_in = cross(a, b, cur) >= 0.0;
            const bool prev_in = cross(a, b, prev) >= 0.0;

            if (cur_in) {
                if (!prev_in) {
                    output.push_back(line_intersection(prev, cur, a, b));
                }
                output.push_back(cur);
            } else if (prev_in) {
                output.push_back(line_intersection(prev, cur, a, b));
            }
        }
    }
    return output;
}

double convex_intersection_area(const std::vector<Vec2>& a,
                                const std::vector<Vec2>& b) {
    const std::vector<Vec2> clipped = clip_polygon(a, b);
    return polygon_area(clipped);
}

double bev_intersection_area(const Box3D& a, const Box3D& b) {
    const std::array<Vec2, 4> ca = a.bev_corners();
    const std::array<Vec2, 4> cb = b.bev_corners();
    const std::vector<Vec2> pa(ca.begin(), ca.end());
    const std::vector<Vec2> pb(cb.begin(), cb.end());
    return convex_intersection_area(pa, pb);
}

double iou_bev(const Box3D& a, const Box3D& b) {
    const double inter = bev_intersection_area(a, b);
    if (inter <= 0.0) {
        return 0.0;
    }
    const double uni = a.bev_area() + b.bev_area() - inter;
    return uni > 0.0 ? inter / uni : 0.0;
}

double iou_3d(const Box3D& a, const Box3D& b) {
    const double bev = bev_intersection_area(a, b);
    if (bev <= 0.0) {
        return 0.0;
    }
    const double z_lo = std::max(a.z_min(), b.z_min());
    const double z_hi = std::min(a.z_max(), b.z_max());
    const double z_overlap = z_hi - z_lo;
    if (z_overlap <= 0.0) {
        return 0.0;
    }
    const double inter_vol = bev * z_overlap;
    const double uni_vol = a.volume() + b.volume() - inter_vol;
    return uni_vol > 0.0 ? inter_vol / uni_vol : 0.0;
}

double heading_error(double a, double b) {
    double d = a - b;
    // Reduce to (-pi, pi].
    d = std::fmod(d, 2.0 * kPi);
    if (d < 0.0) {
        d += 2.0 * kPi;
    }
    // d is now in [0, 2pi); fold to [0, pi].
    if (d > kPi) {
        d = 2.0 * kPi - d;
    }
    return d;
}

double heading_accuracy(double a, double b) {
    const double err = heading_error(a, b);
    double acc = 1.0 - err / kPi;
    if (acc < 0.0) acc = 0.0;
    if (acc > 1.0) acc = 1.0;
    return acc;
}

}  // namespace pev
