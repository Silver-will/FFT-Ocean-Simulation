#include "engine_psos.h"
#include "vk_pipelines.h"
#include "vk_engine.h"
#include "vk_initializers.h"
#include "vk_images.h"
#include <print>
#include <iostream>



void SkyBoxPipelineResources::build_pipelines(VulkanEngine* engine, PipelineCreationInfo& info)
{
	VkShaderModule skyVertexShader;
	if (!vkutil::load_shader_module("../../assets/shaders/skybox.vert.spv", engine->_device, &skyVertexShader)) {
		std::cout<<("Error when building the shadow vertex shader module\n");
	}

	VkShaderModule skyFragmentShader;
	if (!vkutil::load_shader_module("../../assets/shaders/skybox.frag.spv", engine->_device, &skyFragmentShader)) {
		std::cout <<("Error when building the shadow fragment shader module\n");
	}

	VkPushConstantRange matrixRange{};
	matrixRange.offset = 0;
	matrixRange.size = sizeof(GPUDrawPushConstants);
	matrixRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

	///VkDescriptorSetLayout layouts[] = { engine->_skyboxDescriptorLayout };

	VkPipelineLayoutCreateInfo sky_layout_info = vkinit::pipeline_layout_create_info();
	sky_layout_info.setLayoutCount = 1;
	sky_layout_info.pSetLayouts = info.layouts.data();
	sky_layout_info.pPushConstantRanges = &matrixRange;
	sky_layout_info.pushConstantRangeCount = 1;

	VkPipelineLayout newLayout;
	VK_CHECK(vkCreatePipelineLayout(engine->_device, &sky_layout_info, nullptr, &skyPipeline.layout));

	PipelineBuilder pipelineBuilder;
	pipelineBuilder.set_shaders(skyVertexShader, skyFragmentShader);
	pipelineBuilder.set_input_topology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
	pipelineBuilder.set_polygon_mode(VK_POLYGON_MODE_FILL);
	pipelineBuilder.set_cull_mode(VK_CULL_MODE_NONE, VK_FRONT_FACE_COUNTER_CLOCKWISE);
	pipelineBuilder.set_multisampling_level(engine->msaa_samples);
	pipelineBuilder.disable_blending();
	pipelineBuilder.enable_depthtest(true, true, VK_COMPARE_OP_LESS_OR_EQUAL);
	pipelineBuilder._pipelineLayout = skyPipeline.layout;

	pipelineBuilder.set_color_attachment_format(info.imageFormat);
	pipelineBuilder.set_depth_format(info.depthFormat);

	skyPipeline.pipeline = pipelineBuilder.build_pipeline(engine->_device);

	vkDestroyShaderModule(engine->_device, skyVertexShader, nullptr);
	vkDestroyShaderModule(engine->_device, skyFragmentShader, nullptr);
}

void SkyBoxPipelineResources::clear_resources(VkDevice device)
{
	vkDestroyDescriptorSetLayout(device, materialLayout, nullptr);
	vkDestroyPipelineLayout(device, skyPipeline.layout, nullptr);

	vkDestroyPipeline(device, skyPipeline.pipeline, nullptr);
}

MaterialInstance SkyBoxPipelineResources::write_material(VkDevice device, vkutil::MaterialPass pass, const MaterialResources& resources, DescriptorAllocatorGrowable& descriptorAllocator)
{
	MaterialInstance newmat;
	return newmat;
}


