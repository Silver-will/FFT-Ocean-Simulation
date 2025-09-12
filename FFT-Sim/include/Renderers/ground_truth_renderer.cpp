#include "ground_truth_renderer.h"
#include "../vk_device.h"
#include "../graphics.h"
#include "../UI.h"

#include <VkBootstrap.h>

#include <chrono>
#include <thread>
#include <print>
#include <random>

#include <Tracy.hpp>

#include <string>
#include <glm/glm.hpp>
using namespace std::literals::string_literals;

#include <vk_mem_alloc.h>

#include <imgui/imgui.h>
#include <imgui/imgui_impl_glfw.h>
#include <imgui/imgui_impl_vulkan.h>

#define M_PI       3.14159265358979323846


void GroundTruthRenderer::Init(VulkanEngine* engine)
{
	assert(engine != nullptr);
	this->engine = engine;

	InitEngine();

	ConfigureRenderWindow();

	InitSwapchain();

	InitRenderTargets();

	InitCommands();

	InitSyncStructures();

	InitDescriptors();

	InitDefaultData();

	InitBuffers();

	InitPipelines();

	InitImgui();

	LoadAssets();

	_isInitialized = true;
}

void GroundTruthRenderer::ConfigureRenderWindow()
{

	glfwSetWindowUserPointer(engine->window, this);
	glfwSetFramebufferSizeCallback(engine->window, FramebufferResizeCallback);
	glfwSetKeyCallback(engine->window, KeyCallback);
	glfwSetCursorPosCallback(engine->window, CursorCallback);
	glfwSetInputMode(engine->window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
}


void GroundTruthRenderer::KeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
	auto app = reinterpret_cast<GroundTruthRenderer*>(glfwGetWindowUserPointer(window));
	app->main_camera.processKeyInput(window, key, action);
}

void GroundTruthRenderer::CursorCallback(GLFWwindow* window, double xpos, double ypos)
{
	auto app = reinterpret_cast<GroundTruthRenderer*>(glfwGetWindowUserPointer(window));
	app->main_camera.processMouseMovement(window, xpos, ypos);
}

void GroundTruthRenderer::FramebufferResizeCallback(GLFWwindow* window, int width, int height)
{
	auto app = reinterpret_cast<GroundTruthRenderer*>(glfwGetWindowUserPointer(window));
	app->resize_requested = true;
	if (width == 0 || height == 0)
		app->stop_rendering = true;
	else
		app->stop_rendering = false;
}

void GroundTruthRenderer::InitEngine()
{
	//Request required GPU features and extensions
	//vulkan 1.3 features
	VkPhysicalDeviceVulkan13Features features{ .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES };
	features.dynamicRendering = true;
	features.synchronization2 = true;

	//vulkan 1.2 features
	VkPhysicalDeviceVulkan12Features features12{ .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES };
	features12.bufferDeviceAddress = true;
	features12.descriptorIndexing = true;
	features12.runtimeDescriptorArray = true;
	features12.descriptorBindingPartiallyBound = true;
	features12.descriptorBindingSampledImageUpdateAfterBind = true;
	features12.descriptorBindingUniformBufferUpdateAfterBind = true;
	features12.descriptorBindingStorageImageUpdateAfterBind = true;
	features12.shaderSampledImageArrayNonUniformIndexing = true;
	features12.descriptorBindingUpdateUnusedWhilePending = true;
	features12.descriptorBindingVariableDescriptorCount = true;
	features12.samplerFilterMinmax = true;


	VkPhysicalDeviceVulkan11Features features11{ .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES };
	features11.shaderDrawParameters = true;

	VkPhysicalDeviceFeatures baseFeatures{};
	baseFeatures.geometryShader = true;
	baseFeatures.samplerAnisotropy = true;
	baseFeatures.sampleRateShading = true;
	baseFeatures.drawIndirectFirstInstance = true;
	baseFeatures.multiDrawIndirect = true;
	engine->init(baseFeatures, features11, features12, features);
	resource_manager = std::make_shared<ResourceManager>(engine);
	scene_manager = std::make_shared<SceneManager>();
	scene_manager->Init(resource_manager, engine);
}

void GroundTruthRenderer::InitSwapchain()
{
	CreateSwapchain(_windowExtent.width, _windowExtent.height);
}

void GroundTruthRenderer::InitRenderTargets()
{
	VkExtent3D drawImageExtent = {
	_windowExtent.width,
	_windowExtent.height,
	1
	};

	//Allocate images larger than swapchain to avoid 
	const GLFWvidmode* mode = glfwGetVideoMode(glfwGetPrimaryMonitor());

	
	//hardcoding the draw format to 16 bit float
	_drawImage.imageFormat = VK_FORMAT_R16G16B16A16_SFLOAT;
	_drawImage.imageExtent = drawImageExtent;

	VkImageUsageFlags drawImageUsages{};
	drawImageUsages |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
	drawImageUsages |= VK_IMAGE_USAGE_SAMPLED_BIT;
	drawImageUsages |= VK_IMAGE_USAGE_STORAGE_BIT;
	drawImageUsages |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;

	VkImageCreateInfo rimg_info = vkinit::image_create_info(_drawImage.imageFormat, drawImageUsages, drawImageExtent, 1);

	//for the draw image, we want to allocate it from gpu local memory
	VmaAllocationCreateInfo rimg_allocinfo = {};
	rimg_allocinfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
	rimg_allocinfo.requiredFlags = VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

	//allocate and create the image
	vmaCreateImage(engine->_allocator, &rimg_info, &rimg_allocinfo, &_drawImage.image, &_drawImage.allocation, nullptr);
	vmaSetAllocationName(engine->_allocator, _drawImage.allocation, "Draw image");

	//build a image-view for the draw image to use for rendering
	VkImageViewCreateInfo rview_info = vkinit::imageview_create_info(_drawImage.imageFormat, _drawImage.image, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_VIEW_TYPE_2D);

	VK_CHECK(vkCreateImageView(engine->_device, &rview_info, nullptr, &_drawImage.imageView));

	//add to deletion queues
	resource_manager->deletionQueue.push_function([=]() {
		vkDestroyImageView(engine->_device, _drawImage.imageView, nullptr);
		vmaDestroyImage(engine->_allocator, _drawImage.image, _drawImage.allocation);
		});
}


