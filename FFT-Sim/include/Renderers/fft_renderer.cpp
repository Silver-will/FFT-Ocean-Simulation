#include "fft_renderer.h"
#include "../vk_device.h"
#include "../UI.h"
#include <stb_image.h>
#include <VkBootstrap.h>

#include <chrono>
#include <thread>
#include <random>
#include <iostream>

#include <string>
#include <glm/glm.hpp>
using namespace std::literals::string_literals;

#include <vk_mem_alloc.h>

#include <imgui/imgui.h>
#include <imgui/imgui_impl_glfw.h>
#include <imgui/imgui_impl_vulkan.h>

#define M_PI       3.14159265358979323846


void FFTRenderer::Init(VulkanEngine* engine)
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

	BuildOceanMesh();

	PreProcessComputePass();

	_isInitialized = true;
}

void FFTRenderer::PreProcessComputePass()
{
	VkDescriptorSet butterFlyDescriptor = globalDescriptorAllocator.allocate(engine->_device, butterfly_layout);

	ocean_params.ocean_size = surface.grid_dimensions;
	ocean_params.resolution = surface.texture_dimensions;
	ocean_params.log_size = log2(surface.texture_dimensions);

	engine->immediate_submit([&](VkCommandBuffer cmd)
		{
			vkutil::transition_image(cmd, surface.butterfly_texture.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
			DescriptorWriter writer;
			writer.write_image(0, surface.butterfly_texture.imageView,defaultSamplerLinear,VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
			writer.update_set(engine->_device, butterFlyDescriptor);

			
			vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, butterfly_pso.pipeline);

			vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, butterfly_pso.layout, 0, 1, &butterFlyDescriptor, 0, nullptr);

			vkCmdPushConstants(cmd, butterfly_pso.layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(FFTParams), &ocean_params);
			vkCmdDispatch(cmd, ocean_params.log_size, (surface.texture_dimensions / 8), 1);
			});
}
void FFTRenderer::ConfigureRenderWindow()
{

	glfwSetWindowUserPointer(engine->window, this);
	glfwSetFramebufferSizeCallback(engine->window, FramebufferResizeCallback);
	glfwSetKeyCallback(engine->window, KeyCallback);
	glfwSetCursorPosCallback(engine->window, CursorCallback);
	glfwSetInputMode(engine->window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
}


void FFTRenderer::KeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
	auto app = reinterpret_cast<FFTRenderer*>(glfwGetWindowUserPointer(window));
	app->main_camera.processKeyInput(window, key, action);
}

void FFTRenderer::CursorCallback(GLFWwindow* window, double xpos, double ypos)
{
	auto app = reinterpret_cast<FFTRenderer*>(glfwGetWindowUserPointer(window));
	app->main_camera.processMouseMovement(window, xpos, ypos);
}

void FFTRenderer::FramebufferResizeCallback(GLFWwindow* window, int width, int height)
{
	auto app = reinterpret_cast<FFTRenderer*>(glfwGetWindowUserPointer(window));
	app->resize_requested = true;
	if (width == 0 || height == 0)
		app->stop_rendering = true;
	else
		app->stop_rendering = false;
}

void FFTRenderer::InitEngine()
{
	//Request required GPU features and extensions
	//vulkan 1.3 features
	VkPhysicalDeviceVulkan13Features features{};
	features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
	features.dynamicRendering = true;
	features.synchronization2 = true;

	//vulkan 1.2 features
	VkPhysicalDeviceVulkan12Features features12{};
	features12.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
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


	VkPhysicalDeviceVulkan11Features features11{};
	features11.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES;
	features11.shaderDrawParameters = true;
	
	VkPhysicalDeviceFeatures baseFeatures{};
	baseFeatures.geometryShader = true;
	baseFeatures.samplerAnisotropy = true;
	baseFeatures.sampleRateShading = true;
	baseFeatures.drawIndirectFirstInstance = true;
	baseFeatures.multiDrawIndirect = true;
	engine->init(baseFeatures, features11, features12, features);
	resource_manager = std::make_shared<ResourceManager>(engine);
}

void FFTRenderer::InitSwapchain()
{
	CreateSwapchain(_windowExtent.width, _windowExtent.height);
}

void FFTRenderer::InitRenderTargets()
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


	_depthImage.imageFormat = VK_FORMAT_D32_SFLOAT;
	_depthImage.imageExtent = drawImageExtent;
	VkImageUsageFlags depthImageUsages{};
	depthImageUsages |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;

	VkImageCreateInfo depth_info = vkinit::image_create_info(_depthImage.imageFormat, depthImageUsages | VK_IMAGE_USAGE_SAMPLED_BIT, drawImageExtent, 1);

	//allocate and create the image
	vmaCreateImage(engine->_allocator, &depth_info, &rimg_allocinfo, &_depthImage.image, &_depthImage.allocation, nullptr);

	//build a image-view for the draw image to use for rendering
	VkImageViewCreateInfo dRview_info = vkinit::imageview_create_info(_depthImage.imageFormat, _depthImage.image, VK_IMAGE_ASPECT_DEPTH_BIT, VK_IMAGE_VIEW_TYPE_2D);

	VK_CHECK(vkCreateImageView(engine->_device, &dRview_info, nullptr, &_depthImage.imageView));

	//add to deletion queues
	resource_manager->deletionQueue.push_function([=]() {
		vkDestroyImageView(engine->_device, _drawImage.imageView, nullptr);
		vmaDestroyImage(engine->_allocator, _drawImage.image, _drawImage.allocation);

		vkDestroyImageView(engine->_device, _depthImage.imageView, nullptr);
		vmaDestroyImage(engine->_allocator, _depthImage.image, _drawImage.allocation);
		});
}


void FFTRenderer::InitCommands()
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

void FFTRenderer::InitSyncStructures()
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

