#ifndef GROUND_TRUTH_RENDERER
#define GROUND_TRUTH_RENDERER

#include "base_renderer.h"
#include "../vk_engine.h"

struct FFTParams {
	uint32_t resolution;
	uint32_t ocean_size;
	glm::vec2 wind;
	float delta_time;
	float choppiness;
	uint32_t total_count;
	uint32_t subseq_count;
};
struct OceanVertex {
	glm::vec3 position;
	glm::vec2 uv;
};

struct OceanSurface {
	std::vector<OceanVertex> vertices;
	std::vector<uint32_t> indices;
	AllocatedImage displacement_map;

	uint32_t grid_dimensions = 1024;
	uint32_t texture_dimensions = 512;

	AllocatedImage spectrum_texture;
	AllocatedImage temp_texture;
	AllocatedImage inital_spectrum_texture;
	AllocatedImage spectrum_texture;
	AllocatedImage normal_map;
	AllocatedImage ping_phase_texture;
	AllocatedImage pong_phase_texture;
};
struct FFTRenderer : public BaseRenderer
{
	void Init(VulkanEngine* engine) override;

	void Cleanup() override;

	void Draw() override;
	void DrawUI() override;

	void Run() override;
	void UpdateScene() override;
	void LoadAssets() override;
	void InitImgui() override;

	void Trace(VkCommandBuffer cmd);
	void DrawBackground(VkCommandBuffer cmd);
	void DrawPostProcess(VkCommandBuffer cmd);
	void DrawImgui(VkCommandBuffer cmd, VkImageView targetImageView);
	void BuildOceanMesh();

	void ConfigureRenderWindow();
	void InitEngine();
	void InitCommands();
	void InitRenderTargets();
	void InitSwapchain();
	void InitComputePipelines();
	void InitDefaultData();
	void InitSyncStructures();
	void InitDescriptors();
	void InitBuffers();
	void InitPipelines();

	void CreateSwapchain(uint32_t width, uint32_t height);
	void DestroySwapchain();
	void ResizeSwapchain();

	static void KeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods);
	static void CursorCallback(GLFWwindow* window, double xpos, double ypos);
	static void FramebufferResizeCallback(GLFWwindow* window, int width, int height);

private:
	OceanSurface surface;
	DrawContext drawCommands;
	DrawContext skyDrawCommands;

	VkDescriptorSetLayout skybox_descriptor_layout;

	Camera main_camera;
	std::shared_ptr<ResourceManager> resource_manager;
	std::shared_ptr<SceneManager> scene_manager;

	VkSwapchainKHR swapchain;
	VkFormat swapchain_image_format;
	std::vector<VkImage> swapchain_images;
	std::vector<VkImageView> swapchain_image_views;
	VkExtent2D _swapchainExtent;

	MaterialInstance defaultData;
	GLTFMetallic_Roughness metalRoughMaterial;

	DescriptorAllocator globalDescriptorAllocator;
	VkDescriptorSet _drawImageDescriptors;

	std::vector<vkutil::MaterialPass> forward_passes;

	BlackKey::FrameData _frames[FRAME_OVERLAP];
	BlackKey::FrameData& get_current_frame() { return _frames[_frameNumber % FRAME_OVERLAP]; };


	bool resize_requested = false;
	bool _isInitialized{ false };
	int _frameNumber{ 0 };
	bool stop_rendering{ false };
	bool use_bindless = true;
	bool debugBuffer = false;
	bool readDebugBuffer = false;

	struct {
		float lastFrame;
	} delta;
	VkExtent2D _windowExtent{ 1280,720 };
	float bloom_filter_radius = 0.005f;
	float bloom_strength = 0.08f;
	float _aspect_width = 1280;
	float _aspect_height = 720;

	DeletionQueue _mainDeletionQueue;
	AllocatedImage _drawImage;
	AllocatedImage _depthImage;
	AllocatedImage _depthPyramid;
	std::vector<BlackKey::BloomMip> bloom_mip_maps;
	uint32_t mip_chain_length = 5;

	IBLData IBL;
	int draw_count = 0;

	VkExtent2D _drawExtent;
	float render_scale = 1.f;

	VkPipeline gradient_pipeline;
	VkPipelineLayout gradient_pipeline_layout;

	PipelineStateObject fft_horizontal_pso;
	PipelineStateObject fft_vertical_pso;
	PipelineStateObject normal_caluclation_pso;
	PipelineStateObject initial_spectrum_pso;
	PipelineStateObject spectrum_pso;
	PipelineStateObject phase_pso;
	PipelineStateObject trace_rays_pso;
	GPUSceneData scene_data;

	AllocatedImage white_image;
	AllocatedImage black_image;
	AllocatedImage grey_image;
	AllocatedImage storage_image;
	AllocatedImage errorCheckerboardImage;
	AllocatedImage _skyImage;

	VkSampler defaultSamplerLinear;
	VkSampler defaultSamplerNearest;
	VkSampler cubeMapSampler;

	EngineStats stats;
	std::vector<VkBufferMemoryBarrier> compute_barriers;

	//lights
	DirectionalLight directLight;
	std::string assets_path;

};

#endif