void GroundTruthRenderer::InitCommands()
{
	VkCommandPoolCreateInfo commandPoolInfo = vkinit::command_pool_create_info(engine->_graphicsQueueFamily, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);

	for (int i = 0; i < FRAME_OVERLAP; i++) {

		VK_CHECK(vkCreateCommandPool(engine->_device, &commandPoolInfo, nullptr, &_frames[i]._commandPool));

		// allocate the default command buffer that we will use for rendering
		VkCommandBufferAllocateInfo cmdAllocInfo = vkinit::command_buffer_allocate_info(_frames[i]._commandPool, 1);

		VK_CHECK(vkAllocateCommandBuffers(engine->_device, &cmdAllocInfo, &_frames[i]._mainCommandBuffer));

		resource_manager->deletionQueue.push_function([=]() { vkDestroyCommandPool(engine->_device, _frames[i]._commandPool, nullptr); });
	}


}

void GroundTruthRenderer::InitSyncStructures()
{
	VkFenceCreateInfo fenceCreateInfo = vkinit::fence_create_info(VK_FENCE_CREATE_SIGNALED_BIT);
	for (int i = 0; i < FRAME_OVERLAP; i++) {

		VK_CHECK(vkCreateFence(engine->_device, &fenceCreateInfo, nullptr, &_frames[i]._renderFence));

		VkSemaphoreCreateInfo semaphoreCreateInfo = vkinit::semaphore_create_info();

		VK_CHECK(vkCreateSemaphore(engine->_device, &semaphoreCreateInfo, nullptr, &_frames[i]._swapchainSemaphore));
		VK_CHECK(vkCreateSemaphore(engine->_device, &semaphoreCreateInfo, nullptr, &_frames[i]._renderSemaphore));

		resource_manager->deletionQueue.push_function([=]() {
			vkDestroyFence(engine->_device, _frames[i]._renderFence, nullptr);
			vkDestroySemaphore(engine->_device, _frames[i]._swapchainSemaphore, nullptr);
			vkDestroySemaphore(engine->_device, _frames[i]._renderSemaphore, nullptr);
			});
	}
}