void FFTRenderer::InitDescriptors()
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
		builder.add_binding(0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
		builder.add_binding(1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
		image_blit_layout = builder.build(engine->_device, VK_SHADER_STAGE_COMPUTE_BIT);
	}

	{
		DescriptorLayoutBuilder builder;
		builder.add_binding(0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
		builder.add_binding(1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
		builder.add_binding(2, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
		builder.add_binding(3, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
		wrap_spectrum_layout = builder.build(engine->_device, VK_SHADER_STAGE_COMPUTE_BIT);
	}
	{
		DescriptorLayoutBuilder builder;
		builder.add_binding(0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
		butterfly_layout = builder.build(engine->_device, VK_SHADER_STAGE_COMPUTE_BIT);
	}

	{
		DescriptorLayoutBuilder builder;
		builder.add_binding(0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
		builder.add_binding(1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
		builder.add_binding(2, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
		builder.add_binding(3, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
		builder.add_binding(4, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
		builder.add_binding(5, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
		builder.add_binding(6, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
		spectrum_layout = builder.build(engine->_device, VK_SHADER_STAGE_COMPUTE_BIT);
	}

	{
		DescriptorLayoutBuilder builder;
		builder.add_binding(0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
		builder.add_binding(1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
		builder.add_binding(2, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
		fft_layout = builder.build(engine->_device, VK_SHADER_STAGE_COMPUTE_BIT);
	}

	{
		DescriptorLayoutBuilder builder;
		builder.add_binding(0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
		builder.add_binding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
		debug_layout = builder.build(engine->_device, VK_SHADER_STAGE_COMPUTE_BIT);
	}

	{
		DescriptorLayoutBuilder builder;
		builder.add_binding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
		builder.add_binding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
		builder.add_binding(2, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
		builder.add_binding(3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
		ocean_shading_layout = builder.build(engine->_device, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
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
		vkDestroyDescriptorSetLayout(engine->_device, butterfly_layout, nullptr);
		vkDestroyDescriptorSetLayout(engine->_device, image_blit_layout, nullptr);
		vkDestroyDescriptorSetLayout(engine->_device, spectrum_layout, nullptr);
		vkDestroyDescriptorSetLayout(engine->_device, ocean_shading_layout, nullptr);
		vkDestroyDescriptorSetLayout(engine->_device, skybox_descriptor_layout, nullptr);
		vkDestroyDescriptorSetLayout(engine->_device, resource_manager->bindless_descriptor_layout, nullptr);
		vkDestroyDescriptorSetLayout(engine->_device, debug_layout, nullptr);
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

void FFTRenderer::DrawOceanMesh(VkCommandBuffer cmd)
{

	AllocatedBuffer oceanDataBuffer = vkutil::create_buffer(sizeof(OceanUBO), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU, engine);

	get_current_frame()._deletionQueue.push_function([=] () {
		vkutil::destroy_buffer(oceanDataBuffer, engine);
		});


	//write our allocated uniform buffers
	void* oceanDataPtr = nullptr;
	vmaMapMemory(engine->_allocator, oceanDataBuffer.allocation, &oceanDataPtr);
	OceanUBO* ptr = (OceanUBO*)oceanDataPtr;
	*ptr = ocean_scene_data;
	vmaUnmapMemory(engine->_allocator, oceanDataBuffer.allocation);

	VkDescriptorSet globalDescriptor = get_current_frame()._frameDescriptors.allocate(engine->_device, ocean_shading_layout);

	DescriptorWriter writer;
	writer.write_image(0, surface.displacement_map.imageView, defaultSamplerLinear, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
	writer.write_image(1, surface.height_derivative.imageView, defaultSamplerLinear, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
	writer.write_buffer(2, oceanDataBuffer.buffer, sizeof(OceanUBO), 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
	writer.write_image(3, surface.sky_image.imageView,cubeMapSampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
	writer.update_set(engine->_device, globalDescriptor);

	vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, fft_pipeline.FFTOceanPipeline.pipeline);
	vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, fft_pipeline.FFTOceanPipeline.layout, 0, 1,
		&globalDescriptor, 0, nullptr);

	VkViewport viewport = {};
	viewport.x = 0;
	viewport.y = 0;
	viewport.width = (float)_windowExtent.width;;
	viewport.height = (float)_windowExtent.height;
	viewport.minDepth = 0.f;
	viewport.maxDepth = 1.f;
	vkCmdSetViewport(cmd, 0, 1, &viewport);

	VkRect2D scissor = {};
	scissor.offset.x = 0;
	scissor.offset.y = 0;
	scissor.extent.width = _windowExtent.width;;
	scissor.extent.height = _windowExtent.height;
	vkCmdSetScissor(cmd, 0, 1, &scissor);

	
	GPUDrawPushConstants push_constants;
	push_constants.vertexBuffer = surface.mesh_data.vertexBufferAddress;
	push_constants.material_index = ocean_params.resolution;
	push_constants.worldMatrix = scene_data.viewproj;

	vkCmdPushConstants(cmd, fft_pipeline.FFTOceanPipeline.layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(GPUDrawPushConstants), &push_constants);
	vkCmdBindIndexBuffer(cmd, surface.mesh_data.indexBuffer.buffer, 0, VK_INDEX_TYPE_UINT32);
	vkCmdDrawIndexed(cmd, surface.indices.size(), 1, 0, 0, 0);


}

void FFTRenderer::BuildOceanMesh()
{
	float tex_coord_scale = 1.f;
	int GRID_DIM = 512;
	int HALF_DIM = GRID_DIM / 2;
	int vertex_count = GRID_DIM + 1;
	int idx = 0;
	for (int z = -HALF_DIM; z <= HALF_DIM; ++z)
	{
		for (int x = -HALF_DIM; x <= HALF_DIM; ++x)
		{
			OceanVertex vertex;
			vertex.position = glm::vec4(float(x), 0.f, float(z),1.0f);
			float u = ((float)x / GRID_DIM) + 0.5f;
			float v = ((float)z / GRID_DIM) + 0.5f;
			vertex.uv = glm::vec2(u, v) * tex_coord_scale;
			vertex.pad = glm::vec2(0);
			surface.vertices.push_back(vertex);
		}
	}

	idx = 0;
	
	for (unsigned int y = 0; y < GRID_DIM; ++y)
	{
		for (unsigned int x = 0; x < GRID_DIM-1; ++x)
		{
			uint32_t v0 = y * vertex_count + x;
			uint32_t v1 = y * vertex_count + (x + 1);
			uint32_t v2 = (y + 1) * vertex_count + x;
			uint32_t v3 = (y + 1) * vertex_count + (x + 1);

			surface.indices.push_back(v3);
			surface.indices.push_back(v1);
			surface.indices.push_back(v0);
			
			surface.indices.push_back(v0);
			surface.indices.push_back(v2);
			surface.indices.push_back(v3);
			
			/*surface.indices[idx++] = (vertex_count * y) + x;
			surface.indices[idx++] = (vertex_count * (y + 1)) + x;
			surface.indices[idx++] = (vertex_count * y) + x + 1;

			surface.indices[idx++] = (vertex_count * y) + x + 1;
			surface.indices[idx++] = (vertex_count * (y + 1)) + x;
			surface.indices[idx++] = (vertex_count * (y + 1)) + x + 1;
		*/
		}
	}
	/*
	for (unsigned int y = 0; y < GRID_DIM - 1; ++y)
	{
		for (unsigned int x = 0; x < GRID_DIM; ++x)
		{
			for (unsigned int k = 0; k < 2; ++k)
			{
				surface.indices.push_back(x * GRID_DIM * (y + k * 1));
			}
			uint32_t v0 = y * GRID_DIM + x;
			uint32_t v1 = y * GRID_DIM + (x + 1);
			uint32_t v2 = (y + 1) * GRID_DIM + x;
			uint32_t v3 = (y + 1) * GRID_DIM + (x + 1);

			surface.indices.push_back(v0);
			surface.indices.push_back(v1);
			surface.indices.push_back(v2);

			surface.indices.push_back(v1);
			surface.indices.push_back(v2);
			surface.indices.push_back(v3);
		}
	}
	*/
	//Voxel texture visualization buffer
	size_t buffer_size = surface.vertices.size() * sizeof(OceanVertex);
	const size_t indexBufferSize = surface.indices.size() * sizeof(uint32_t);
	
	surface.mesh_data.vertexBuffer = resource_manager->CreateBuffer(buffer_size, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
		VMA_MEMORY_USAGE_GPU_ONLY);

	surface.mesh_data.indexBuffer = resource_manager->CreateBuffer(indexBufferSize, VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
		VMA_MEMORY_USAGE_GPU_ONLY);

	VkBufferDeviceAddressInfo deviceAdressInfo{};
	deviceAdressInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
	deviceAdressInfo.buffer = surface.mesh_data.vertexBuffer.buffer;
	surface.mesh_data.vertexBufferAddress = vkGetBufferDeviceAddress(resource_manager->engine->_device, &deviceAdressInfo);


	AllocatedBuffer staging = resource_manager->CreateBuffer(buffer_size + indexBufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY);

	void* data = nullptr;
	vmaMapMemory(resource_manager->engine->_allocator, staging.allocation, &data);
	// copy vertex buffer
	memcpy(data, surface.vertices.data(), buffer_size);

	memcpy((char*)data + buffer_size, surface.indices.data(), indexBufferSize);

	resource_manager->engine->immediate_submit([&](VkCommandBuffer cmd) {
		VkBufferCopy vertexCopy{ 0 };
		vertexCopy.dstOffset = 0;
		vertexCopy.srcOffset = 0;
		vertexCopy.size = buffer_size;

		vkCmdCopyBuffer(cmd, staging.buffer, surface.mesh_data.vertexBuffer.buffer, 1, &vertexCopy);

		VkBufferCopy indexCopy{ 0 };
		indexCopy.dstOffset = 0;
		indexCopy.srcOffset = buffer_size;
		indexCopy.size = indexBufferSize;

		vkCmdCopyBuffer(cmd, staging.buffer, surface.mesh_data.indexBuffer.buffer, 1, &indexCopy);
		});

	vmaUnmapMemory(resource_manager->engine->_allocator, staging.allocation);
	resource_manager->DestroyBuffer(staging);
}

void FFTRenderer::InitPipelines()
{
	PipelineCreationInfo info;
	info.layouts.push_back(ocean_shading_layout);
	info.imageFormat = _drawImage.imageFormat;
	info.depthFormat = _depthImage.imageFormat;
	fft_pipeline.build_pipelines(engine, info);
	InitComputePipelines();
}


void FFTRenderer::InitComputePipelines()
{
	auto spectrum_layout_info = vkinit::pipeline_layout_create_info();
	spectrum_layout_info.pSetLayouts = &spectrum_layout;
	spectrum_layout_info.setLayoutCount = 1;

	VkPushConstantRange push_constant{};
	push_constant.offset = 0;
	push_constant.size = sizeof(FFTParams);
	push_constant.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

	spectrum_layout_info.pPushConstantRanges = &push_constant;
	spectrum_layout_info.pushConstantRangeCount = 1;

	VK_CHECK(vkCreatePipelineLayout(engine->_device, &spectrum_layout_info, nullptr, &spectrum_pso.layout));

	VkShaderModule spectrum_shader;
	if (!vkutil::load_shader_module(std::string(assets_path + "/shaders/time_dependent_spectrum.spv").c_str(), engine->_device, &spectrum_shader)) {
		std::cout<<"Error when building the compute shader \n";
	}
	
	auto spectrum_stage_info = vkinit::pipeline_shader_stage_create_info(VK_SHADER_STAGE_COMPUTE_BIT, spectrum_shader);
	auto spectrum_compute_pipeline_creation_info = vkinit::compute_pipeline_create_info(spectrum_pso.layout, spectrum_stage_info);
	
	VK_CHECK(vkCreateComputePipelines(engine->_device, VK_NULL_HANDLE, 1, &spectrum_compute_pipeline_creation_info, nullptr, &spectrum_pso.pipeline));


	auto wrap_spectrum_layout_info = vkinit::pipeline_layout_create_info();
	wrap_spectrum_layout_info.pSetLayouts = &wrap_spectrum_layout;
	wrap_spectrum_layout_info.setLayoutCount = 1;

	wrap_spectrum_layout_info.pPushConstantRanges = &push_constant;
	wrap_spectrum_layout_info.pushConstantRangeCount = 1;

	VK_CHECK(vkCreatePipelineLayout(engine->_device, &wrap_spectrum_layout_info, nullptr, &wrap_spectrum_pso.layout));

	VkShaderModule wrap_spectrum_shader;
	if (!vkutil::load_shader_module(std::string(assets_path + "/shaders/spectrum_wrapper.spv").c_str(), engine->_device, &wrap_spectrum_shader)) {
		std::cout<<("Error when building the compute shader \n");
	}

	auto wrap_spectrum_stage_info = vkinit::pipeline_shader_stage_create_info(VK_SHADER_STAGE_COMPUTE_BIT, wrap_spectrum_shader);
	auto wrap_spectrum_compute_pipeline_creation_info = vkinit::compute_pipeline_create_info(wrap_spectrum_pso.layout, wrap_spectrum_stage_info);

	VK_CHECK(vkCreateComputePipelines(engine->_device, VK_NULL_HANDLE, 1, &wrap_spectrum_compute_pipeline_creation_info, nullptr, &wrap_spectrum_pso.pipeline));


	
	
	//Phase Pso setup
	auto image_blit_layout_info = vkinit::pipeline_layout_create_info();
	image_blit_layout_info.pSetLayouts = &image_blit_layout;
	image_blit_layout_info.setLayoutCount = 1;

	image_blit_layout_info.pPushConstantRanges = &push_constant;
	image_blit_layout_info.pushConstantRangeCount = 1;

	VK_CHECK(vkCreatePipelineLayout(engine->_device, &image_blit_layout_info, nullptr, &phase_pso.layout));

	VkShaderModule phase_shader;
	if (!vkutil::load_shader_module(std::string(assets_path + "/shaders/phase.spv").c_str(), engine->_device, &phase_shader)) {
		std::cout<<("Error when building the compute shader \n");
	}

	auto phase_stage_info = vkinit::pipeline_shader_stage_create_info(VK_SHADER_STAGE_COMPUTE_BIT, phase_shader);
	auto phase_compute_pipeline_creation_info = vkinit::compute_pipeline_create_info(phase_pso.layout, phase_stage_info);

	VK_CHECK(vkCreateComputePipelines(engine->_device, VK_NULL_HANDLE, 1, &phase_compute_pipeline_creation_info, nullptr, &phase_pso.pipeline));

	//Conjugate spectrum setup
	VK_CHECK(vkCreatePipelineLayout(engine->_device, &image_blit_layout_info, nullptr, &conjugate_spectrum_pso.layout));

	VkShaderModule conjugate_spectrum_shader;
	if (!vkutil::load_shader_module(std::string(assets_path + "/shaders/conjugate_spectrum.spv").c_str(), engine->_device, &conjugate_spectrum_shader)) {
		std::cout<<("Error when building the compute shader \n");
	}

	auto conjugate_spectrum_stage_info = vkinit::pipeline_shader_stage_create_info(VK_SHADER_STAGE_COMPUTE_BIT, conjugate_spectrum_shader);
	auto conjugate_spectrum_compute_pipeline_creation_info = vkinit::compute_pipeline_create_info(conjugate_spectrum_pso.layout, conjugate_spectrum_stage_info);

	VK_CHECK(vkCreateComputePipelines(engine->_device, VK_NULL_HANDLE, 1, &conjugate_spectrum_compute_pipeline_creation_info, nullptr, &conjugate_spectrum_pso.pipeline));

	//Copy pass setup
	VK_CHECK(vkCreatePipelineLayout(engine->_device, &image_blit_layout_info, nullptr, &copy_pso.layout));

	VkShaderModule copy_shader;
	if (!vkutil::load_shader_module(std::string(assets_path + "/shaders/copy.spv").c_str(), engine->_device, &copy_shader)) {
		std::cout<<("Error when building the compute shader \n");
	}

	auto copy_stage_info = vkinit::pipeline_shader_stage_create_info(VK_SHADER_STAGE_COMPUTE_BIT, copy_shader);
	auto copy_compute_pipeline_creation_info = vkinit::compute_pipeline_create_info(copy_pso.layout, copy_stage_info);

	VK_CHECK(vkCreateComputePipelines(engine->_device, VK_NULL_HANDLE, 1, &copy_compute_pipeline_creation_info, nullptr, &copy_pso.pipeline));


	//Permute scale pass setup
	VK_CHECK(vkCreatePipelineLayout(engine->_device, &image_blit_layout_info, nullptr, &permute_scale_pso.layout));

	VkShaderModule permute_shader;
	if (!vkutil::load_shader_module(std::string(assets_path + "/shaders/permute_and_scale.spv").c_str(), engine->_device, &permute_shader)) {
		std::cout<<("Error when building the compute shader \n");
	}

	auto permute_stage_info = vkinit::pipeline_shader_stage_create_info(VK_SHADER_STAGE_COMPUTE_BIT, permute_shader);
	auto permute_compute_pipeline_creation_info = vkinit::compute_pipeline_create_info(permute_scale_pso.layout, permute_stage_info);

	VK_CHECK(vkCreateComputePipelines(engine->_device, VK_NULL_HANDLE, 1, &permute_compute_pipeline_creation_info, nullptr, &permute_scale_pso.pipeline));

	//Normal 
	VK_CHECK(vkCreatePipelineLayout(engine->_device, &image_blit_layout_info, nullptr, &normal_calculation_pso.layout));

	VkShaderModule normal_shader;
	if (!vkutil::load_shader_module(std::string(assets_path + "/shaders/normal_map.spv").c_str(), engine->_device, &normal_shader)) {
		std::cout<<("Error when building the compute shader \n");
	}

	auto normal_stage_info = vkinit::pipeline_shader_stage_create_info(VK_SHADER_STAGE_COMPUTE_BIT, normal_shader);
	auto normal_compute_pipeline_creation_info = vkinit::compute_pipeline_create_info(normal_calculation_pso.layout, normal_stage_info);

	VK_CHECK(vkCreateComputePipelines(engine->_device, VK_NULL_HANDLE, 1, &normal_compute_pipeline_creation_info, nullptr, &normal_calculation_pso.pipeline));

	//Initial spectrum
	VK_CHECK(vkCreatePipelineLayout(engine->_device, &spectrum_layout_info, nullptr, &initial_spectrum_pso.layout));

	VkShaderModule initial_spectrum_shader;
	if (!vkutil::load_shader_module(std::string(assets_path + "/shaders/jonswap_spectrum.spv").c_str(), engine->_device, &initial_spectrum_shader)) {
		std::cout<<("Error when building the compute shader \n");
	}

	auto initial_spectrum_stage_info = vkinit::pipeline_shader_stage_create_info(VK_SHADER_STAGE_COMPUTE_BIT, initial_spectrum_shader);
	auto initial_spectrum_compute_pipeline_creation_info = vkinit::compute_pipeline_create_info(initial_spectrum_pso.layout, initial_spectrum_stage_info);

	VK_CHECK(vkCreateComputePipelines(engine->_device, VK_NULL_HANDLE, 1, &initial_spectrum_compute_pipeline_creation_info, nullptr, &initial_spectrum_pso.pipeline));


	///Debug shader
	auto debug_layout_info = vkinit::pipeline_layout_create_info();
	debug_layout_info.pSetLayouts = &debug_layout;
	debug_layout_info.setLayoutCount = 1;

	debug_layout_info.pPushConstantRanges = nullptr;
	debug_layout_info.pushConstantRangeCount = 0;

	
	VK_CHECK(vkCreatePipelineLayout(engine->_device, &debug_layout_info, nullptr, &debug_pso.layout));

	VkShaderModule debug_shader;
	if (!vkutil::load_shader_module(std::string(assets_path + "/shaders/debug.spv").c_str(), engine->_device, &debug_shader)) {
		std::cout<<("Error when building the compute shader \n");
	}

	auto debug_stage_info = vkinit::pipeline_shader_stage_create_info(VK_SHADER_STAGE_COMPUTE_BIT, debug_shader);
	auto debug_compute_pipeline_creation_info = vkinit::compute_pipeline_create_info(debug_pso.layout, debug_stage_info);

	VK_CHECK(vkCreateComputePipelines(engine->_device, VK_NULL_HANDLE, 1, &debug_compute_pipeline_creation_info, nullptr, &debug_pso.pipeline));
	

	auto fft_layout_info = vkinit::pipeline_layout_create_info();
	fft_layout_info.pSetLayouts = &fft_layout;
	fft_layout_info.setLayoutCount = 1;

	fft_layout_info.pPushConstantRanges = &push_constant;
	fft_layout_info.pushConstantRangeCount = 1;


	//FFT Vertical
	VK_CHECK(vkCreatePipelineLayout(engine->_device, &fft_layout_info, nullptr, &fft_vertical_pso.layout));

	VkShaderModule fft_vertical_shader;
	if (!vkutil::load_shader_module(std::string(assets_path + "/shaders/fft_vertical.spv").c_str(), engine->_device, &fft_vertical_shader)) {
		std::cout<<("Error when building the compute shader \n");
	}

	auto fft_vertical_stage_info = vkinit::pipeline_shader_stage_create_info(VK_SHADER_STAGE_COMPUTE_BIT, fft_vertical_shader);
	auto fft_vertical_compute_pipeline_creation_info = vkinit::compute_pipeline_create_info(fft_vertical_pso.layout, fft_vertical_stage_info);

	VK_CHECK(vkCreateComputePipelines(engine->_device, VK_NULL_HANDLE, 1, &fft_vertical_compute_pipeline_creation_info, nullptr, &fft_vertical_pso.pipeline));


	//FFT Horizontal 
	VK_CHECK(vkCreatePipelineLayout(engine->_device, &fft_layout_info, nullptr, &fft_horizontal_pso.layout));

	VkShaderModule fft_horizontal_shader;
	if (!vkutil::load_shader_module(std::string(assets_path + "/shaders/fft_horizontal.spv").c_str(), engine->_device, &fft_horizontal_shader)) {
		std::cout<<("Error when building the compute shader \n");
	}

	auto fft_horizontal_stage_info = vkinit::pipeline_shader_stage_create_info(VK_SHADER_STAGE_COMPUTE_BIT, fft_horizontal_shader);
	auto fft_horizontal_compute_pipeline_creation_info = vkinit::compute_pipeline_create_info(fft_horizontal_pso.layout, fft_horizontal_stage_info);

	VK_CHECK(vkCreateComputePipelines(engine->_device, VK_NULL_HANDLE, 1, &fft_horizontal_compute_pipeline_creation_info, nullptr, &fft_horizontal_pso.pipeline));


	//Butterfly pass
	auto butterfly_layout_info = vkinit::pipeline_layout_create_info();
	butterfly_layout_info.pSetLayouts = &butterfly_layout;
	butterfly_layout_info.setLayoutCount = 1;

	butterfly_layout_info.pPushConstantRanges = &push_constant;
	butterfly_layout_info.pushConstantRangeCount = 1;


	//FFT Vertical
	VK_CHECK(vkCreatePipelineLayout(engine->_device, &butterfly_layout_info, nullptr, &butterfly_pso.layout));

	VkShaderModule butterfly_shader;
	if (!vkutil::load_shader_module(std::string(assets_path + "/shaders/butterfly.spv").c_str(), engine->_device, &butterfly_shader)) {
		std::cout<<"Error when building the compute shader \n";
	}

	auto butterfly_stage_info = vkinit::pipeline_shader_stage_create_info(VK_SHADER_STAGE_COMPUTE_BIT, butterfly_shader);
	auto butterfly_compute_pipeline_creation_info = vkinit::compute_pipeline_create_info(butterfly_pso.layout, butterfly_stage_info);

	VK_CHECK(vkCreateComputePipelines(engine->_device, VK_NULL_HANDLE, 1, &butterfly_compute_pipeline_creation_info, nullptr, &butterfly_pso.pipeline));

	
	_mainDeletionQueue.push_function([=]() {
		resource_manager->DestroyPSO(spectrum_pso);
		resource_manager->DestroyPSO(initial_spectrum_pso);
		resource_manager->DestroyPSO(normal_calculation_pso);
		resource_manager->DestroyPSO(fft_vertical_pso);
		resource_manager->DestroyPSO(fft_horizontal_pso);
		resource_manager->DestroyPSO(debug_pso);
		resource_manager->DestroyPSO(copy_pso);
		resource_manager->DestroyPSO(phase_pso);
		});
}

void FFTRenderer::InitDefaultData()
{
	assets_path = GetAssetPath();

	//Create FFT resource images
	uint32_t RES = surface.texture_dimensions;

	std::vector<float> ping_phase_array(RES * RES);
	std::vector<float> gaussian_noise(RES * RES * 2);

	//generate phase values
	std::random_device dev;
	std::mt19937 rng(dev());
	std::uniform_real_distribution<> distFloat(0.f, 1.f);
	std::uniform_real_distribution<> distGaus(-1.f, 1.f);

	for (size_t i = 0; i < RES * RES; ++i)
	{
		ping_phase_array[i] = distFloat(rng) * 2.f * M_PI;
		//ping_phase_array[i] = distFloat(rng);
		/*if (i < 70000)
		{ 
			ping_phase_array[i] = 0.0f;
		}
		else 
			ping_phase_array[i] = 1.0f;
		*/
	}
	
	for (size_t i = 0; i < RES * RES * 2; ++i)
	{
		gaussian_noise[i] = distGaus(rng);
	}

	auto log_size = log2(RES);
	VkExtent3D oceanExtent;
	oceanExtent.width = RES;
	oceanExtent.height = RES;
	oceanExtent.depth = 1;

	VkExtent3D logExtent;
	logExtent = oceanExtent;
	logExtent.width = log_size;

	//stbi_load(std::string(assets_path + "textures/back.png"))
	surface.displacement_map = resource_manager->CreateImage(oceanExtent, VK_FORMAT_R32G32B32A32_SFLOAT, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT);
	surface.inital_spectrum_texture = resource_manager->CreateImage(oceanExtent, VK_FORMAT_R32G32_SFLOAT, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT);
	surface.horizontal_map = resource_manager->CreateImage(oceanExtent, VK_FORMAT_R32G32B32A32_SFLOAT, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT);
	surface.wave_texture = resource_manager->CreateImage(oceanExtent, VK_FORMAT_R32G32B32A32_SFLOAT, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT);
	surface.conjugated_spectrum_texture = resource_manager->CreateImage(oceanExtent, VK_FORMAT_R32G32B32A32_SFLOAT, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT);
	surface.height_map = resource_manager->CreateImage(oceanExtent, VK_FORMAT_R32G32B32A32_SFLOAT, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT);
	surface.butterfly_texture = resource_manager->CreateImage(logExtent, VK_FORMAT_R32G32B32A32_SFLOAT, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT);
	surface.gaussian_noise_texture = resource_manager->CreateImage(gaussian_noise.data(), oceanExtent, VK_FORMAT_R32G32_SFLOAT, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT, 8);
	surface.height_derivative = resource_manager->CreateImage(oceanExtent, VK_FORMAT_R32G32B32A32_SFLOAT, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT);
	surface.ping_1 = resource_manager->CreateImage(oceanExtent, VK_FORMAT_R32G32B32A32_SFLOAT, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT);
	surface.normal_map = resource_manager->CreateImage(oceanExtent, VK_FORMAT_R32G32B32A32_SFLOAT, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT);
	surface.frequency_domain_texture = resource_manager->CreateImage(oceanExtent, VK_FORMAT_R32G32B32A32_SFLOAT, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT);
	surface.horizontal_displacement_map = resource_manager->CreateImage(oceanExtent, VK_FORMAT_R32G32B32A32_SFLOAT, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT);
	surface.height_derivative_texture = resource_manager->CreateImage(oceanExtent, VK_FORMAT_R32G32B32A32_SFLOAT, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT);
	surface.jacobian_XxZz_map = resource_manager->CreateImage(oceanExtent, VK_FORMAT_R32G32B32A32_SFLOAT, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT);
	surface.jacobian_xz_map = resource_manager->CreateImage(oceanExtent, VK_FORMAT_R32G32_SFLOAT, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT);
	std::string cubemap_path(assets_path + "/textures/");
	surface.sky_image = vkutil::load_cubemap_image(cubemap_path,engine, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT |VK_IMAGE_USAGE_SAMPLED_BIT );

	ocean_params.log_size = log2(RES);
	//Create default images
	uint32_t black = glm::packUnorm4x8(glm::vec4(0, 0, 0, 0));
	

	storage_image = resource_manager->CreateImage((void*)&black, VkExtent3D{ 1, 1, 1 }, VK_FORMAT_R8G8B8A8_UNORM,
		VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT);

	main_camera.type = Camera::CameraType::firstperson;
	//mainCamera.flipY = true;
	main_camera.movementSpeed = 12.5f;
	main_camera.setPerspective(45.0f, (float)_windowExtent.width / (float)_windowExtent.height, 0.1f, 1000.0f);
	main_camera.setPosition(glm::vec3(0.0f, -25.f, 0.0f));
	main_camera.setRotation(glm::vec3(-17.0f, 7.0f, 0.0f));

	VkSamplerCreateInfo sampl{};
	sampl.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;

	sampl.magFilter = VK_FILTER_NEAREST;
	sampl.minFilter = VK_FILTER_NEAREST;
	sampl.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	sampl.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;

	vkCreateSampler(engine->_device, &sampl, nullptr, &defaultSamplerNearest);

	sampl.magFilter = VK_FILTER_LINEAR;
	sampl.minFilter = VK_FILTER_LINEAR;
	vkCreateSampler(engine->_device, &sampl, nullptr, &samplerLinear);
	sampl.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	sampl.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;

	vkCreateSampler(engine->_device, &sampl, nullptr, &defaultSamplerLinear);



	VkSamplerCreateInfo cubeSampl{};
	cubeSampl.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
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
	cubeSampl.maxLod = (float)10;
	cubeSampl.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
	vkCreateSampler(engine->_device, &cubeSampl, nullptr, &cubeMapSampler);

	//< default_img

	_mainDeletionQueue.push_function([=]() {
		resource_manager->DestroyImage(surface.inital_spectrum_texture);
		resource_manager->DestroyImage(surface.butterfly_texture);
		resource_manager->DestroyImage(surface.horizontal_map);
		resource_manager->DestroyImage(surface.height_derivative);
		resource_manager->DestroyImage(surface.ping_1);
		resource_manager->DestroyImage(surface.normal_map);
		resource_manager->DestroyImage(storage_image);
		resource_manager->DestroyImage(_skyImage);
		vkDestroySampler(engine->_device, defaultSamplerLinear, nullptr);
		vkDestroySampler(engine->_device, defaultSamplerNearest, nullptr);
		vkDestroySampler(engine->_device, cubeMapSampler, nullptr);
		vkDestroySampler(engine->_device, samplerLinear, nullptr);
		});
}

void FFTRenderer::CreateSwapchain(uint32_t width, uint32_t height)
{
	vkb::SwapchainBuilder swapchainBuilder{ engine->_chosenGPU,engine->_device,engine->_surface };

	swapchain_image_format = VK_FORMAT_B8G8R8A8_UNORM;

	VkSurfaceFormatKHR surface{};
	surface.format = swapchain_image_format;
	surface.colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
	vkb::Swapchain vkbSwapchain = swapchainBuilder
		//.use_default_format_selection()
		.set_desired_format(surface)
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


void FFTRenderer::InitBuffers()
{
	
}


void FFTRenderer::InitImgui()
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

	VkPipelineRenderingCreateInfoKHR render_info{};
	render_info.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
	render_info.pNext = nullptr;
	//dynamic rendering parameters for imgui to use
	init_info.PipelineRenderingCreateInfo = render_info;
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

void FFTRenderer::DestroySwapchain()
{
	vkDestroySwapchainKHR(engine->_device, swapchain, nullptr);

	// destroy swapchain resources
	for (int i = 0; i < swapchain_image_views.size(); i++) {

		vkDestroyImageView(engine->_device, swapchain_image_views[i], nullptr);
	}
}

void FFTRenderer::UpdateScene()
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

	ocean_scene_data.cam_pos = camPos;
	ocean_scene_data.sun_direction = glm::vec3(scene_data.sunlightDirection);
	// invert the Y direction on projection matrix so that we are more similar
	// to opengl and gltf axis
	scene_data.proj[1][1] *= -1;
	scene_data.viewproj = scene_data.proj * scene_data.view;
	glm::mat4 model(1.0f);
	model = glm::translate(model, glm::vec3(0, 50, -500));
	model = glm::scale(model, glm::vec3(10, 10, 10));
	//sceneData.skyMat = model;
	scene_data.skyMat = scene_data.proj * glm::mat4(glm::mat3(scene_data.view));

}

void FFTRenderer::LoadAssets()
{
	
}

void FFTRenderer::Cleanup()
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

void FFTRenderer::GenerateInitialSpectrum(VkCommandBuffer cmd)
{
	//Generate intial spectrum
	VkDescriptorSet initial_spectrum_set = get_current_frame()._frameDescriptors.allocate(engine->_device, spectrum_layout);
	DescriptorWriter writer;
	writer.write_image(0, surface.inital_spectrum_texture.imageView, samplerLinear, VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
	writer.write_image(1, surface.wave_texture.imageView, samplerLinear, VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
	writer.write_image(2, surface.gaussian_noise_texture.imageView, samplerLinear, VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);

	writer.update_set(engine->_device, initial_spectrum_set);

	ocean_params.ocean_size = surface.grid_dimensions;
	ocean_params.resolution = surface.texture_dimensions;
	float wind_angle_rad = glm::radians(sim_params.wind_angle);
	ocean_params.wind = glm::vec2(sim_params.wind_magnitude, sim_params.wind_angle);
	ocean_params.depth = 500.0f;
	ocean_params.swell = 0.5f;
	ocean_params.fetch = 1000.0f * 1000.0f;

	vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, initial_spectrum_pso.pipeline);

	vkCmdPushConstants(cmd, initial_spectrum_pso.layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(FFTParams), &ocean_params);

	vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, initial_spectrum_pso.layout, 0, 1, &initial_spectrum_set, 0, nullptr);

	vkCmdDispatch(cmd, (surface.texture_dimensions / 32), (surface.texture_dimensions / 32) , 1);

	auto barrier = vkinit::image_barrier(surface.inital_spectrum_texture.image, VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_ASPECT_COLOR_BIT);
	image_barriers.push_back(barrier);

	vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_DEPENDENCY_BY_REGION_BIT, 0, 0, 0, 0, 1, &barrier);

	//Conjugate generated spectrum
	VkDescriptorSet conjugate_spectrum_set = get_current_frame()._frameDescriptors.allocate(engine->_device, image_blit_layout);
	DescriptorWriter writer_c;
	writer_c.write_image(0, surface.inital_spectrum_texture.imageView, samplerLinear, VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
	writer_c.write_image(1, surface.conjugated_spectrum_texture.imageView, samplerLinear, VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
	writer_c.update_set(engine->_device, conjugate_spectrum_set);

	vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, conjugate_spectrum_pso.pipeline);

	vkCmdPushConstants(cmd, conjugate_spectrum_pso.layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(FFTParams), &ocean_params);

	vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, conjugate_spectrum_pso.layout, 0, 1, &conjugate_spectrum_set, 0, nullptr);

	vkCmdDispatch(cmd, (surface.texture_dimensions / 32), (surface.texture_dimensions / 32), 1);

}

void FFTRenderer::GenerateSpectrum(VkCommandBuffer cmd)
{
	float currentFrame = glfwGetTime();
	float deltaTime = currentFrame - delta.lastFrame;
	delta.lastFrame = currentFrame;
	ocean_params.ocean_size = surface.grid_dimensions;
	ocean_params.resolution = surface.texture_dimensions;
	ocean_params.delta_time = currentFrame;

	VkDescriptorSet spectrum_set = get_current_frame()._frameDescriptors.allocate(engine->_device, spectrum_layout);
	DescriptorWriter writer;
	
	writer.write_image(0, surface.conjugated_spectrum_texture.imageView, samplerLinear, VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
	writer.write_image(1, surface.wave_texture.imageView, samplerLinear, VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
	writer.write_image(2, surface.frequency_domain_texture.imageView, samplerLinear, VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
	writer.write_image(3, surface.height_derivative_texture.imageView, samplerLinear, VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
	writer.write_image(4, surface.horizontal_displacement_map.imageView, samplerLinear, VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
	writer.write_image(5, surface.jacobian_XxZz_map.imageView, samplerLinear, VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
	writer.write_image(6, surface.jacobian_xz_map.imageView, samplerLinear, VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);

	writer.update_set(engine->_device, spectrum_set);

	vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, spectrum_pso.pipeline);

	vkCmdPushConstants(cmd, spectrum_pso.layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(FFTParams), &ocean_params);

	vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, spectrum_pso.layout, 0, 1, &spectrum_set, 0, nullptr);

	vkCmdDispatch(cmd, (surface.texture_dimensions / 32), (surface.texture_dimensions / 32), 1);
}

void FFTRenderer::DebugComputePass(VkCommandBuffer cmd)
{
	vkutil::transition_image(cmd, _drawImage.image, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL);
	vkutil::transition_image(cmd, surface.horizontal_map.image, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	vkutil::transition_image(cmd, surface.height_derivative.image, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	vkutil::transition_image(cmd, surface.inital_spectrum_texture.image, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	vkutil::transition_image(cmd, surface.conjugated_spectrum_texture.image, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	vkutil::transition_image(cmd, surface.frequency_domain_texture.image, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	vkutil::transition_image(cmd, surface.horizontal_displacement_map.image, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	vkutil::transition_image(cmd, surface.height_derivative_texture.image, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	vkutil::transition_image(cmd, surface.butterfly_texture.image, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	vkutil::transition_image(cmd, surface.height_map.image, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	vkutil::transition_image(cmd, surface.displacement_map.image, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);


	VkDescriptorSet debug_set = get_current_frame()._frameDescriptors.allocate(engine->_device, debug_layout);
	DescriptorWriter writer;
	writer.write_image(0, _drawImage.imageView, samplerLinear, VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
	writer.write_image(1, surface.height_derivative.imageView, samplerLinear, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
	writer.update_set(engine->_device, debug_set);

	//auto screen_dims = glm::vec2(_drawImage.imageExtent.width, _drawImage.imageExtent.height);
	vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, debug_pso.pipeline);

	//vkCmdPushConstants(cmd, debug_pso.layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(FFTParams), &ocean_params);

	vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, debug_pso.layout, 0, 1, &debug_set, 0, nullptr);

	vkCmdDispatch(cmd, (_drawImage.imageExtent.width / 32) + 1, (_drawImage.imageExtent.height / 32) + 1, 1);
	
	vkutil::transition_image(cmd, _drawImage.image, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
	vkutil::transition_image(cmd, surface.butterfly_texture.image, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL);
	vkutil::transition_image(cmd, surface.frequency_domain_texture.image, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL);
	vkutil::transition_image(cmd, surface.horizontal_displacement_map.image, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL);
	vkutil::transition_image(cmd, surface.height_derivative_texture.image, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL);
	vkutil::transition_image(cmd, surface.height_map.image, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL);
	vkutil::transition_image(cmd, surface.horizontal_map.image, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL);
	vkutil::transition_image(cmd, surface.conjugated_spectrum_texture.image, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,VK_IMAGE_LAYOUT_GENERAL);
	vkutil::transition_image(cmd, surface.height_derivative.image, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL);
	vkutil::transition_image(cmd, surface.displacement_map.image, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,VK_IMAGE_LAYOUT_GENERAL);


}

void FFTRenderer::DoIFFT(VkCommandBuffer cmd, AllocatedImage* input, AllocatedImage* output)
{
	AllocatedImage* ping_0 = input;
	int ping_pong = 0;
	ocean_params.log_size = log2(ocean_params.resolution);
	if (output != nullptr)
	{
		//Copy input to output if output is specified
		VkDescriptorSet copy_set = get_current_frame()._frameDescriptors.allocate(engine->_device, image_blit_layout);
		DescriptorWriter writer;

		writer.write_image(0, input->imageView, samplerLinear, VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
		writer.write_image(1, output->imageView, samplerLinear, VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);

		writer.update_set(engine->_device, copy_set);

		vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, copy_pso.pipeline);

		vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, copy_pso.layout, 0, 1, &copy_set, 0, nullptr);

		vkCmdDispatch(cmd, (surface.texture_dimensions / 32), (surface.texture_dimensions / 32), 1);

		auto barrier = vkinit::image_barrier(output->image, VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_ASPECT_COLOR_BIT);
		vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_DEPENDENCY_BY_REGION_BIT, 0, 0, 0, 0, 1, &barrier);
		ping_0 = output;
	}

	
	VkDescriptorSet fft_set = get_current_frame()._frameDescriptors.allocate(engine->_device, fft_layout);
	DescriptorWriter writer;

	writer.write_image(0, ping_0->imageView, samplerLinear, VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
	writer.write_image(1, surface.ping_1.imageView, samplerLinear, VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
	writer.write_image(2, surface.butterfly_texture.imageView, samplerLinear, VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);


	writer.update_set(engine->_device, fft_set);

	for (int stage = 0; stage < ocean_params.log_size; stage++)
	{
		ocean_params.ping_pong_count = ping_pong;
		ocean_params.stage = stage;

		vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, fft_horizontal_pso.pipeline);

		vkCmdPushConstants(cmd, fft_horizontal_pso.layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(FFTParams), &ocean_params);

		vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, fft_horizontal_pso.layout, 0, 1, &fft_set, 0, nullptr);

		vkCmdDispatch(cmd, (surface.texture_dimensions / 32), (surface.texture_dimensions / 32), 1);

		VkImageMemoryBarrier barrier;
		if (ping_pong == 0)
			barrier = vkinit::image_barrier(surface.ping_1.image, VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_ASPECT_COLOR_BIT);
		else
			barrier = vkinit::image_barrier(ping_0->image, VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_ASPECT_COLOR_BIT);

		vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_DEPENDENCY_BY_REGION_BIT, 0, 0, 0, 0, 1, &barrier);

		ping_pong = (ping_pong + 1) % 2;
	}

	for (int stage = 0; stage < ocean_params.log_size; stage++)
	{
		ocean_params.ping_pong_count = ping_pong;
		ocean_params.stage = stage;

		vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, fft_vertical_pso.pipeline);

		vkCmdPushConstants(cmd, fft_vertical_pso.layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(FFTParams), &ocean_params);

		vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, fft_vertical_pso.layout, 0, 1, &fft_set, 0, nullptr);

		vkCmdDispatch(cmd, (surface.texture_dimensions / 32), (surface.texture_dimensions / 32), 1);
		
		VkImageMemoryBarrier barrier;
		if(ping_pong == 0)
			barrier = vkinit::image_barrier(surface.ping_1.image, VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_ASPECT_COLOR_BIT);
		else
			barrier = vkinit::image_barrier(ping_0->image, VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_ASPECT_COLOR_BIT);
		
		vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_DEPENDENCY_BY_REGION_BIT, 0, 0, 0, 0, 1, &barrier);
		ping_pong = (ping_pong + 1) % 2;
	}
	
	
	
	//Copy output
	VkDescriptorSet copy_set = get_current_frame()._frameDescriptors.allocate(engine->_device, image_blit_layout);
	DescriptorWriter writer_copy;

	writer_copy.write_image(0, ping_0->imageView, samplerLinear, VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
	writer_copy.write_image(1, surface.ping_1.imageView, samplerLinear, VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);

	writer_copy.update_set(engine->_device, copy_set);

	vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, copy_pso.pipeline);

	vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, copy_pso.layout, 0, 1, &copy_set, 0, nullptr);

	vkCmdDispatch(cmd, (surface.texture_dimensions / 32), (surface.texture_dimensions / 32), 1);

	auto copy_barrier = vkinit::image_barrier(surface.ping_1.image, VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_ASPECT_COLOR_BIT);
	vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_DEPENDENCY_BY_REGION_BIT, 0, 0, 0, 0, 1, &copy_barrier);
	
	VkDescriptorSet permute_set = get_current_frame()._frameDescriptors.allocate(engine->_device, image_blit_layout);
	DescriptorWriter writer_permute;

	writer_permute.write_image(0, ping_0->imageView, samplerLinear, VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
	writer_permute.write_image(1, surface.ping_1.imageView, samplerLinear, VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);

	writer_permute.update_set(engine->_device, permute_set);

	vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, permute_scale_pso.pipeline);

	vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, permute_scale_pso.layout, 0, 1, &permute_set, 0, nullptr);

	vkCmdDispatch(cmd, (surface.texture_dimensions / 32), (surface.texture_dimensions / 32), 1);

	auto barrier_permute = vkinit::image_barrier(ping_0->image, VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_ASPECT_COLOR_BIT);
	vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_DEPENDENCY_BY_REGION_BIT, 0, 0, 0, 0, 1, &barrier_permute);
	
}

void FFTRenderer::WrapSpectrum(VkCommandBuffer cmd)
{
	float currentFrame = glfwGetTime();
	float deltaTime = currentFrame - delta.lastFrame;
	delta.lastFrame = currentFrame;
	ocean_params.ocean_size = surface.grid_dimensions;
	ocean_params.resolution = surface.texture_dimensions;
	ocean_params.delta_time = currentFrame;
	//ocean_params.displacement_factor

	VkDescriptorSet wrap_spectrum_set = get_current_frame()._frameDescriptors.allocate(engine->_device, wrap_spectrum_layout);
	DescriptorWriter writer;

	writer.write_image(0, surface.height_derivative.imageView, samplerLinear, VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
	writer.write_image(1, surface.height_map.imageView, samplerLinear, VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
	writer.write_image(2, surface.horizontal_map.imageView, samplerLinear, VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
	writer.write_image(3, surface.displacement_map.imageView, samplerLinear, VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);

	writer.update_set(engine->_device, wrap_spectrum_set);

	vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, wrap_spectrum_pso.pipeline);

	vkCmdPushConstants(cmd, wrap_spectrum_pso.layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(FFTParams), &ocean_params);

	vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, wrap_spectrum_pso.layout, 0, 1, &wrap_spectrum_set, 0, nullptr);

	vkCmdDispatch(cmd, (surface.texture_dimensions / 32), (surface.texture_dimensions / 32), 1);

}
void FFTRenderer::DrawMain(VkCommandBuffer cmd)
{
	if (sim_params.changed)
	{
		GenerateInitialSpectrum(cmd);
	}
	//PingPongPhasePass(cmd);

	GenerateSpectrum(cmd);

	sim_params.is_ping_phase = !sim_params.is_ping_phase;

	//Perform FFT on frequency textures
	DoIFFT(cmd, &surface.frequency_domain_texture, &surface.height_map);
	DoIFFT(cmd, &surface.height_derivative_texture, &surface.height_derivative);
	DoIFFT(cmd, &surface.horizontal_displacement_map, &surface.horizontal_map);
	WrapSpectrum(cmd);

	
	vkutil::transition_image(cmd, surface.displacement_map.image, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	vkutil::transition_image(cmd, surface.height_derivative.image, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

	VkClearValue geometryClear{ 1.0,1.0,1.0,1.0f };
	VkRenderingAttachmentInfo colorAttachment = vkinit::attachment_info(_drawImage.imageView,nullptr, &geometryClear, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, true);
	VkClearValue depthClear;
	depthClear.depthStencil.depth = 1.0f;
	VkRenderingAttachmentInfo depthAttachment = vkinit::attachment_info(_depthImage.imageView,nullptr, &depthClear, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,true);
	VkRenderingInfo renderInfo = vkinit::rendering_info(_windowExtent, &colorAttachment, &depthAttachment);

	vkCmdBeginRendering(cmd, &renderInfo);
	DrawOceanMesh(cmd);
	vkCmdEndRendering(cmd);

	vkutil::transition_image(cmd, surface.displacement_map.image, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL);
	vkutil::transition_image(cmd, surface.height_derivative.image, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL);

	image_barriers.clear();

	if (debug_texture)
		DebugComputePass(cmd);
}

void FFTRenderer::Draw()
{
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
	vkutil::transition_image(cmd, _depthImage.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);
	vkutil::transition_image(cmd, surface.inital_spectrum_texture.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
	vkutil::transition_image(cmd, surface.horizontal_map.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
	vkutil::transition_image(cmd, surface.height_derivative.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
	vkutil::transition_image(cmd, surface.ping_1.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
	vkutil::transition_image(cmd, surface.gaussian_noise_texture.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
	vkutil::transition_image(cmd, surface.wave_texture.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
	vkutil::transition_image(cmd, surface.conjugated_spectrum_texture.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
	vkutil::transition_image(cmd, surface.jacobian_XxZz_map.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
	vkutil::transition_image(cmd, surface.jacobian_xz_map.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
	vkutil::transition_image(cmd, surface.height_derivative_texture.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
	vkutil::transition_image(cmd, surface.frequency_domain_texture.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
	vkutil::transition_image(cmd, surface.horizontal_displacement_map.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
	vkutil::transition_image(cmd, surface.height_map.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
	vkutil::transition_image(cmd, surface.displacement_map.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);

	DrawBackground(cmd);
	//vkutil::transition_image(cmd, _drawImage.image, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL);

	DrawMain(cmd);

	vkutil::transition_image(cmd, _drawImage.image, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
	vkutil::transition_image(cmd, swapchain_images[swapchainImageIndex], VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

	//< draw_first
	//> imgui_draw
	// execute a copy from the draw image into the swapchain
	//vkutil::copy_image_to_image(cmd, surface.inital_spectrum_texture.image, swapchain_images[swapchainImageIndex], _drawExtent, _swapchainExtent);
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

void FFTRenderer::DrawPostProcess(VkCommandBuffer cmd)
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

void FFTRenderer::DrawBackground(VkCommandBuffer cmd)
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

void FFTRenderer::DrawImgui(VkCommandBuffer cmd, VkImageView targetImageView)
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

void FFTRenderer::Run()
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
}

void FFTRenderer::ResizeSwapchain()
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

void FFTRenderer::DrawUI()
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

	if (ImGui::CollapsingHeader("Ocean"))
	{
		bool wind_mag_changed = ImGui::SliderFloat("Wind Magnitude", &sim_params.wind_magnitude, 10.f, 50.f);
		bool wind_dir_changed = ImGui::SliderFloat("Wind Angle", &sim_params.wind_angle, 0, 359);

		ImGui::SliderFloat("Choppiness", &ocean_params.displacement_factor, 0.f, 7.5f);
		ImGui::SliderInt("Sun Elevation", &sim_params.sun_elevation, 0, 89);
		ImGui::SliderInt("Sun Azimuth", &sim_params.sun_azimuth, 0, 359);
		ImGui::Checkbox("Wireframe", &sim_params.wireframe);
		ImGui::Checkbox("Debug texture", &debug_texture);


		sim_params.changed = wind_mag_changed || wind_dir_changed;
	}
	if (ImGui::CollapsingHeader("Lighting"))
	{
		std::string index{};
		
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
	}
	ImGui::End();
}