#include "resource_manager.h"
#include "stb_image.h"
#include "vk_engine.h"
#include <print>

#define USE_BINDLESS

void ResourceManager::init(VulkanEngine* engine_ptr) {
    engine = engine_ptr;

    std::vector<DescriptorAllocator::PoolSizeRatio> bindless_sizes = {
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1 },
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1 },
        { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1 },
    };
    bindless_material_descriptor.init_pool(engine->_device, 65536, bindless_sizes, true);

    //Create default images
    uint32_t white = glm::packUnorm4x8(glm::vec4(1, 1, 1, 1));
    _whiteImage = CreateImage((void*)&white, VkExtent3D{ 1, 1, 1 }, VK_FORMAT_R8G8B8A8_UNORM,
        VK_IMAGE_USAGE_SAMPLED_BIT);

    uint32_t grey = glm::packUnorm4x8(glm::vec4(0.66f, 0.66f, 0.66f, 1));
    _greyImage = CreateImage((void*)&grey, VkExtent3D{ 1, 1, 1 }, VK_FORMAT_R8G8B8A8_UNORM,
        VK_IMAGE_USAGE_SAMPLED_BIT);

    uint32_t black = glm::packUnorm4x8(glm::vec4(0, 0, 0, 0));
    _blackImage = CreateImage((void*)&black, VkExtent3D{ 1, 1, 1 }, VK_FORMAT_R8G8B8A8_UNORM,
        VK_IMAGE_USAGE_SAMPLED_BIT);

    uint32_t magenta = glm::packUnorm4x8(glm::vec4(1, 0, 1, 1));
    std::array<uint32_t, 16 * 16 > pixels; //for 16x16 checkerboard texture
    for (int x = 0; x < 16; x++) {
        for (int y = 0; y < 16; y++) {
            pixels[y * 16 + x] = ((x % 2) ^ (y % 2)) ? magenta : black;
        }
    }

    errorCheckerboardImage = CreateImage(pixels.data(), VkExtent3D{ 16, 16, 1 }, VK_FORMAT_R8G8B8A8_UNORM,
        VK_IMAGE_USAGE_SAMPLED_BIT);

    VkSamplerCreateInfo sampl = { .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO };

    sampl.magFilter = VK_FILTER_NEAREST;
    sampl.minFilter = VK_FILTER_NEAREST;

    vkCreateSampler(engine->_device, &sampl, nullptr, &defaultSamplerNearest);

    sampl.magFilter = VK_FILTER_LINEAR;
    sampl.minFilter = VK_FILTER_LINEAR;
    vkCreateSampler(engine->_device, &sampl, nullptr, &defaultSamplerLinear);
    
}


std::optional<AllocatedImage> ResourceManager::LoadImage(std::string_view filePath, VkFormat format)
{
    AllocatedImage newImage{};

    int width, height, nrChannels;
   
    std::string path_s(filePath);
    unsigned char* data = stbi_load(path_s.c_str(), &width, &height, &nrChannels, 4);
    if (data) {
        VkExtent3D imagesize;
        imagesize.width = width;
        imagesize.height = height;
        imagesize.depth = 1;

        newImage = vkutil::create_image(data, imagesize, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT, engine, true);

        stbi_image_free(data);
    }

    if (newImage.image == VK_NULL_HANDLE) {
        return {};
    }
    else {
        return newImage;
    }
}