void RenderImagePipelineObject::build_pipelines(VulkanEngine* engine, PipelineCreationInfo& info)
{
	VkShaderModule HDRVertexShader;
	if (!vkutil::load_shader_module("../../assets/shaders/hdr.vert.spv", engine->_device, &HDRVertexShader)) {
		std::cout << ("Error when building the shadow vertex shader module\n");
	}

	VkShaderModule HDRFragmentShader;
	if (!vkutil::load_shader_module("../../assets/shaders/hdr.frag.spv", engine->_device, &HDRFragmentShader)) {
		std::cout << ("Error when building the shadow fragment shader module\n");
	}

	VkPushConstantRange matrixRange{};
	matrixRange.offset = 0;
	matrixRange.size = sizeof(PostProcessPushConstants);
	matrixRange.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

//	VkDescriptorSetLayout layouts[] = { engine->_drawImageDescriptorLayout };

	VkPipelineLayoutCreateInfo image_layout_info = vkinit::pipeline_layout_create_info();
	image_layout_info.setLayoutCount = 1;
	image_layout_info.pSetLayouts = info.layouts.data();
	image_layout_info.pPushConstantRanges = &matrixRange;
	image_layout_info.pushConstantRangeCount = 1;

	VkPipelineLayout newLayout;
	VK_CHECK(vkCreatePipelineLayout(engine->_device, &image_layout_info, nullptr, &renderImagePipeline.layout));

	PipelineBuilder pipelineBuilder;
	pipelineBuilder.set_shaders(HDRVertexShader, HDRFragmentShader);
	pipelineBuilder.set_input_topology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
	pipelineBuilder.set_polygon_mode(VK_POLYGON_MODE_FILL);
	pipelineBuilder.set_cull_mode(VK_CULL_MODE_FRONT_BIT, VK_FRONT_FACE_COUNTER_CLOCKWISE);
	pipelineBuilder.set_multisampling_none();
	pipelineBuilder.disable_blending();
	pipelineBuilder.enable_depthtest(false, false, VK_COMPARE_OP_GREATER_OR_EQUAL);
	pipelineBuilder._pipelineLayout = renderImagePipeline.layout;

	pipelineBuilder.set_color_attachment_format(info.imageFormat);

	renderImagePipeline.pipeline = pipelineBuilder.build_pipeline(engine->_device);

	vkDestroyShaderModule(engine->_device, HDRVertexShader, nullptr);
	vkDestroyShaderModule(engine->_device, HDRFragmentShader, nullptr);

}

void RenderImagePipelineObject::clear_resources(VkDevice device)
{
	vkDestroyDescriptorSetLayout(device, materialLayout, nullptr);
	vkDestroyPipelineLayout(device, renderImagePipeline.layout, nullptr);

	vkDestroyPipeline(device, renderImagePipeline.pipeline, nullptr);
}

void FFTPipelineObject::build_pipelines(VulkanEngine* engine, PipelineCreationInfo& info)
{
	std::string assets_path = ENGINE_ASSET_PATH;
	VkShaderModule oceanVertexShader;
	if (!vkutil::load_shader_module(std::string(assets_path + "/shaders/ocean.vert.spv").c_str(), engine->_device, &oceanVertexShader)) {
		std::cout << ("Error when building the shadow vertex shader module\n");
	}

	VkShaderModule oceanFragmentShader;
	if (!vkutil::load_shader_module(std::string(assets_path + "/shaders/ocean.frag.spv").c_str(), engine->_device, &oceanFragmentShader)) {
		std::cout << ("Error when building the shadow fragment shader module\n");
	}


	VkPushConstantRange matrixRange{};
	matrixRange.offset = 0;
	matrixRange.size = sizeof(GPUDrawPushConstants);
	matrixRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

	//VkDescriptorSetLayout layouts[] = { engine->gpu_scene_data_descriptor_layout };

	VkPipelineLayoutCreateInfo mesh_layout_info = vkinit::pipeline_layout_create_info();
	mesh_layout_info.setLayoutCount = info.layouts.size();
	mesh_layout_info.pSetLayouts = info.layouts.data();
	mesh_layout_info.pPushConstantRanges = &matrixRange;
	mesh_layout_info.pushConstantRangeCount = 1;

	VkPipelineLayout newLayout;
	VK_CHECK(vkCreatePipelineLayout(engine->_device, &mesh_layout_info, nullptr, &newLayout));

	FFTOceanPipeline.layout = newLayout;

	PipelineBuilder pipelineBuilder;
	pipelineBuilder.set_shaders(oceanVertexShader, oceanFragmentShader);
	pipelineBuilder.set_input_topology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
	pipelineBuilder.set_polygon_mode(VK_POLYGON_MODE_FILL);
	pipelineBuilder.set_cull_mode(VK_CULL_MODE_NONE, VK_FRONT_FACE_COUNTER_CLOCKWISE);
	pipelineBuilder.set_multisampling_level(VK_SAMPLE_COUNT_1_BIT);
	pipelineBuilder.disable_blending();
	pipelineBuilder.enable_depthtest(true, true, VK_COMPARE_OP_LESS_OR_EQUAL);
	pipelineBuilder.set_depth_format(info.depthFormat);

	pipelineBuilder.set_color_attachment_format(info.imageFormat);

	pipelineBuilder._pipelineLayout = newLayout;

	FFTOceanPipeline.pipeline = pipelineBuilder.build_pipeline(engine->_device);

	vkDestroyShaderModule(engine->_device, oceanVertexShader, nullptr);
	vkDestroyShaderModule(engine->_device, oceanFragmentShader, nullptr);
}

void FFTPipelineObject::clear_resources(VkDevice device)
{
	vkDestroyPipelineLayout(device, FFTOceanPipeline.layout, nullptr);

	vkDestroyPipeline(device, FFTOceanPipeline.pipeline, nullptr);
}