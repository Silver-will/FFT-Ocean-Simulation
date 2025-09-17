#include "include/vk_engine.h"
#include "include/Renderers/fft_renderer.h"
#include <memory>
using namespace std;


int main(int argc, char* argv[])
{
	auto engine = std::make_shared<VulkanEngine>();

	std::unique_ptr<BaseRenderer> FFTOceanSimulation = std::make_unique<FFTRenderer>();
	FFTOceanSimulation->Init(engine.get());
	FFTOceanSimulation->Run();
	FFTOceanSimulation->Cleanup();
	engine->cleanup();

	return 0;
}