ResourceManager::ResourceManager(VulkanEngine* engine)
{
    this->engine = engine;

    std::vector<DescriptorAllocator::PoolSizeRatio> bindless_sizes = {
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1 },
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1 },
        { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1 },
    };
    bindless_material_descriptor.init_pool(engine->_device, 65536, bindless_sizes, true);

    //Create default images
    uint32_t white = glm::packUnorm4x8(glm::vec4(1, 1, 1, 1));
    _whiteImage = CreateImage((void*)&white, VkExtent3D{ 1, 1, 1 }, VK_FORMAT_R8G8B8A8_UNORM,
        VK_IMAGE_USAGE_SAMPLED_BIT);

    uint32_t grey = glm::packUnorm4x8(glm::vec4(0.66f, 0.66f, 0.66f, 1));
    _greyImage = CreateImage((void*)&grey, VkExtent3D{ 1, 1, 1 }, VK_FORMAT_R8G8B8A8_UNORM,
        VK_IMAGE_USAGE_SAMPLED_BIT);

    uint32_t black = glm::packUnorm4x8(glm::vec4(0, 0, 0, 0));
    _blackImage = CreateImage((void*)&black, VkExtent3D{ 1, 1, 1 }, VK_FORMAT_R8G8B8A8_UNORM,
        VK_IMAGE_USAGE_SAMPLED_BIT);

    storageImage = CreateImage((void*)&black, VkExtent3D{ 1, 1, 1 }, VK_FORMAT_R8G8B8A8_UNORM,
        VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT);


    uint32_t magenta = glm::packUnorm4x8(glm::vec4(1, 0, 1, 1));
    std::array<uint32_t, 16 * 16 > pixels; //for 16x16 checkerboard texture
    for (int x = 0; x < 16; x++) {
        for (int y = 0; y < 16; y++) {
            pixels[y * 16 + x] = ((x % 2) ^ (y % 2)) ? magenta : black;
        }
    }

    errorCheckerboardImage = CreateImage(pixels.data(), VkExtent3D{ 16, 16, 1 }, VK_FORMAT_R8G8B8A8_UNORM,
        VK_IMAGE_USAGE_SAMPLED_BIT);

    VkSamplerCreateInfo sampl = { .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO };

    sampl.magFilter = VK_FILTER_NEAREST;
    sampl.minFilter = VK_FILTER_NEAREST;

    vkCreateSampler(engine->_device, &sampl, nullptr, &defaultSamplerNearest);

    sampl.magFilter = VK_FILTER_LINEAR;
    sampl.minFilter = VK_FILTER_LINEAR;
    vkCreateSampler(engine->_device, &sampl, nullptr, &defaultSamplerLinear);
}
void ResourceManager::cleanup()
{
    vkDestroyDescriptorPool(engine->_device, bindless_material_descriptor.pool, nullptr);
    deletionQueue.flush();
}



void ResourceManager::write_material_array()
{
    writer.clear();
    for (int i = 0; i < bindless_resources.size(); i++)
    {
        int offset = i * 4;
        writer.write_buffer(0, bindless_resources[i].dataBuffer, sizeof(GLTFMetallic_Roughness::MaterialConstants), bindless_resources[i].dataBufferOffset, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 2);
        writer.write_image(1, bindless_resources[i].colorImage.imageView, bindless_resources[i].colorSampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, offset);
        writer.write_image(1, bindless_resources[i].metalRoughImage.imageView, bindless_resources[i].metalRoughSampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, offset + 1);
        writer.write_image(1, bindless_resources[i].normalImage.imageView, bindless_resources[i].normalSampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, offset + 2);
        writer.write_image(1, bindless_resources[i].occlusionImage.imageView, bindless_resources[i].occlusionSampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, offset + 3);
        writer.write_image(2, storageImage.imageView, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, i);
    }

    bindless_set = bindless_material_descriptor.allocate(engine->_device, bindless_descriptor_layout);
    writer.update_set(engine->_device, bindless_set);
}

VkDescriptorSet* ResourceManager::GetBindlessSet()
{
    VkDescriptorSet* desc = &bindless_set;
    return desc;
}

void ResourceManager::ReadBackBufferData(VkCommandBuffer cmd, AllocatedBuffer* buffer)
{
    if (readBackBufferInitialized == true)
    {
        DestroyBuffer(readableBuffer);
    }

    readableBuffer =  CreateBuffer(buffer->info.size, VK_BUFFER_USAGE_TRANSFER_DST_BIT, VMA_MEMORY_USAGE_CPU_ONLY);
    readBackBufferInitialized = true;

    VkBufferCopy dataCopy{ 0 };
    dataCopy.dstOffset = 0;
    dataCopy.srcOffset = 0;
    dataCopy.size = buffer->info.size -64;

    vkCmdCopyBuffer(cmd, buffer->buffer, readableBuffer.buffer, 1, &dataCopy);

}

AllocatedBuffer* ResourceManager::GetReadBackBuffer()
{
    return &readableBuffer;
}


