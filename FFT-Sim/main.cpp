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
	float x = 0.13;
	float y = 12;
	float t = 5;
	std::cout << "Your Height value at Point ("<<x<<", " <<y<<")"<<"= \n" << getHeight(0.3, 12, 5, FFTOceanSimulation.get()) << std::endl;
	FFTOceanSimulation->Run();
	FFTOceanSimulation->Cleanup();
	engine->cleanup();

	return 0;
}
