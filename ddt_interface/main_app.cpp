#include <iostream>
#include "sim_utils.h"


int main(int argc, char const *argv[])
{
    // Create FEM elements
    Eigen::MatrixXd elementVertices = createFEMElements();

    // Get height at each element vertex at given sim time
    const double simTime = 5.0;     // arbitarily chosen
    Eigen::VectorXd waveHeights = calculateWaveHeights(elementVertices, simTime);

    // Print wave heights
    std::cout << "Wave heights at t = " << simTime << ":" << std::endl;
    for (unsigned int itr = 0; itr < elementVertices.cols(); itr ++)
    {
        const Eigen::Vector3d vertex = elementVertices.col(itr);
        std::cout << "Point: (" <<  vertex.x() << "," << vertex.y() << "," << vertex.z() << ")\tHeight: " << waveHeights[itr] << std::endl;
    }
    return 0;
}
