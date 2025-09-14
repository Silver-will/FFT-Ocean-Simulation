#pragma once
#include "vk_types.h"
#include "vk_renderer.h"
#include "vk_descriptors.h"
#include <unordered_map>
#include <filesystem>

class VulkanEngine;

struct SkyBoxPipelineResources {
	MaterialPipeline skyPipeline;

	VkDescriptorSetLayout materialLayout;

	DescriptorWriter writer;

	struct MaterialResources {
		AllocatedImage cubeMapImage;
		VkSampler cubeMapSampler;
		VkBuffer dataBuffer;
		uint32_t dataBufferOffset;
	};

	void build_pipelines(VulkanEngine* engine, PipelineCreationInfo& info);

	void clear_resources(VkDevice device);

	MaterialInstance write_material(VkDevice device, vkutil::MaterialPass pass, const MaterialResources& resources, DescriptorAllocatorGrowable& descriptorAllocator);
};


struct UpsamplePipelineObject {
	MaterialPipeline renderImagePipeline;
	VkDescriptorSetLayout materialLayout;

	DescriptorWriter writer;

	struct MaterialResources {
		VkSampler Sampler;
		VkBuffer dataBuffer;
		uint32_t dataBufferOffset;
	};

	void build_pipelines(VulkanEngine* engine, PipelineCreationInfo& info);

	void clear_resources(VkDevice device);
};