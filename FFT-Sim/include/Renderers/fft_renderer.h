#ifndef GROUND_TRUTH_RENDERER
#define GROUND_TRUTH_RENDERER

#include "base_renderer.h"
#include "../vk_engine.h"

struct OceanUBO {
	glm::vec3 cam_pos;
	int show_wireframe;
	glm::vec3 sun_direction;
	int padding;
	glm::vec4 sun_color;
	glm::vec4 diffuse_reflectance = glm::vec4(0.01, 0.06, 0.1,1.0f);
	float fresnel_normal_strength = 0.224f;
	float fresnel_shininess = 5.0f;
	float fresnel_bias = 0.02f;
	float specular_reflectance = 0.04f;
	float specular_normal_strength = 1.0f;
	float shininess = 1.0f;
	float fresnel_strength = 1.0f;
	float foam;
	glm::vec4 ambient = glm::vec4(0.02, 0.04, 0.06, 1);
	glm::vec4 fresnel_color = glm::vec4(1.0f);
};

struct HeightSimParams {
	float wind_angle = 45.f;
	float wind_magnitude = 5.142135f;
	int sun_elevation = 0.f;
	int sun_azimuth = 90.f;
	bool wireframe;
	bool changed = true;
	bool is_ping_phase = true;
	bool use_temp_texture = true;
};

struct FFTParams {
	int resolution;
	int ocean_size;
	glm::vec2 wind;
	float delta_time;
	float choppiness = 1.5f;
	int total_count;
	int log_size;
	float fetch;
	float swell;
	float depth;
	int stage;
	int ping_pong_count;
	float displacement_factor = 0.9f;
	float foam_intensity;
	float foam_decay;
};
struct OceanVertex {
	glm::vec4 position;
	glm::vec2 uv;
	glm::vec2 pad;
};

struct OceanSurface {
	std::vector<OceanVertex> vertices;
	std::vector<uint32_t> indices;
	
	GPUMeshBuffers mesh_data;

	uint32_t grid_dimensions = 1024;
	uint32_t texture_dimensions = 512;

	AllocatedImage height_derivative;
	AllocatedImage frequency_domain_texture;
	AllocatedImage height_derivative_texture;
	AllocatedImage horizontal_displacement_map;
	AllocatedImage jacobian_XxZz_map;
	AllocatedImage jacobian_xz_map;
	AllocatedImage ping_1;
	AllocatedImage butterfly_texture;
	AllocatedImage inital_spectrum_texture;
	AllocatedImage normal_map;
	AllocatedImage gaussian_noise_texture;
	AllocatedImage wave_texture;
	AllocatedImage conjugated_spectrum_texture;
	AllocatedImage horizontal_map;
	AllocatedImage height_map;
	AllocatedImage displacement_map;
	AllocatedImage sky_image;
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

	void DrawMain(VkCommandBuffer cmd);
	void DrawBackground(VkCommandBuffer cmd);
	void DrawPostProcess(VkCommandBuffer cmd);
	void DrawImgui(VkCommandBuffer cmd, VkImageView targetImageView);
	void BuildOceanMesh();
	void DrawOceanMesh(VkCommandBuffer cmd);
	void GenerateInitialSpectrum(VkCommandBuffer cmd);
	void GenerateSpectrum(VkCommandBuffer cmd);
	void DebugComputePass(VkCommandBuffer cmd);
	void PreProcessComputePass();
	void WrapSpectrum(VkCommandBuffer cmd);
	void DoIFFT(VkCommandBuffer cmd, AllocatedImage* input = nullptr, AllocatedImage* output = nullptr);

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
	VkDescriptorSetLayout spectrum_layout;
	VkDescriptorSetLayout butterfly_layout;
	VkDescriptorSetLayout image_blit_layout;
	VkDescriptorSetLayout ocean_shading_layout;
	VkDescriptorSetLayout debug_layout;
	VkDescriptorSetLayout wrap_spectrum_layout;
	VkDescriptorSetLayout fft_layout;
	std::vector<VkImageMemoryBarrier> image_barriers;
	
	Camera main_camera;
	std::shared_ptr<ResourceManager> resource_manager;

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
	bool debug_texture = false;

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

	FFTPipelineObject fft_pipeline;
	PipelineStateObject fft_horizontal_pso;
	PipelineStateObject fft_vertical_pso;
	PipelineStateObject normal_calculation_pso;
	PipelineStateObject initial_spectrum_pso;
	PipelineStateObject conjugate_spectrum_pso;
	PipelineStateObject wrap_spectrum_pso;
	PipelineStateObject spectrum_pso;
	PipelineStateObject phase_pso;
	PipelineStateObject debug_pso;
	PipelineStateObject copy_pso;
	PipelineStateObject permute_scale_pso;
	PipelineStateObject butterfly_pso;
	GPUSceneData scene_data;

	
	int planeLength = 10;
	int waveCount = 4;
	float medianWavelength = 1.0f;
	float wavelengthRange = 1.0f;
	float medianDirection = 0.0f;
	float directionalRange = 30.0f;
	float medianAmplitude = 1.0f;
	float medianSpeed = 1.0f;
	float speedRange = 0.1f;
	float steepness = 0.0f;

	// FBM Settings
	int vertexWaveCount = 8;
	int fragmentWaveCount = 40;

	AllocatedImage white_image;
	AllocatedImage black_image;
	AllocatedImage grey_image;
	AllocatedImage storage_image;
	AllocatedImage errorCheckerboardImage;
	AllocatedImage _skyImage;

	VkSampler defaultSamplerLinear;
	VkSampler defaultSamplerNearest;
	VkSampler cubeMapSampler;
	VkSampler samplerLinear;

	FFTParams ocean_params;
	HeightSimParams sim_params;
	OceanUBO ocean_scene_data;

	EngineStats stats;
	std::vector<VkBufferMemoryBarrier> compute_barriers;

	//lights
	DirectionalLight directLight;
	std::string assets_path;

};

#endif