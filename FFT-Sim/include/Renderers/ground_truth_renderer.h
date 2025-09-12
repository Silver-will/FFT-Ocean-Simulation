#ifndef GROUND_TRUTH_RENDERER
#define GROUND_TRUTH_RENDERER

#include "base_renderer.h"
#include "../vk_engine.h"

struct GroundTruthRenderer : public BaseRenderer
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
	void BuildBVHStructure();

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
	DrawContext drawCommands;
	DrawContext skyDrawCommands;

	VkDescriptorSetLayout trace_descriptor_layout;
	VkDescriptorSetLayout post_process_descriptor_layout;
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
	VkExtent2D _windowExtent{ 1920,1080 };
	float bloom_filter_radius = 0.005f;
	float bloom_strength = 0.08f;
	float _aspect_width = 1920;
	float _aspect_height = 1080;

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

	PipelineStateObject trace_rays_pso;

	GPUSceneData scene_data;

	AllocatedImage white_image;
	AllocatedImage black_image;
	AllocatedImage grey_image;
	AllocatedImage storage_image;
	AllocatedImage errorCheckerboardImage;

	AllocatedImage _skyImage;
	ktxVulkanTexture _skyBoxImage;

	std::unordered_map<std::string, std::shared_ptr<LoadedGLTF>> loadedScenes;
	VkSampler defaultSamplerLinear;
	VkSampler defaultSamplerNearest;
	VkSampler cubeMapSampler;

	EngineStats stats;
	std::vector<VkBufferMemoryBarrier> compute_barriers;

	//lights
	DirectionalLight directLight;
	uint32_t maxLights = 100;

	struct PointLightData {

		uint32_t numOfLights = 6;
		std::vector<PointLight> pointLights;

	}pointData;

	std::string assets_path;
};

#endif