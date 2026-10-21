#include "assignment.hpp"

#include <limits>
#include <vector>

namespace pev {

std::vector<int> solve_assignment(
    const std::vector<std::vector<double>>& cost) {
    const int n = static_cast<int>(cost.size());
    if (n == 0) {
        return {};
    }

    constexpr double kInf = std::numeric_limits<double>::infinity();

    // 1-indexed potentials and bookkeeping (classic O(n^3) formulation).
    std::vector<double> u(static_cast<std::size_t>(n) + 1, 0.0);
    std::vector<double> v(static_cast<std::size_t>(n) + 1, 0.0);
    std::vector<int> p(static_cast<std::size_t>(n) + 1, 0);    // col -> row
    std::vector<int> way(static_cast<std::size_t>(n) + 1, 0);  // augmenting path

    for (int i = 1; i <= n; ++i) {
        p[0] = i;
        int j0 = 0;
        std::vector<double> minv(static_cast<std::size_t>(n) + 1, kInf);
        std::vector<char> used(static_cast<std::size_t>(n) + 1, 0);
        do {
            used[static_cast<std::size_t>(j0)] = 1;
            const int i0 = p[static_cast<std::size_t>(j0)];
            double delta = kInf;
            int j1 = -1;
            for (int j = 1; j <= n; ++j) {
                if (used[static_cast<std::size_t>(j)]) {
                    continue;
                }
                const double cur =
                    cost[static_cast<std::size_t>(i0 - 1)]
                        [static_cast<std::size_t>(j - 1)] -
                    u[static_cast<std::size_t>(i0)] -
                    v[static_cast<std::size_t>(j)];
                if (cur < minv[static_cast<std::size_t>(j)]) {
                    minv[static_cast<std::size_t>(j)] = cur;
                    way[static_cast<std::size_t>(j)] = j0;
                }
                if (minv[static_cast<std::size_t>(j)] < delta) {
                    delta = minv[static_cast<std::size_t>(j)];
                    j1 = j;
                }
            }
            for (int j = 0; j <= n; ++j) {
                if (used[static_cast<std::size_t>(j)]) {
                    u[static_cast<std::size_t>(p[static_cast<std::size_t>(j)])] +=
                        delta;
                    v[static_cast<std::size_t>(j)] -= delta;
                } else {
                    minv[static_cast<std::size_t>(j)] -= delta;
                }
            }
            j0 = j1;
        } while (p[static_cast<std::size_t>(j0)] != 0);

        // Unwind the augmenting path.
        do {
            const int j1 = way[static_cast<std::size_t>(j0)];
            p[static_cast<std::size_t>(j0)] = p[static_cast<std::size_t>(j1)];
            j0 = j1;
        } while (j0 != 0);
    }

    std::vector<int> assign(static_cast<std::size_t>(n), -1);
    for (int j = 1; j <= n; ++j) {
        const int row = p[static_cast<std::size_t>(j)];
        if (row >= 1 && row <= n) {
            assign[static_cast<std::size_t>(row - 1)] = j - 1;
        }
    }
    return assign;
}

}  // namespace pev
