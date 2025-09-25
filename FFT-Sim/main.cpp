#include "include/vk_engine.h"
#include "include/Renderers/fft_renderer.h"
#include <memory>
#include <iostream>
#include "sim_utils.h"
using namespace std;


int main(int argc, char* argv[])
{
	auto engine = std::make_shared<VulkanEngine>();

	auto FFTOceanSimulation = std::make_unique<FFTRenderer>();
	FFTOceanSimulation->Init(engine.get());

	std::cout << getHeight(0.3, 12, 5, FFTOceanSimulation.get()) << std::endl;
	//FFTOceanSimulation->Run();
	FFTOceanSimulation->Cleanup();
	engine->cleanup();

	return 0;
}
