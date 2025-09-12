#pragma once
#include "base_renderer.h"
#include <memory>

struct ClusteredForwardRenderer : BaseRenderer
{
	void Init(VulkanEngine* engine) override;
	void Cleanup() override;

	void Draw() override;
	void DrawUI() override;
	void Run() override;

	void InitImgui() override;

	void LoadAssets() override;
	void UpdateScene() override;

	//Compute shader passes
	void PreProcessPass();
	// IBL preparation passes
	void GenerateIrradianceCube();
	void GeneratePrefilteredCubemap();
	// Light clustering building
	void BuildClusters();
	void CullLights(VkCommandBuffer cmd);
	void GenerateAABB(VkCommandBuffer cmd);
	//Bloom compute pass
	void DownSampleBloom(VkCommandBuffer cmd);
	void UpSampleBloom(VkCommandBuffer cmd);
	void ReduceDepth(VkCommandBuffer cmd);
	void ExecuteComputeCull(VkCommandBuffer cmd, vkutil::cullParams& cullParams, SceneManager::MeshPass* meshPass);


	void DrawShadows(VkCommandBuffer cmd);
	void DrawMain(VkCommandBuffer cmd);
	void DrawPostProcess(VkCommandBuffer cmd);
	void DrawBackground(VkCommandBuffer cmd);
	void DrawImgui(VkCommandBuffer cmd, VkImageView targetImageView);
	void DrawGeometry(VkCommandBuffer cmd);
	void DrawHdr(VkCommandBuffer cmd);
	void DrawEarlyDepth(VkCommandBuffer cmd);

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
	ShadowPipelineResources cascadedShadows;
	SkyBoxPipelineResources skyBoxPSO;
	BloomBlurPipelineObject postProcessPSO;
	RenderImagePipelineObject HdrPSO;
	UpsamplePipelineObject upsamplePSO;
	EarlyDepthPipelineObject depthPrePassPSO;

	DescriptorAllocator globalDescriptorAllocator;
	VkDescriptorSet _drawImageDescriptors;
	VkDescriptorSetLayout _shadowSceneDescriptorLayout;

	std::vector<vkutil::MaterialPass> forward_passes;

	BlackKey::FrameData _frames[FRAME_OVERLAP];
	BlackKey::FrameData& get_current_frame() { return _frames[_frameNumber % FRAME_OVERLAP]; };


	bool resize_requested = false;
	bool _isInitialized{ false };
	int _frameNumber{ 0 };
	bool render_shadowMap{ true };
	bool stop_rendering{ false };
	bool debugShadowMap = false;
	bool use_bindless = true;
	bool debugBuffer = false;
	bool debugLightClustering = false;
	bool readDebugBuffer = false;

	struct {
		float lastFrame;
	} delta;
	VkExtent2D _windowExtent{ 1920,1080};
	float bloom_filter_radius = 0.005f;
	float bloom_strength = 0.08f;
	bool use_fxaa = false;
	bool use_smaa = false;
	float _aspect_width = 1920;
	float _aspect_height = 1080;

	Cascade cascadeData;
	DeletionQueue _mainDeletionQueue;
	AllocatedImage _drawImage;
	AllocatedImage _depthImage;
	AllocatedImage _depthResolveImage;
	AllocatedImage _resolveImage;
	AllocatedImage _hdrImage;
	AllocatedImage _shadowDepthImage;
	AllocatedImage _presentImage;
	AllocatedImage _depthPyramid;
	std::vector<BlackKey::BloomMip> bloom_mip_maps;
	uint32_t mip_chain_length = 5;
	
	IBLData IBL;
	int draw_count = 0;

	VkExtent2D _drawExtent;
	float render_scale = 1.f;

	VkPipeline gradient_pipeline;
	VkPipelineLayout gradient_pipeline_layout;

	PipelineStateObject cull_lights_pso;
	PipelineStateObject cull_objects_pso;
	PipelineStateObject generate_clusters_pso;
	PipelineStateObject depth_reduce_pso;
	PipelineStateObject downsample_bloom_pso;
	PipelineStateObject upsample_bloom_pso;

	GPUSceneData scene_data;
	shadowData shadow_data;
	VkDescriptorSetLayout _gpuSceneDataDescriptorLayout;
	VkDescriptorSetLayout _singleImageDescriptorLayout;
	VkDescriptorSetLayout _skyboxDescriptorLayout;
	VkDescriptorSetLayout postprocess_descriptor_layout;
	VkDescriptorSetLayout _cullLightsDescriptorLayout;
	VkDescriptorSetLayout _buildClustersDescriptorLayout;
	VkDescriptorSetLayout compute_cull_descriptor_layout;
	VkDescriptorSetLayout depth_reduce_descriptor_layout;
	VkDescriptorSetLayout cascaded_shadows_descriptor_layout;
	//VkDescriptorSetLayout _

	AllocatedImage _whiteImage;
	AllocatedImage _blackImage;
	AllocatedImage _greyImage;
	AllocatedImage storageImage;
	AllocatedImage errorCheckerboardImage;
	VkImageView depthPyramidMips[16];

	AllocatedImage _skyImage;
	ktxVulkanTexture _skyBoxImage;

	VkSampler defaultSamplerLinear;
	VkSampler defaultSamplerNearest;
	VkSampler cubeMapSampler;
	VkSampler depthSampler;
	VkSampler depthReductionSampler;
	VkSampler bloomSampler;
	DrawContext drawCommands;
	DrawContext skyDrawCommands;
	DrawContext imageDrawCommands;
	ShadowCascades shadows;

	uint32_t depthPyramidWidth;
	uint32_t depthPyramidHeight;
	uint32_t depthPyramidLevels;

	uint32_t shadowMapSize = 2048;
	EngineStats stats;
	VkSampleCountFlagBits msaa_samples;

	bool debugDepthTexture = false;

	std::vector<VkBufferMemoryBarrier> cullBarriers;

	//Clustered culling  values
	struct {
		//Configuration values
		const uint32_t gridSizeX = 16;
		const uint32_t gridSizeY = 9;
		const uint32_t gridSizeZ = 24;
		const uint32_t numClusters = gridSizeX * gridSizeY * gridSizeZ;
		const uint32_t maxLightsPerTile = 1000;
		uint32_t sizeX, sizeY;

		//Storage Buffers
		AllocatedBuffer AABBVolumeGridSSBO;
		AllocatedBuffer screenToViewSSBO;
		AllocatedBuffer lightSSBO;
		AllocatedBuffer lightIndexListSSBO;
		AllocatedBuffer lightGridSSBO;
		AllocatedBuffer lightGlobalIndex[2];
	} ClusterValues;

	std::vector<uint32_t> draws;
	std::unordered_map<std::string, std::shared_ptr<LoadedGLTF>> loadedScenes;

	//lights
	DirectionalLight directLight;
	uint32_t maxLights = 100;

	struct PointLightData {

		uint32_t numOfLights = 6;
		std::vector<PointLight> pointLights;

	}pointData;
	DrawContext mainDrawContext;
};