void GroundTruthRenderer::InitDescriptors()
{
	//create a descriptor pool that will hold 10 sets with 1 image each
	std::vector<DescriptorAllocator::PoolSizeRatio> sizes = {
		{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 3 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 6 },
		{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 3 },
	};

	globalDescriptorAllocator.init_pool(engine->_device, 30, sizes);
	_mainDeletionQueue.push_function(
		[&]() { vkDestroyDescriptorPool(engine->_device, globalDescriptorAllocator.pool, nullptr); });


	{
		DescriptorLayoutBuilder builder;
		builder.add_binding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
		builder.add_binding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
		builder.add_binding(2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
		post_process_descriptor_layout = builder.build(engine->_device, VK_SHADER_STAGE_FRAGMENT_BIT);
	}

	{
		DescriptorLayoutBuilder builder;
		builder.add_binding(0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
		builder.add_binding(1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
		builder.add_binding(2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
		builder.add_binding(3, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
		trace_descriptor_layout = builder.build(engine->_device, VK_SHADER_STAGE_COMPUTE_BIT);
	}

	{
		DescriptorLayoutBuilder builder;
		builder.add_binding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
		builder.add_binding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
		skybox_descriptor_layout = builder.build(engine->_device, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
	}

	{
		DescriptorLayoutBuilder builder;
		builder.add_binding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 65536);
		builder.add_binding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 65536);
		builder.add_binding(2, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 65536);
		resource_manager->bindless_descriptor_layout = builder.build(engine->_device, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, nullptr, VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT_EXT);
	}

	_mainDeletionQueue.push_function([&]() {
		vkDestroyDescriptorSetLayout(engine->_device, post_process_descriptor_layout, nullptr);
		vkDestroyDescriptorSetLayout(engine->_device, skybox_descriptor_layout, nullptr);
		vkDestroyDescriptorSetLayout(engine->_device, trace_descriptor_layout, nullptr);
		vkDestroyDescriptorSetLayout(engine->_device, resource_manager->bindless_descriptor_layout, nullptr);
		});

	for (int i = 0; i < FRAME_OVERLAP; i++) {
		// create a descriptor 
		std::vector<DescriptorAllocatorGrowable::PoolSizeRatio> frame_sizes = {
			{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 3 },
			{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 6 },
			{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 3 },
			{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 4 },
		};

		std::vector<DescriptorAllocator::PoolSizeRatio> bindless_sizes = {
		{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1 },
		{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1 },
		};

		_frames[i]._frameDescriptors = DescriptorAllocatorGrowable{};
		_frames[i]._frameDescriptors.init(engine->_device, 1000, frame_sizes);
		_frames[i].bindless_material_descriptor = DescriptorAllocator{};
		_frames[i].bindless_material_descriptor.init_pool(engine->_device, 65536, bindless_sizes);
		_mainDeletionQueue.push_function([&, i]() {
			_frames[i]._frameDescriptors.destroy_pools(engine->_device);
			_frames[i].bindless_material_descriptor.destroy_pool(engine->_device);
			});
	}
}


void GroundTruthRenderer::InitPipelines()
{
	
	InitComputePipelines();
}


void GroundTruthRenderer::InitComputePipelines()
{
	VkPipelineLayoutCreateInfo trace_rays_rays_layout_info = {};
	trace_rays_rays_layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	trace_rays_rays_layout_info.pNext = nullptr;
	trace_rays_rays_layout_info.pSetLayouts = &trace_descriptor_layout;
	trace_rays_rays_layout_info.setLayoutCount = 1;

	VkPushConstantRange push_constant{};
	push_constant.offset = 0;
	push_constant.size = sizeof(TraceParams);
	push_constant.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

	trace_rays_rays_layout_info.pPushConstantRanges = nullptr;
	trace_rays_rays_layout_info.pushConstantRangeCount = 0;


	VK_CHECK(vkCreatePipelineLayout(engine->_device, &trace_rays_rays_layout_info, nullptr, &trace_rays_pso.layout));

	VkShaderModule trace_shader;
	if (!vkutil::load_shader_module(std::string(assets_path + "/shaders/gradient.spv").c_str(), engine->_device, &trace_shader)) {
		std::print("Error when building the compute shader \n");
	}
	
	VkPipelineShaderStageCreateInfo stage_info{};
	stage_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	stage_info.pNext = nullptr;
	stage_info.stage = VK_SHADER_STAGE_COMPUTE_BIT;
	stage_info.module = trace_shader;
	stage_info.pName = "main";

	VkComputePipelineCreateInfo compute_pipeline_creation_info{};
	compute_pipeline_creation_info.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
	compute_pipeline_creation_info.pNext = nullptr;
	compute_pipeline_creation_info.layout = trace_rays_pso.layout;
	compute_pipeline_creation_info.stage = stage_info;

	VK_CHECK(vkCreateComputePipelines(engine->_device, VK_NULL_HANDLE, 1, &compute_pipeline_creation_info, nullptr, &trace_rays_pso.pipeline));

	_mainDeletionQueue.push_function([=]() {
		vkDestroyPipelineLayout(engine->_device, trace_rays_pso.layout, nullptr);
		vkDestroyPipeline(engine->_device, trace_rays_pso.pipeline, nullptr);
		});
}

void GroundTruthRenderer::InitDefaultData()
{
	assets_path = GetAssetPath();
	//Create default images
	uint32_t white = glm::packUnorm4x8(glm::vec4(1, 1, 1, 1));
	white_image = resource_manager->CreateImage((void*)&white, VkExtent3D{ 1, 1, 1 }, VK_FORMAT_R8G8B8A8_UNORM,
		VK_IMAGE_USAGE_SAMPLED_BIT);

	uint32_t grey = glm::packUnorm4x8(glm::vec4(0.66f, 0.66f, 0.66f, 1));
	grey_image = resource_manager->CreateImage((void*)&grey, VkExtent3D{ 1, 1, 1 }, VK_FORMAT_R8G8B8A8_UNORM,
		VK_IMAGE_USAGE_SAMPLED_BIT);

	uint32_t black = glm::packUnorm4x8(glm::vec4(0, 0, 0, 0));
	black_image = resource_manager->CreateImage((void*)&black, VkExtent3D{ 1, 1, 1 }, VK_FORMAT_R8G8B8A8_UNORM,
		VK_IMAGE_USAGE_SAMPLED_BIT);

	storage_image = resource_manager->CreateImage((void*)&black, VkExtent3D{ 1, 1, 1 }, VK_FORMAT_R8G8B8A8_UNORM,
		VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT);

	main_camera.type = Camera::CameraType::firstperson;
	//mainCamera.flipY = true;
	main_camera.movementSpeed = 2.5f;
	main_camera.setPerspective(45.0f, (float)_windowExtent.width / (float)_windowExtent.height, 0.1f, 1000.0f);
	main_camera.setPosition(glm::vec3(-0.12f, -5.14f, -2.25f));
	main_camera.setRotation(glm::vec3(-17.0f, 7.0f, 0.0f));

	//Populate point light list
	int numOfLights = 1;
	std::random_device dev;
	std::mt19937 rng(dev());
	std::uniform_real_distribution<> distFloat(-25.0f, 25.0f);
	std::uniform_real_distribution<> distRadius(5.5f, 8.f);
	std::uniform_real_distribution<> distRGB(0, 255.0f);
	for (int i = 0; i < numOfLights; i++)
	{
		pointData.pointLights.push_back(PointLight(glm::vec4(distFloat(rng), (distFloat(rng) + 25.0f) / 2.0f, distFloat(rng), 1.0f), glm::vec4(distRGB(rng) / 255.0f, distRGB(rng) / 255.0f, distRGB(rng) / 255.0f, 1.0), distRadius(rng), 10.0f));
		//pointData.pointLights.push_back(PointLight(glm::vec4(3, 5, 4.1,1.0), glm::vec4(1.0),10.3f, 10.0f));
	}

	glm::vec2 mip_extent(_windowExtent.width, _windowExtent.height);
	glm::ivec2 mip_int_extent(mip_extent.r, mip_extent.g);
	//Create Bloom mip texture
	for (size_t i = 0; i < mip_chain_length; i++)
	{
		BlackKey::BloomMip mip;
		mip_extent *= 0.5f;
		mip_int_extent /= 2;
		mip.size = mip_extent;
		mip.i_size = mip_int_extent;
		mip.mip = resource_manager->CreateImageEmpty(VkExtent3D(mip_extent.r, mip_extent.g, 1), VK_FORMAT_B10G11R11_UFLOAT_PACK32,
			VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT, VK_IMAGE_VIEW_TYPE_2D, false, 1);

		bloom_mip_maps.emplace_back(mip);
	}
	

	//checkerboard image
	uint32_t magenta = glm::packUnorm4x8(glm::vec4(1, 0, 1, 1));
	std::array<uint32_t, 16 * 16 > pixels; //for 16x16 checkerboard texture
	for (int x = 0; x < 16; x++) {
		for (int y = 0; y < 16; y++) {
			pixels[y * 16 + x] = ((x % 2) ^ (y % 2)) ? magenta : black;
		}
	}

	errorCheckerboardImage = resource_manager->CreateImage(pixels.data(), VkExtent3D{ 16, 16, 1 }, VK_FORMAT_R8G8B8A8_UNORM,
		VK_IMAGE_USAGE_SAMPLED_BIT, this);

	VkSamplerCreateInfo sampl = { .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO };

	sampl.magFilter = VK_FILTER_NEAREST;
	sampl.minFilter = VK_FILTER_NEAREST;

	vkCreateSampler(engine->_device, &sampl, nullptr, &defaultSamplerNearest);

	sampl.magFilter = VK_FILTER_LINEAR;
	sampl.minFilter = VK_FILTER_LINEAR;
	vkCreateSampler(engine->_device, &sampl, nullptr, &defaultSamplerLinear);

	/*VkSamplerCreateInfo bloomSampl = sampl;
	bloomSampl.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	bloomSampl.addressModeV = bloomSampl.addressModeU;
	bloomSampl.addressModeW = bloomSampl.addressModeU;
	vkCreateSampler(engine->_device, &bloomSampl, nullptr, &bloomSampler);
	*/

	VkSamplerCreateInfo cubeSampl = { .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO };
	cubeSampl.magFilter = VK_FILTER_LINEAR;
	cubeSampl.minFilter = VK_FILTER_LINEAR;
	cubeSampl.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
	cubeSampl.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	cubeSampl.addressModeV = cubeSampl.addressModeU;
	cubeSampl.addressModeW = cubeSampl.addressModeU;
	cubeSampl.mipLodBias = 0.0f;
	//cubeSampl.maxAnisotropy = device->enabledFeatures.samplerAnisotropy ? device->properties.limits.maxSamplerAnisotropy : 1.0f;
	//samplerCreateInfo.anisotropyEnable = device->enabledFeatures.samplerAnisotropy;
	cubeSampl.compareOp = VK_COMPARE_OP_NEVER;
	cubeSampl.minLod = 0.0f;
	cubeSampl.maxLod = (float)11;
	cubeSampl.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
	vkCreateSampler(engine->_device, &cubeSampl, nullptr, &cubeMapSampler);

	//< default_img

	_mainDeletionQueue.push_function([=]() {
		resource_manager->DestroyImage(white_image);
		resource_manager->DestroyImage(grey_image);
		resource_manager->DestroyImage(black_image);
		resource_manager->DestroyImage(storage_image);
		resource_manager->DestroyImage(_skyImage);
		vkDestroySampler(engine->_device, defaultSamplerLinear, nullptr);
		vkDestroySampler(engine->_device, defaultSamplerNearest, nullptr);
		vkDestroySampler(engine->_device, cubeMapSampler, nullptr);
		/*
		for (size_t i = 0; i < bloom_mip_maps.size(); i++)
		{
			resource_manager->DestroyImage(bloom_mip_maps[i].mip);
		}
		*/
		});
}


void GroundTruthRenderer::CreateSwapchain(uint32_t width, uint32_t height)
{
	vkb::SwapchainBuilder swapchainBuilder{ engine->_chosenGPU,engine->_device,engine->_surface };

	swapchain_image_format = VK_FORMAT_B8G8R8A8_UNORM;

	vkb::Swapchain vkbSwapchain = swapchainBuilder
		//.use_default_format_selection()
		.set_desired_format(VkSurfaceFormatKHR{ .format = swapchain_image_format, .colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR })
		//use vsync present mode
		.set_desired_present_mode(VK_PRESENT_MODE_IMMEDIATE_KHR)
		.set_desired_extent(width, height)
		.add_image_usage_flags(VK_IMAGE_USAGE_TRANSFER_DST_BIT)
		.build()
		.value();


	_swapchainExtent = vkbSwapchain.extent;
	//store swapchain and its related images
	swapchain = vkbSwapchain.swapchain;
	swapchain_images = vkbSwapchain.get_images().value();
	swapchain_image_views = vkbSwapchain.get_image_views().value();
}


void GroundTruthRenderer::InitBuffers()
{
	
}


void GroundTruthRenderer::InitImgui()
{

	// 1: create descriptor pool for IMGUI
	//  the size of the pool is very oversize, but it's copied from imgui demo
	//  itself.
	VkDescriptorPoolSize pool_sizes[] = { { VK_DESCRIPTOR_TYPE_SAMPLER, 100 },
		{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 100 },
		{ VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 100 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 100 },
		{ VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 100 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 100 },
		{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 100 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 100 },
		{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 100 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 100 },
		{ VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 100 } };

	VkDescriptorPoolCreateInfo pool_info = {};
	pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
	pool_info.maxSets = 1000;
	pool_info.poolSizeCount = (uint32_t)std::size(pool_sizes);
	pool_info.pPoolSizes = pool_sizes;

	VkDescriptorPool imguiPool;
	VK_CHECK(vkCreateDescriptorPool(engine->_device, &pool_info, nullptr, &imguiPool));

	// 2: initialize imgui library

	// this initializes the core structures of imgui
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO(); (void)io;
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;

	// this initializes imgui for SDL
	ImGui_ImplGlfw_InitForVulkan(engine->window, true);

	// this initializes imgui for Vulkan
	ImGui_ImplVulkan_InitInfo init_info = {};
	init_info.Instance = engine->_instance;
	init_info.PhysicalDevice = engine->_chosenGPU;
	init_info.Device = engine->_device;
	init_info.Queue = engine->_graphicsQueue;
	init_info.DescriptorPool = imguiPool;
	init_info.MinImageCount = 3;
	init_info.ImageCount = 3;
	init_info.UseDynamicRendering = true;

	//dynamic rendering parameters for imgui to use
	init_info.PipelineRenderingCreateInfo = { .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO };
	init_info.PipelineRenderingCreateInfo.colorAttachmentCount = 1;
	init_info.PipelineRenderingCreateInfo.pColorAttachmentFormats = &swapchain_image_format;


	init_info.MSAASamples = VK_SAMPLE_COUNT_1_BIT;

	ImGui_ImplVulkan_Init(&init_info);

	ImGui_ImplVulkan_CreateFontsTexture();

	// add the destroy the imgui created structures
	_mainDeletionQueue.push_function([=]() {
		ImGui_ImplVulkan_Shutdown();
		vkDestroyDescriptorPool(engine->_device, imguiPool, nullptr);
		});
}

void GroundTruthRenderer::DestroySwapchain()
{
	vkDestroySwapchainKHR(engine->_device, swapchain, nullptr);

	// destroy swapchain resources
	for (int i = 0; i < swapchain_image_views.size(); i++) {

		vkDestroyImageView(engine->_device, swapchain_image_views[i], nullptr);
	}
}

void GroundTruthRenderer::UpdateScene()
{
	float currentFrame = glfwGetTime();
	float deltaTime = currentFrame - delta.lastFrame;
	delta.lastFrame = currentFrame;
	main_camera.update(deltaTime);
	//mainDrawContext.OpaqueSurfaces.clear();

	scene_data.view = main_camera.matrices.view;
	auto camPos = main_camera.position * -1.0f;
	scene_data.cameraPos = glm::vec4(camPos, 1.0f);
	// camera projection
	main_camera.updateAspectRatio(_aspect_width / _aspect_height);
	scene_data.proj = main_camera.matrices.perspective;

	// invert the Y direction on projection matrix so that we are more similar
	// to opengl and gltf axis
	scene_data.proj[1][1] *= -1;
	scene_data.viewproj = scene_data.proj * scene_data.view;
	glm::mat4 model(1.0f);
	model = glm::translate(model, glm::vec3(0, 50, -500));
	model = glm::scale(model, glm::vec3(10, 10, 10));
	//sceneData.skyMat = model;
	scene_data.skyMat = scene_data.proj * glm::mat4(glm::mat3(scene_data.view));

	//some default lighting parameters
	scene_data.sunlightColor = directLight.color;
	scene_data.sunlightDirection = directLight.direction;
	scene_data.lightCount = pointData.pointLights.size();
	scene_data.ConfigData.x = main_camera.getNearClip();
	scene_data.ConfigData.y = main_camera.getFarClip();

	//Prepare Render objects
	//loadedScenes["sponza"]->Draw(glm::mat4{ 1.f }, drawCommands);
	//loadedScenes["cube"]->Draw(glm::mat4{ 1.f }, skyDrawCommands);
}

void GroundTruthRenderer::LoadAssets()
{
	/*
	//Load in skyBox image
	_skyImage = vkutil::load_cubemap_image("../../assets/textures/hdris/overcast.ktx", VkExtent3D{ 1,1,1 }, engine, VK_FORMAT_R16G16B16A16_SFLOAT, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT, true);

	std::string structurePath{ "../../assets/models/sponza/sponza.gltf" };
	auto structureFile = resource_manager->loadGltf(engine, structurePath, true);
	assert(structureFile.has_value());

	std::string cubePath{ "../../assets/models/cube.gltf" };
	auto cubeFile = resource_manager->loadGltf(engine, cubePath);
	assert(cubeFile.has_value());

	std::string planePath{ "../../assets/models/plane.glb" };
	auto planeFile = resource_manager->loadGltf(engine, planePath);
	assert(planeFile.has_value());

	//loadedScenes["sponza"] = *structureFile;
	loadedScenes["cube"] = *cubeFile;
	loadedScenes["plane"] = *planeFile;

	loadedScenes["sponza"]->Draw(glm::mat4{ 1.f }, drawCommands);
	scene_manager->RegisterMeshAssetReference("sponza");
	//Register render objects for draw indirect
	scene_manager->RegisterObjectBatch(drawCommands);
	scene_manager->MergeMeshes();
	scene_manager->PrepareIndirectBuffers();
	scene_manager->BuildBatches();
	resource_manager->write_material_array();
	*/
}

void GroundTruthRenderer::Cleanup()
{
	if (_isInitialized)
	{
		vkDeviceWaitIdle(engine->_device);

		for (auto& frame : _frames) {
			frame._deletionQueue.flush();
		}
		_mainDeletionQueue.flush();
		resource_manager->cleanup();
		DestroySwapchain();
		engine->cleanup();
	}
	engine = nullptr;
}

void GroundTruthRenderer::Trace(VkCommandBuffer cmd)
{

	VkDescriptorSet trace_descriptor = get_current_frame()._frameDescriptors.allocate(engine->_device, trace_descriptor_layout);
	DescriptorWriter writer;
	writer.write_image(0,_drawImage.imageView,defaultSamplerLinear, VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
	writer.update_set(engine->_device, trace_descriptor);

	vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, trace_rays_pso.pipeline);
	vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, trace_rays_pso.layout, 0, 1, &trace_descriptor, 0, nullptr);
	vkCmdDispatch(cmd, ((uint32_t)_windowExtent.width / 16) + 1, ((uint32_t)_windowExtent.height / 16) + 1, 1);
}

void GroundTruthRenderer::Draw()
{
	ZoneScoped;
	auto start_update = std::chrono::system_clock::now();
	//wait until the gpu has finished rendering the last frame. Timeout of 1 second
	VK_CHECK(vkWaitForFences(engine->_device, 1, &get_current_frame()._renderFence, true, 1000000000));


	auto end_update = std::chrono::system_clock::now();
	auto elapsed_update = std::chrono::duration_cast<std::chrono::microseconds>(end_update - start_update);
	stats.update_time = elapsed_update.count() / 1000.f;

	get_current_frame()._deletionQueue.flush();
	get_current_frame()._frameDescriptors.clear_pools(engine->_device);

	//request image from the swapchain
	uint32_t swapchainImageIndex;
	VkResult e = vkAcquireNextImageKHR(engine->_device, swapchain, 1000000000, get_current_frame()._swapchainSemaphore, nullptr, &swapchainImageIndex);

	if (e == VK_ERROR_OUT_OF_DATE_KHR) {
		resize_requested = true;
		return;
	}

	_drawExtent.height = std::min(_swapchainExtent.height, _drawImage.imageExtent.height);
	_drawExtent.width = std::min(_swapchainExtent.width, _drawImage.imageExtent.width);

	VK_CHECK(vkResetFences(engine->_device, 1, &get_current_frame()._renderFence));

	//now that we are sure that the commands finished executing, we can safely reset the command buffer to begin recording again.
	VK_CHECK(vkResetCommandBuffer(get_current_frame()._mainCommandBuffer, 0));

	//naming it cmd for shorter writing
	VkCommandBuffer cmd = get_current_frame()._mainCommandBuffer;

	//begin the command buffer recording. We will use this command buffer exactly once, so we want to let vulkan know that
	VkCommandBufferBeginInfo cmdBeginInfo = vkinit::command_buffer_begin_info(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

	//> draw_first
	VK_CHECK(vkBeginCommandBuffer(cmd, &cmdBeginInfo));

	// transition our main draw image into general layout so we can write into it
	// we will overwrite it all so we dont care about what was the older layout
	vkutil::transition_image(cmd, _drawImage.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

	DrawBackground(cmd);

	vkutil::transition_image(cmd, _drawImage.image, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL);
	Trace(cmd);
	DrawPostProcess(cmd);

	vkutil::transition_image(cmd, _drawImage.image, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
	vkutil::transition_image(cmd, swapchain_images[swapchainImageIndex], VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

	//< draw_first
	//> imgui_draw
	// execute a copy from the draw image into the swapchain
	vkutil::copy_image_to_image(cmd, _drawImage.image, swapchain_images[swapchainImageIndex], _drawExtent, _swapchainExtent);
	//vkutil::copy_image_to_image(cmd, _resolveImage.image, _swapchainImages[swapchainImageIndex], _drawExtent, _swapchainExtent);
	// set swapchain image layout to Attachment Optimal so we can draw it
	vkutil::transition_image(cmd, swapchain_images[swapchainImageIndex], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

	//draw UI directly into the swapchain image
	DrawImgui(cmd, swapchain_image_views[swapchainImageIndex]);

	// set swapchain image layout to Present so we can draw it
	vkutil::transition_image(cmd, swapchain_images[swapchainImageIndex], VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

	//finalize the command buffer (we can no longer add commands, but it can now be executed)
	VK_CHECK(vkEndCommandBuffer(cmd));

	//prepare the submission to the queue. 
	//we want to wait on the _presentSemaphore, as that semaphore is signaled when the swapchain is ready
	//we will signal the _renderSemaphore, to signal that rendering has finished

	VkCommandBufferSubmitInfo cmdinfo = vkinit::command_buffer_submit_info(cmd);

	VkSemaphoreSubmitInfo waitInfo = vkinit::semaphore_submit_info(VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR, get_current_frame()._swapchainSemaphore);
	VkSemaphoreSubmitInfo signalInfo = vkinit::semaphore_submit_info(VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT, get_current_frame()._renderSemaphore);

	VkSubmitInfo2 submit = vkinit::submit_info(&cmdinfo, &signalInfo, &waitInfo);

	//submit command buffer to the queue and execute it.
	// _renderFence will now block until the graphic commands finish execution
	VK_CHECK(vkQueueSubmit2(engine->_graphicsQueue, 1, &submit, get_current_frame()._renderFence));

	//prepare present
	// this will put the image we just rendered to into the visible window.
	// we want to wait on the _renderSemaphore for that, 
	// as its necessary that drawing commands have finished before the image is displayed to the user
	VkPresentInfoKHR presentInfo = vkinit::present_info();

	presentInfo.pSwapchains = &swapchain;
	presentInfo.swapchainCount = 1;

	presentInfo.pWaitSemaphores = &get_current_frame()._renderSemaphore;
	presentInfo.waitSemaphoreCount = 1;

	presentInfo.pImageIndices = &swapchainImageIndex;

	VkResult presentResult = vkQueuePresentKHR(engine->_graphicsQueue, &presentInfo);
	if (e == VK_ERROR_OUT_OF_DATE_KHR) {
		resize_requested = true;
		return;
	}
	//increase the number of frames drawn
	_frameNumber++;
}


void GroundTruthRenderer::DrawPostProcess(VkCommandBuffer cmd)
{
	/*
	ZoneScoped;
	vkutil::transition_image(cmd, _resolveImage.image, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	vkutil::transition_image(cmd, _depthResolveImage.image, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

	//vkutil::transition_image(cmd, bloom_mip_maps[0].mip.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	VkClearValue clear{ 1.0f, 1.0f, 1.0f, 1.0f };
	VkRenderingAttachmentInfo colorAttachment = vkinit::attachment_info(_hdrImage.imageView, nullptr, &clear, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
	VkRenderingInfo hdrRenderInfo = vkinit::rendering_info(_windowExtent, &colorAttachment, nullptr);
	vkCmdBeginRendering(cmd, &hdrRenderInfo);
	vkCmdEndRendering(cmd);
	*/
}

void GroundTruthRenderer::DrawBackground(VkCommandBuffer cmd)
{
	/*
	ZoneScoped;
	std::vector<uint32_t> b_draws;
	b_draws.reserve(skyDrawCommands.OpaqueSurfaces.size());
	//allocate a new uniform buffer for the scene data
	AllocatedBuffer skySceneDataBuffer = vkutil::create_buffer(sizeof(GPUSceneData), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU, engine);

	//add it to the deletion queue of this frame so it gets deleted once its been used
	get_current_frame()._deletionQueue.push_function([=, this]() {
		vkutil::destroy_buffer(skySceneDataBuffer, engine);
		});

	//write the buffer
	void* sceneDataPtr = nullptr;
	vmaMapMemory(engine->_allocator, skySceneDataBuffer.allocation, &sceneDataPtr);
	GPUSceneData* sceneUniformData = (GPUSceneData*)sceneDataPtr;
	*sceneUniformData = scene_data;
	vmaUnmapMemory(engine->_allocator, skySceneDataBuffer.allocation);

	//create a descriptor set that binds that buffer and update it
	VkDescriptorSet globalDescriptor = get_current_frame()._frameDescriptors.allocate(engine->_device, _skyboxDescriptorLayout);

	DescriptorWriter writer;
	writer.write_buffer(0, skySceneDataBuffer.buffer, sizeof(GPUSceneData), 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
	writer.write_image(1, _skyImage.imageView, cubeMapSampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
	writer.update_set(engine->_device, globalDescriptor);

	VkBuffer lastIndexBuffer = VK_NULL_HANDLE;
	auto b_draw = [&](const RenderObject& r) {
		vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, skyBoxPSO.skyPipeline.pipeline);
		vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, skyBoxPSO.skyPipeline.layout, 0, 1,
			&globalDescriptor, 0, nullptr);

		VkViewport viewport = {};
		viewport.x = 0;
		viewport.y = 0;
		viewport.width = (float)_windowExtent.width;
		viewport.height = (float)_windowExtent.height;
		viewport.minDepth = 0.f;
		viewport.maxDepth = 1.f;

		vkCmdSetViewport(cmd, 0, 1, &viewport);

		VkRect2D scissor = {};
		scissor.offset.x = 0;
		scissor.offset.y = 0;
		scissor.extent.width = _windowExtent.width;
		scissor.extent.height = _windowExtent.height;
		vkCmdSetScissor(cmd, 0, 1, &scissor);

		if (r.indexBuffer != lastIndexBuffer)
		{
			lastIndexBuffer = r.indexBuffer;
			vkCmdBindIndexBuffer(cmd, r.indexBuffer, 0, VK_INDEX_TYPE_UINT32);
		}
		// calculate final mesh matrix
		GPUDrawPushConstants push_constants;
		push_constants.worldMatrix = r.transform;
		push_constants.vertexBuffer = r.vertexBufferAddress;

		vkCmdPushConstants(cmd, skyBoxPSO.skyPipeline.layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(GPUDrawPushConstants), &push_constants);
		vkCmdDrawIndexed(cmd, r.indexCount, 1, r.firstIndex, 0, 0);
		};
	b_draw(skyDrawCommands.OpaqueSurfaces[0]);
	skyDrawCommands.OpaqueSurfaces.clear();
	*/
}

void GroundTruthRenderer::DrawImgui(VkCommandBuffer cmd, VkImageView targetImageView)
{
	auto start_imgui = std::chrono::system_clock::now();
	VkRenderingAttachmentInfo colorAttachment = vkinit::attachment_info(targetImageView, nullptr, nullptr, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
	VkRenderingInfo renderInfo = vkinit::rendering_info(_swapchainExtent, &colorAttachment, nullptr);

	vkCmdBeginRendering(cmd, &renderInfo);

	ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd);

	vkCmdEndRendering(cmd);
	auto end_imgui = std::chrono::system_clock::now();
	auto elapsed_imgui = std::chrono::duration_cast<std::chrono::microseconds>(end_imgui - start_imgui);
	stats.ui_draw_time = elapsed_imgui.count() / 1000.f;
}

void GroundTruthRenderer::Run()
{
	bool bQuit = false;


	// main loop
	while (!glfwWindowShouldClose(engine->window)) {
		auto start = std::chrono::system_clock::now();
		if (resize_requested) {
			ResizeSwapchain();
		}
		// do not draw if we are minimized
		if (stop_rendering) {
			// throttle the speed to avoid the endless spinning
			std::this_thread::sleep_for(std::chrono::milliseconds(100));
			continue;
		}

		ImGui_ImplVulkan_NewFrame();
		ImGui_ImplGlfw_NewFrame();


		ImGui::NewFrame();
		SetImguiTheme(0.8f);
		DrawUI();
		ImGui::Render();

		auto start_update = std::chrono::system_clock::now();
		UpdateScene();
		auto end_update = std::chrono::system_clock::now();
		auto elapsed_update = std::chrono::duration_cast<std::chrono::microseconds>(end_update - start_update);
		Draw();
		glfwPollEvents();
		auto end = std::chrono::system_clock::now();

		//convert to microseconds (integer), and then come back to miliseconds
		auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
		stats.frametime = elapsed.count() / 1000.f;
	}
	FrameMark;
}

void GroundTruthRenderer::ResizeSwapchain()
{
	vkDeviceWaitIdle(engine->_device);

	DestroySwapchain();

	int width, height;
	glfwGetWindowSize(engine->window, &width, &height);
	_windowExtent.width = width;
	_windowExtent.height = height;

	_aspect_width = width;
	_aspect_height = height;

	CreateSwapchain(_windowExtent.width, _windowExtent.height);

	//Destroy and recreate render targets
	resource_manager->DestroyImage(_drawImage);

	VkExtent3D ImageExtent{
		width,
		height,
		1
	};
	_drawImage = vkutil::create_image_empty(ImageExtent, VK_FORMAT_R16G16B16A16_SFLOAT, VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
				VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, 
				engine, VK_IMAGE_VIEW_TYPE_2D, false, 1);
	resize_requested = false;
}

void GroundTruthRenderer::DrawUI()
{
	// Demonstrate the various window flags. Typically you would just use the default!
	static bool no_titlebar = false;
	static bool no_scrollbar = false;
	static bool no_menu = false;
	static bool no_move = false;
	static bool no_resize = false;
	static bool no_collapse = false;
	static bool no_close = false;
	static bool no_nav = false;
	static bool no_background = false;
	static bool no_bring_to_front = false;
	static bool no_docking = false;
	static bool unsaved_document = false;

	ImGuiWindowFlags window_flags = 0;
	if (no_titlebar)        window_flags |= ImGuiWindowFlags_NoTitleBar;
	if (no_scrollbar)       window_flags |= ImGuiWindowFlags_NoScrollbar;
	if (!no_menu)           window_flags |= ImGuiWindowFlags_MenuBar;
	if (no_move)            window_flags |= ImGuiWindowFlags_NoMove;
	if (no_resize)          window_flags |= ImGuiWindowFlags_NoResize;
	if (no_collapse)        window_flags |= ImGuiWindowFlags_NoCollapse;
	if (no_nav)             window_flags |= ImGuiWindowFlags_NoNav;
	if (no_background)      window_flags |= ImGuiWindowFlags_NoBackground;
	if (no_bring_to_front)  window_flags |= ImGuiWindowFlags_NoBringToFrontOnFocus;
	//if (no_docking)         window_flags |= ImGuiWindowFlags_NoDocking;
	if (unsaved_document)   window_flags |= ImGuiWindowFlags_UnsavedDocument;

	const ImGuiViewport* main_viewport = ImGui::GetMainViewport();
	ImGui::SetNextWindowPos(ImVec2(main_viewport->WorkPos.x + 650, main_viewport->WorkPos.y + 20), ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowSize(ImVec2(550, 680), ImGuiCond_FirstUseEver);

	bool p_open = true;
	if (!ImGui::Begin("Black key", &p_open, window_flags))
	{
		ImGui::End();
		return;
	}

	if (ImGui::CollapsingHeader("Lighting"))
	{
		std::string index{};
		if (ImGui::TreeNode("Point Lights"))
		{
			for (size_t i = 0; i < pointData.pointLights.size(); i++)
			{
				index = std::to_string(i);
				if (ImGui::TreeNode(("Point "s + index).c_str()))
				{
					if (ImGui::TreeNode("Position"))
					{
						float pos[3] = { pointData.pointLights[i].position.x, pointData.pointLights[i].position.y, pointData.pointLights[i].position.z };
						ImGui::SliderFloat3("x,y,z", pos, -15.0f, 15.0f);
						pointData.pointLights[i].position = glm::vec4(pos[0], pos[1], pos[2], 1.0f);
						ImGui::TreePop();
						ImGui::Spacing();
					}
					if (ImGui::TreeNode("Color"))
					{
						float col[4] = { pointData.pointLights[i].color.x, pointData.pointLights[i].color.y, pointData.pointLights[i].color.z, pointData.pointLights[i].color.w };
						ImGui::ColorEdit4("Light color", col);
						pointData.pointLights[i].color = glm::vec4(col[0], col[1], col[2], col[3]);
						ImGui::TreePop();
						ImGui::Spacing();
					}

					if (ImGui::TreeNode("Attenuation"))
					{
						//move this declaration to a higher scope later
						ImGui::SliderFloat("Range", &pointData.pointLights[i].range, 0.0f, 15.0f);
						ImGui::SliderFloat("Intensity", &pointData.pointLights[i].intensity, 0.0f, 5.0f);
						//ImGui::SliderFloat("quadratic", &points[i].quadratic, 0.0f, 2.0f);
						//ImGui::SliderFloat("radius", &points[i].quadratic, 0.0f, 100.0f);
						ImGui::TreePop();
						ImGui::Spacing();

					}
					ImGui::TreePop();
				}
			}
			ImGui::TreePop();
		}
		if (ImGui::TreeNode("Direct Light"))
		{
			ImGui::SeparatorText("direction");
			float pos[3] = { directLight.direction.x, directLight.direction.y, directLight.direction.z };
			ImGui::SliderFloat3("x,y,z", pos, -7, 7);
			directLight.direction = glm::vec4(pos[0], pos[1], pos[2], 0.0f);

			ImGui::SeparatorText("color");
			float col[4] = { directLight.color.x, directLight.color.y, directLight.color.z, directLight.color.w };
			ImGui::ColorEdit4("Light color", col);
			directLight.color = glm::vec4(col[0], col[1], col[2], col[3]);
			ImGui::TreePop();
		}
	}
	if (ImGui::CollapsingHeader("Post processing"))
	{
		ImGui::SeparatorText("Bloom");
		ImGui::SliderFloat("Bloom filter Radius", &bloom_filter_radius, 0.01f, 2.0f);
		ImGui::SliderFloat("Bloom strength", &bloom_strength, 0.01f, 1.0f);
	}
	if (ImGui::CollapsingHeader("Debugging"))
	{
		ImGui::Checkbox("Read buffer", &readDebugBuffer);
		ImGui::Checkbox("Display buffer", &debugBuffer);
		std::string breh;
		if (debugBuffer)
		{
			auto buffer = resource_manager->GetReadBackBuffer();
			void* data_ptr = nullptr;
			std::vector<uint32_t> buffer_values;
			buffer_values.resize(buffer->info.size);
			vmaMapMemory(engine->_allocator, buffer->allocation, &data_ptr);
			uint32_t* buffer_ptr = (uint32_t*)data_ptr;
			memcpy(buffer_values.data(), buffer_ptr, buffer->info.size);
			vmaUnmapMemory(engine->_allocator, buffer->allocation);


			for (size_t i = 0; i < buffer_values.size(); i++)
			{
				auto string = std::to_string((double)buffer_values[i]);
				breh += string + " ";

				if (i % 10 == 0)
					breh += "\n";
			}

		}
		ImGui::Text(breh.c_str());
	}

	if (ImGui::CollapsingHeader("Engine Stats"))
	{
		ImGui::SeparatorText("Render timings");
		ImGui::Text("FPS %f ", 1000.0f / stats.frametime);
		ImGui::Text("frametime %f ms", stats.frametime);
		ImGui::Text("Triangles: %i", stats.triangle_count);
		ImGui::Text("Indirect Draws: %i", stats.drawcall_count);
		ImGui::Text("UI render time %f ms", stats.ui_draw_time);
		ImGui::Text("Update time %f ms", stats.update_time);
		ImGui::Text("Shadow Pass time %f ms", stats.shadow_pass_time);
		ImGui::Text("Number of active point light %i", static_cast<int>(pointData.pointLights.size()));
	}
	ImGui::End();
}