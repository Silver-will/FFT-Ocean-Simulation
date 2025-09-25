/**
 * @file simulation_utils.h
 * @brief Utility functions for finite element mesh generation and wave height calculations.
 *
 * This module provides helper functions to:
 * - Compute surface height at a given (x, y, t) location.
 * - Generate FEM element geometry for a cube.
 * - Calculate wave heights at element vertices.
 */

#pragma once
#include <Eigen/Dense>
#include "Renderers/fft_renderer.h"

/**
 * @brief Compute the surface height at a given (x, y, t) location.
 *
 * Placeholder function — replace with actual implementation.
 *
 * @param[in] x X-coordinate of the point.
 * @param[in] y Y-coordinate of the point.
 * @param[in] t Simulation time.
 * @return double Height value (currently returns 0.0).
 */
double getHeight(const double x, const double y, const double t, FFTRenderer* fft_simulator);

/**
 * @brief Create a finite element representation of a 1x1x1 m cube.
 *
 * Generates a cube centered at the origin {0, 0, 0} with edge length 1.0 m.
 * The output matrix contains the vertices of the cube.
 *
 * @return Eigen::MatrixXd A 3x8 matrix where each column is a vertex of the cube.
 */
Eigen::MatrixXd createFEMElements();

/**
 * @brief Calculate wave heights at the vertices of a given element.
 *
 * Iterates over each vertex of the provided element and computes the
 * corresponding wave height using the @ref getHeight function.
 *
 * @param[in] elementVertices A 3xN matrix of vertex coordinates (each column is a vertex).
 * @param[in] simTime Simulation time at which to evaluate the wave heights.
 * @return Eigen::VectorXd A vector of wave heights, one for each vertex.
 */
Eigen::VectorXd calculateWaveHeights (const Eigen::MatrixXd& elementVertices, const double simTime, FFTRenderer* fft_simulator);


void check_sim();