AllocatedBuffer ResourceManager::CreateBuffer(size_t allocSize, VkBufferUsageFlags usage, VmaMemoryUsage memoryUsage)
{
    // allocate buffer
    VkBufferCreateInfo bufferInfo = { .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
    bufferInfo.pNext = nullptr;
    bufferInfo.size = allocSize;


    bufferInfo.usage = usage;

    VmaAllocationCreateInfo vmaallocInfo = {};
    vmaallocInfo.usage = memoryUsage;
    vmaallocInfo.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT;
    AllocatedBuffer newBuffer;

    // allocate the buffer
    VK_CHECK(vmaCreateBuffer(engine->_allocator, &bufferInfo, &vmaallocInfo, &newBuffer.buffer, &newBuffer.allocation,
        &newBuffer.info));

    deletionQueue.push_function([=]() {
        DestroyBuffer(newBuffer);
      });
    return newBuffer;
}

AllocatedBuffer ResourceManager::CreateAndUpload(size_t allocSize, VkBufferUsageFlags usage, VmaMemoryUsage memoryUsage, void* data)
{
    AllocatedBuffer stagingBuffer = CreateBuffer(allocSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY);


    void* bufferData = nullptr;
    vmaMapMemory(engine->_allocator, stagingBuffer.allocation, &bufferData);
    memcpy(bufferData, data, allocSize);

    AllocatedBuffer dataBuffer = CreateBuffer(allocSize, usage, memoryUsage);

    engine->immediate_submit([&](VkCommandBuffer cmd) {
        VkBufferCopy dataCopy{ 0 };
        dataCopy.dstOffset = 0;
        dataCopy.srcOffset = 0;
        dataCopy.size = allocSize;

        vkCmdCopyBuffer(cmd, stagingBuffer.buffer, dataBuffer.buffer, 1, &dataCopy);
        });
    vmaUnmapMemory(engine->_allocator, stagingBuffer.allocation);
    DestroyBuffer(stagingBuffer);

    deletionQueue.push_function([=]() {
        DestroyBuffer(dataBuffer);
        });

    return dataBuffer;
}

void ResourceManager::DestroyBuffer(const AllocatedBuffer& buffer)
{
    vmaDestroyBuffer(engine->_allocator, buffer.buffer, buffer.allocation);
}


GPUMeshBuffers ResourceManager::UploadMesh(std::span<uint32_t> indices, std::span<Vertex> vertices)
{
    const size_t vertexBufferSize = vertices.size() * sizeof(Vertex);
    const size_t indexBufferSize = indices.size() * sizeof(uint32_t);

    GPUMeshBuffers newSurface;

    newSurface.vertexBuffer = CreateBuffer(vertexBufferSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        VMA_MEMORY_USAGE_GPU_ONLY);


    VkBufferDeviceAddressInfo deviceAdressInfo{ .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,.buffer = newSurface.vertexBuffer.buffer };
    newSurface.vertexBufferAddress = vkGetBufferDeviceAddress(engine->_device, &deviceAdressInfo);

    newSurface.indexBuffer = CreateBuffer(indexBufferSize, VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VMA_MEMORY_USAGE_GPU_ONLY);

    AllocatedBuffer staging = CreateBuffer(vertexBufferSize + indexBufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY);

    void* data = nullptr;
    vmaMapMemory(engine->_allocator, staging.allocation, &data);
    // copy vertex buffer
    memcpy(data, vertices.data(), vertexBufferSize);
    // copy index buffer
    memcpy((char*)data + vertexBufferSize, indices.data(), indexBufferSize);

    engine->immediate_submit([&](VkCommandBuffer cmd) {
        VkBufferCopy vertexCopy{ 0 };
        vertexCopy.dstOffset = 0;
        vertexCopy.srcOffset = 0;
        vertexCopy.size = vertexBufferSize;

        vkCmdCopyBuffer(cmd, staging.buffer, newSurface.vertexBuffer.buffer, 1, &vertexCopy);

        VkBufferCopy indexCopy{ 0 };
        indexCopy.dstOffset = 0;
        indexCopy.srcOffset = vertexBufferSize;
        indexCopy.size = indexBufferSize;

        vkCmdCopyBuffer(cmd, staging.buffer, newSurface.indexBuffer.buffer, 1, &indexCopy);
        });

    vmaUnmapMemory(engine->_allocator, staging.allocation);
    DestroyBuffer(staging);

    return newSurface;

}

AllocatedImage ResourceManager::CreateImage(VkExtent3D size, VkFormat format, VkImageUsageFlags usage, bool mipmapped)
{
    AllocatedImage newImage;
    newImage.imageFormat = format;
    newImage.imageExtent = size;

    VkImageCreateInfo img_info = vkinit::image_create_info(format, usage, size);
    if (mipmapped) {
        img_info.mipLevels = static_cast<uint32_t>(std::floor(std::log2(std::max(size.width, size.height)))) + 1;
    }

    // always allocate images on dedicated GPU memory
    VmaAllocationCreateInfo allocinfo = {};
    allocinfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
    allocinfo.requiredFlags = VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    // allocate and create the image
    VK_CHECK(vmaCreateImage(engine->_allocator, &img_info, &allocinfo, &newImage.image, &newImage.allocation, nullptr));

    // if the format is a depth format, we will need to have it use the correct
    // aspect flag
    VkImageAspectFlags aspectFlag = VK_IMAGE_ASPECT_COLOR_BIT;
    if (format == VK_FORMAT_D32_SFLOAT) {
        aspectFlag = VK_IMAGE_ASPECT_DEPTH_BIT;
    }

    // build a image-view for the image
    VkImageViewCreateInfo view_info = vkinit::imageview_create_info(format, newImage.image, aspectFlag, VK_IMAGE_VIEW_TYPE_2D);
    view_info.subresourceRange.levelCount = img_info.mipLevels;

    VK_CHECK(vkCreateImageView(engine->_device, &view_info, nullptr, &newImage.imageView));

    deletionQueue.push_function([=]() {
        DestroyImage(newImage);
        });
    return newImage;
}


AllocatedImage ResourceManager::CreateImageEmpty(VkExtent3D size, VkFormat format, VkImageUsageFlags usage,VkImageViewType viewType, bool mipmapped, int layers, VkSampleCountFlagBits msaaSamples, int mipLevels)
{
    AllocatedImage newImage;
    newImage.imageFormat = format;
    newImage.imageExtent = size;

    VkImageCreateInfo img_info = vkinit::image_create_info(format, usage, size, layers, msaaSamples);
    if (mipmapped) {
        img_info.mipLevels = mipLevels != -1 ? mipLevels : static_cast<uint32_t>(std::floor(std::log2(std::max(size.width, size.height)))) + 1;
    }

    // always allocate images on dedicated GPU memory
    VmaAllocationCreateInfo allocinfo = {};
    allocinfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
    allocinfo.requiredFlags = VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    // allocate and create the image
    VK_CHECK(vmaCreateImage(engine->_allocator, &img_info, &allocinfo, &newImage.image, &newImage.allocation, nullptr));

    // if the format is a depth format, we will need to have it use the correct
    // aspect flag
    VkImageAspectFlags aspectFlag = VK_IMAGE_ASPECT_COLOR_BIT;
    if (format == VK_FORMAT_D32_SFLOAT) {
        aspectFlag = VK_IMAGE_ASPECT_DEPTH_BIT;
    }

    // build a image-view for the image
    VkImageViewCreateInfo view_info = vkinit::imageview_create_info(format, newImage.image, aspectFlag, viewType, layers);
    view_info.subresourceRange.levelCount = img_info.mipLevels;

    VK_CHECK(vkCreateImageView(engine->_device, &view_info, nullptr, &newImage.imageView));

    return newImage;
}


AllocatedImage ResourceManager::CreateImage(void* data, VkExtent3D size, VkFormat format, VkImageUsageFlags usage,uint32_t byte_size, bool mipmapped)
{
    //Always assumes the texture is 4 components large
    size_t data_size = size.depth * size.width * size.height * byte_size;
    AllocatedBuffer uploadbuffer = CreateBuffer(data_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);

    memcpy(uploadbuffer.info.pMappedData, data, data_size);

    AllocatedImage new_image = vkutil::create_image_empty(size, format, usage | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT, engine, VK_IMAGE_VIEW_TYPE_2D, mipmapped);


    engine->immediate_submit([&](VkCommandBuffer cmd) {
        vkutil::transition_image(cmd, new_image.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

        VkBufferImageCopy copyRegion = {};
        copyRegion.bufferOffset = 0;
        copyRegion.bufferRowLength = 0;
        copyRegion.bufferImageHeight = 0;

        copyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        copyRegion.imageSubresource.mipLevel = 0;
        copyRegion.imageSubresource.baseArrayLayer = 0;
        copyRegion.imageSubresource.layerCount = 1;
        copyRegion.imageExtent = size;


        // copy the buffer into the image
        vkCmdCopyBufferToImage(cmd, uploadbuffer.buffer, new_image.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1,
            &copyRegion);

        if (mipmapped) {
            vkutil::generate_mipmaps(cmd, new_image.image, VkExtent2D{ new_image.imageExtent.width,new_image.imageExtent.height });
        }
        else {
            if (format == VK_IMAGE_USAGE_SAMPLED_BIT)
                vkutil::transition_image(cmd, new_image.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
            else if (format == VK_IMAGE_USAGE_STORAGE_BIT)
                vkutil::transition_image(cmd, new_image.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL);
            else if (format == VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT)
                vkutil::transition_image(cmd, new_image.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

            else
                vkutil::transition_image(cmd, new_image.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        }
        });
    DestroyBuffer(uploadbuffer);
    return new_image;
}

void ResourceManager::DestroyImage(const AllocatedImage& img)
{
    vkDestroyImageView(engine->_device, img.imageView, nullptr);
    vmaDestroyImage(engine->_allocator, img.image, img.allocation);
}

void ResourceManager::DestroyPSO(PipelineStateObject& pso)
{
    vkDestroyPipelineLayout(engine->_device, pso.layout, nullptr);
    vkDestroyPipeline(engine->_device, pso.pipeline, nullptr);
}