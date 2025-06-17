#include "VkTexture.h"

#include "vulkan/vulkan.h"
#include "vk_mem_alloc.h"

void VK::CreateTexture(VkRenderTarget* target, VK::Texture& texture, uint32_t width, uint32_t height, uint8_t* rgba, uint32_t flags, VkFormat format)
{
    VkDeviceSize imageSize = width * height * 4;
    switch (format)
    {
    case VK_FORMAT_R8G8B8A8_UNORM: imageSize = width * height * 4; break;
    case VK_FORMAT_R8G8B8_UNORM:imageSize = width * height * 4; break;
    }

    VkBufferCreateInfo stagingBufInfo = { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
    stagingBufInfo.size = imageSize;
    stagingBufInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;

    VmaAllocationCreateInfo stagingBufAllocCreateInfo = {};
    stagingBufAllocCreateInfo.usage = VMA_MEMORY_USAGE_CPU_ONLY;
    stagingBufAllocCreateInfo.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT;

    VK::Buffer staging = {};
    VmaAllocationInfo stagingBufAllocInfo = {};
    vmaCreateBuffer(target->allocator, &stagingBufInfo, &stagingBufAllocCreateInfo, &staging.buffer, &staging.allocation, &stagingBufAllocInfo);

    unsigned char* const pImageData = (unsigned char*)stagingBufAllocInfo.pMappedData;
    switch (format)
    {
    case VK_FORMAT_R8G8B8A8_UNORM: memcpy(pImageData, rgba, imageSize); break;
    case VK_FORMAT_R8G8B8_UNORM:
    {
        for (uint32_t i = 0; i < width * height; ++i)
        {
            pImageData[i * 4 + 0] = rgba[i * 3 + 0];
            pImageData[i * 4 + 1] = rgba[i * 3 + 1];
            pImageData[i * 4 + 2] = rgba[i * 3 + 2];
            pImageData[i * 4 + 3] = 0xff;
        }
        format = VK_FORMAT_R8G8B8A8_UNORM;
        break;
    }
    }

    VkImageCreateInfo imageInfo = { VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent.width = width;
    imageInfo.extent.height = height;
    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.format = format;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.flags = 0;

    VmaAllocationCreateInfo imageAllocCreateInfo = {};
    imageAllocCreateInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;

    vmaCreateImage(target->allocator, &imageInfo, &imageAllocCreateInfo, &texture.image, &texture.allocation, nullptr);

    // Transition image layouts, copy image.

    target->BeginUploadCommands();

    VkImageMemoryBarrier imgMemBarrier = { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
    imgMemBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    imgMemBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    imgMemBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    imgMemBarrier.subresourceRange.baseMipLevel = 0;
    imgMemBarrier.subresourceRange.levelCount = 1;
    imgMemBarrier.subresourceRange.baseArrayLayer = 0;
    imgMemBarrier.subresourceRange.layerCount = 1;
    imgMemBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imgMemBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    imgMemBarrier.image = texture.image;
    imgMemBarrier.srcAccessMask = 0;
    imgMemBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

    vkCmdPipelineBarrier(
        target->GetUploadCmd(),
        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        0,
        0, nullptr,
        0, nullptr,
        1, &imgMemBarrier);

    VkBufferImageCopy region = {};
    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.layerCount = 1;
    region.imageExtent.width = width;
    region.imageExtent.height = height;
    region.imageExtent.depth = 1;

    vkCmdCopyBufferToImage(target->GetUploadCmd(), staging.buffer, texture.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

    imgMemBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    imgMemBarrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    imgMemBarrier.image = texture.image;
    imgMemBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    imgMemBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    vkCmdPipelineBarrier(
        target->GetUploadCmd(),
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
        0,
        0, nullptr,
        0, nullptr,
        1, &imgMemBarrier);

    target->PushSingleFrameBuffer(staging);
    // Create ImageView

    VkImageViewCreateInfo textureImageViewInfo = { VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
    textureImageViewInfo.image = texture.image;
    textureImageViewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    textureImageViewInfo.format = format;
    textureImageViewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    textureImageViewInfo.subresourceRange.baseMipLevel = 0;
    textureImageViewInfo.subresourceRange.levelCount = 1;
    textureImageViewInfo.subresourceRange.baseArrayLayer = 0;
    textureImageViewInfo.subresourceRange.layerCount = 1;
    vkCreateImageView(target->device, &textureImageViewInfo, nullptr, &texture.imageView);

    CreateTextureSampler(target, texture, flags);
}
void VK::CreateTexture(VkRenderTarget* target, VK::Texture& texture, uint32_t width, uint32_t height, uint32_t usage, uint32_t flags, VkFormat format)
{
    VkDeviceSize imageSize;
    switch (format)
    {
    case VK_FORMAT_R8G8B8A8_UNORM: imageSize = width * height * 4; break;
    case VK_FORMAT_R8G8B8_UNORM:imageSize = width * height * 4; break;
    }

    VkImageCreateInfo imageInfo = { VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent.width = width;
    imageInfo.extent.height = height;
    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.format = format;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | usage;
    if (format == VK_FORMAT_D32_SFLOAT)
        imageInfo.usage |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.flags = 0;

    VmaAllocationCreateInfo imageAllocCreateInfo = {};
    imageAllocCreateInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;

    vmaCreateImage(target->allocator, &imageInfo, &imageAllocCreateInfo, &texture.image, &texture.allocation, nullptr);

    // Create ImageView
    VkImageViewCreateInfo textureImageViewInfo = { VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
    textureImageViewInfo.image = texture.image;
    textureImageViewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    textureImageViewInfo.format = format;
    if (format == VK_FORMAT_D32_SFLOAT)
    {
        textureImageViewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    }
    else
    {
        textureImageViewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    }
    textureImageViewInfo.subresourceRange.baseMipLevel = 0;
    textureImageViewInfo.subresourceRange.levelCount = 1;
    textureImageViewInfo.subresourceRange.baseArrayLayer = 0;
    textureImageViewInfo.subresourceRange.layerCount = 1;
    vkCreateImageView(target->device, &textureImageViewInfo, nullptr, &texture.imageView);

    CreateTextureSampler(target, texture, flags);
}
void VK::CreateTextureSampler(VkRenderTarget* target, VK::Texture& texture, uint32_t flags) {
    VkSamplerCreateInfo samplerInfo = {};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    if (flags & VK::TexLinear)
    {
        samplerInfo.magFilter = VK_FILTER_LINEAR;
        samplerInfo.minFilter = VK_FILTER_LINEAR;
    }
    else
    {
        samplerInfo.magFilter = VK_FILTER_NEAREST;
        samplerInfo.minFilter = VK_FILTER_NEAREST;
    }

    if (flags & VK::TexClamp)
    {
        samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    }
    else
    {
        samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    }

    samplerInfo.anisotropyEnable = VK_FALSE;
    samplerInfo.maxAnisotropy = 16;

    samplerInfo.unnormalizedCoordinates = VK_FALSE;

    samplerInfo.compareEnable = VK_FALSE;
    samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;

    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerInfo.mipLodBias = 0.0f;
    samplerInfo.minLod = 0.0f;
    samplerInfo.maxLod = 0.0f;

    if (vkCreateSampler(target->device, &samplerInfo, nullptr, &texture.sampler) != VK_SUCCESS) {
    }
}

void VK::DestroyTexture(VkRenderTarget* target, VK::Texture& texture)
{
    vkDestroySampler(target->device, texture.sampler, nullptr);
    vkDestroyImageView(target->device, texture.imageView, nullptr);
    vmaDestroyImage(target->allocator, texture.image, texture.allocation);
}
