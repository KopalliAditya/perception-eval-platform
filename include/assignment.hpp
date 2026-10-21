// assignment.hpp — the Hungarian (Kuhn–Munkres) algorithm for minimum-cost
// bipartite assignment on a square cost matrix.
//
// The tracking evaluator uses this to associate ground-truth tracks with
// detection tracks per frame optimally (rather than greedily). The solver is an
// O(n^3) shortest-augmenting-path implementation with row/column potentials.
//
// Author: Aditya Kopalli, 2026. MIT licensed.

#ifndef PEV_ASSIGNMENT_HPP
#define PEV_ASSIGNMENT_HPP

#include <vector>

namespace pev {

// Solve the minimum-cost assignment on a SQUARE cost matrix `cost` (n x n).
// Returns a vector `assign` of length n where assign[r] is the column assigned
// to row r. Every row is assigned exactly one distinct column. An empty matrix
// yields an empty result.
std::vector<int> solve_assignment(const std::vector<std::vector<double>>& cost);

}  // namespace pev

#endif  // PEV_ASSIGNMENT_HPP
