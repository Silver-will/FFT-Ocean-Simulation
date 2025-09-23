#include "sim_utils.h"

double getHeight(const double /*x*/, const double /*y*/, const double /*t*/)
{
    double z = 0.0;
    return z;
}

Eigen::MatrixXd createFEMElements()
{
    Eigen::MatrixXd elementVertices(3, 8);
    const double l = 1.0 / 2;

    elementVertices <<   l,  l, -l, -l,  l,  l, -l, -l,
                        -l,  l,  l, -l, -l,  l,  l, -l,
                        -l, -l, -l, -l,  l,  l,  l,  l;

    return elementVertices;
}

Eigen::VectorXd calculateWaveHeights (const Eigen::MatrixXd& elementVertices, const double simTime)
{
    Eigen::VectorXd waveHeights (elementVertices.cols());       // Number of wave height data points == number of vertices

    for (unsigned int col = 0; col < elementVertices.cols(); col++)
    {
        const Eigen::Vector3d vertex = elementVertices.col(col);
        waveHeights[col] = getHeight(vertex.x(), vertex.y(), simTime);
    }

    return waveHeights;
}