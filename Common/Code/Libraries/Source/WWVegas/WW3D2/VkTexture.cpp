#include "VkTexture.h"

#include "vulkan/vulkan.h"
#include "vk_mem_alloc.h"

void VK::CreateTexture(VkRenderTarget* target, VK::Texture& texture, uint32_t width, uint32_t height, uint8_t* rgba, 
    VK::SamplerSettings settings, VkFormat format)
{
    texture.width = width;
    texture.height = height;
    texture.mips = 1;

    VkDeviceSize imageSize = width * height * VK::SizeOfFormat(format);
    switch (format)
    {
    case VK_FORMAT_R8G8B8A8_UNORM: imageSize = width * height * 4; break;
    case VK_FORMAT_R8G8B8_UNORM:imageSize = width * height * 4; break;
    }
    VkFormatProperties formatProperties;
    vkGetPhysicalDeviceFormatProperties(target->physicalDevice, format, &formatProperties);
    if ((formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT) != 0) {
        // VK_FORMAT_R8G8B8A8_UNORM supports being sampled from an optimal-tiled image.
    }
    else {
        switch (format)
        {
        case VK_FORMAT_B8G8R8_UNORM:
            break;
        default:
            printf("Bad Texture Format\n");
            break;
        }
        // Not supported.
    }

    VkBufferCreateInfo stagingBufInfo = { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
    stagingBufInfo.size = imageSize;
    stagingBufInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;

    VmaAllocationCreateInfo stagingBufAllocCreateInfo = {};
    stagingBufAllocCreateInfo.usage = VMA_MEMORY_USAGE_CPU_ONLY;
    stagingBufAllocCreateInfo.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT;

    VK::Buffer staging = target->vmaPools_.transfer.alloc(nullptr, imageSize);
    void* mapped;
    VkResult mr = vmaMapMemory(target->allocator, staging.allocation, &mapped);

    unsigned char* const pImageData = (unsigned char*)mapped;
    switch (format)
    {
    default:
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
    case VK_FORMAT_B8G8R8_UNORM:
    {
        for (uint32_t i = 0; i < width * height; ++i)
        {
            pImageData[i * 4 + 0] = rgba[i * 3 + 0];
            pImageData[i * 4 + 1] = rgba[i * 3 + 1];
            pImageData[i * 4 + 2] = rgba[i * 3 + 2];
            pImageData[i * 4 + 3] = 0xff;
        }
        format = VK_FORMAT_B8G8R8A8_UNORM;
        break;
    }
    case VK_FORMAT_B8G8R8A8_SRGB:
    {
        for (uint32_t i = 0; i < width * height; ++i)
        {
            pImageData[i * 4 + 0] = rgba[i * 4 + 0];
            pImageData[i * 4 + 1] = rgba[i * 4 + 1];
            pImageData[i * 4 + 2] = rgba[i * 4 + 2];
            pImageData[i * 4 + 3] = 0xff;
        }
        format = VK_FORMAT_B8G8R8A8_UNORM;
        break;
    }
    }
    vmaUnmapMemory(target->allocator, staging.allocation);
    texture.format = format;

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

    VkImageViewUsageCreateInfo tivci = {};
    tivci.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_USAGE_CREATE_INFO;
    tivci.usage = VK_IMAGE_USAGE_SAMPLED_BIT;
    VkImageViewCreateInfo textureImageViewInfo = { VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
    textureImageViewInfo.pNext = &tivci;
    textureImageViewInfo.image = texture.image;
    textureImageViewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    textureImageViewInfo.format = format;
    textureImageViewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    textureImageViewInfo.subresourceRange.baseMipLevel = 0;
    textureImageViewInfo.subresourceRange.levelCount = 1;
    textureImageViewInfo.subresourceRange.baseArrayLayer = 0;
    textureImageViewInfo.subresourceRange.layerCount = 1;
    vkCreateImageView(target->device, &textureImageViewInfo, nullptr, &texture.imageView);

    CreateTextureSampler(target, texture, settings);
}
void VK::CreateTexture(VkRenderTarget* target, VK::Texture& texture, uint32_t width, uint32_t height, uint32_t usage, 
    VK::SamplerSettings settings, VkFormat format)
{
    VkDeviceSize imageSize;
    switch (format)
    {
    case VK_FORMAT_R8G8B8A8_UNORM: imageSize = width * height * 4; break;
    case VK_FORMAT_R8G8B8_UNORM:imageSize = width * height * 4; break;
    }
    texture.format = format;

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
    if (format == VK_FORMAT_D24_UNORM_S8_UINT)
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
    if (format == VK_FORMAT_D24_UNORM_S8_UINT)
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

    CreateTextureSampler(target, texture, settings);
}
void VK::CreateTextureSampler(VkRenderTarget* target, VK::Texture& texture, VK::SamplerSettings settings, uint32_t mips) {
    VkSamplerCreateInfo samplerInfo = {};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    texture.sampSettings = settings;
    samplerInfo.minFilter = (VkFilter)settings.minF;
    samplerInfo.magFilter = (VkFilter)settings.maxF;
    samplerInfo.mipmapMode = (VkSamplerMipmapMode)settings.minF;
    samplerInfo.addressModeU = (VkSamplerAddressMode)settings.addU;
    samplerInfo.addressModeV = (VkSamplerAddressMode)settings.addV;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;

    samplerInfo.anisotropyEnable = VK_FALSE;
    samplerInfo.maxAnisotropy = 16;

    samplerInfo.unnormalizedCoordinates = VK_FALSE;

    samplerInfo.compareEnable = VK_FALSE;
    samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;

    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerInfo.mipLodBias = 0.0f;
    samplerInfo.minLod = 0.0f;
    samplerInfo.maxLod = (float)mips;

    if (vkCreateSampler(target->device, &samplerInfo, nullptr, &texture.sampler) != VK_SUCCESS) {
    }
}

void VK::DestroyTexture(VkRenderTarget* target, VK::Texture& texture)
{
    vkDestroySampler(target->device, texture.sampler, nullptr);
    vkDestroyImageView(target->device, texture.imageView, nullptr);
    vmaDestroyImage(target->allocator, texture.image, texture.allocation);
}

void generateMipmaps(VkRenderTarget* target, VkImage image, VkFormat imageFormat, int32_t texWidth, int32_t texHeight, uint32_t mipLevels) {

    // Check if image format supports linear blitting
        // Check if image format supports linear blitting
    VkFormatProperties formatProperties;
    vkGetPhysicalDeviceFormatProperties(target->physicalDevice, imageFormat, &formatProperties);

    if (!(formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT)) {
        //throw std::runtime_error("texture image format does not support linear blitting!");
    }

    VkCommandBuffer commandBuffer = target->GetUploadCmd();

    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.image = image;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;
    barrier.subresourceRange.levelCount = 1;

    int32_t mipWidth = texWidth;
    int32_t mipHeight = texHeight;

    for (uint32_t i = 1; i < mipLevels; i++) {
        barrier.subresourceRange.baseMipLevel = i - 1;
        barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

        vkCmdPipelineBarrier(commandBuffer,
            VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0,
            0, nullptr,
            0, nullptr,
            1, &barrier);

        VkImageBlit blit{};
        blit.srcOffsets[0] = { 0, 0, 0 };
        blit.srcOffsets[1] = { mipWidth, mipHeight, 1 };
        blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        blit.srcSubresource.mipLevel = i - 1;
        blit.srcSubresource.baseArrayLayer = 0;
        blit.srcSubresource.layerCount = 1;
        blit.dstOffsets[0] = { 0, 0, 0 };
        blit.dstOffsets[1] = { mipWidth > 1 ? mipWidth / 2 : 1, mipHeight > 1 ? mipHeight / 2 : 1, 1 };
        blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        blit.dstSubresource.mipLevel = i;
        blit.dstSubresource.baseArrayLayer = 0;
        blit.dstSubresource.layerCount = 1;

        vkCmdBlitImage(commandBuffer,
            image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            1, &blit,
            VK_FILTER_LINEAR);

        barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

        vkCmdPipelineBarrier(commandBuffer,
            VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0,
            0, nullptr,
            0, nullptr,
            1, &barrier);

        if (mipWidth > 1) mipWidth /= 2;
        if (mipHeight > 1) mipHeight /= 2;
    }

    barrier.subresourceRange.baseMipLevel = mipLevels - 1;
    barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    vkCmdPipelineBarrier(commandBuffer,
        VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0,
        0, nullptr,
        0, nullptr,
        1, &barrier);
}

void VK::CreateTextureMips(VkRenderTarget* target, VK::Texture& texture, uint32_t mips, uint32_t width, uint32_t height, 
    uint8_t* rgba, VK::SamplerSettings settings, VkFormat format)
{
    VkFormatProperties formatProperties;
    vkGetPhysicalDeviceFormatProperties(target->physicalDevice, format, &formatProperties);

    if (!(formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_2_BLIT_DST_BIT)
        || mips <= 1
        ) {
        VK::CreateTexture(target, texture, width, height, rgba, settings, format);
        return;
        //throw std::runtime_error("texture image format does not support linear blitting!");
    }
    texture.width = width;
    texture.height = height;
    texture.mips = mips;

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

    VK::Buffer staging = target->vmaPools_.transfer.alloc(nullptr, imageSize);
    void* mapped;
    VkResult mr = vmaMapMemory(target->allocator, staging.allocation, &mapped);

    unsigned char* const pImageData = (unsigned char*)mapped;
    switch (format)
    {
    case VK_FORMAT_R8G8B8A8_UNORM: memcpy(pImageData, rgba, imageSize); break;
    case VK_FORMAT_B8G8R8_UNORM:
    {
        for (uint32_t i = 0; i < width * height; ++i)
        {
            pImageData[i * 4 + 0] = rgba[i * 3 + 0];
            pImageData[i * 4 + 1] = rgba[i * 3 + 1];
            pImageData[i * 4 + 2] = rgba[i * 3 + 2];
            pImageData[i * 4 + 3] = 0xff;
        }
        format = VK_FORMAT_B8G8R8A8_UNORM;
        break;
    }
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
    case VK_FORMAT_B8G8R8A8_SRGB:
    {
        for (uint32_t i = 0; i < width * height; ++i)
        {
            pImageData[i * 4 + 0] = rgba[i * 4 + 0];
            pImageData[i * 4 + 1] = rgba[i * 4 + 1];
            pImageData[i * 4 + 2] = rgba[i * 4 + 2];
            pImageData[i * 4 + 3] = 0xff;
        }
        format = VK_FORMAT_B8G8R8A8_UNORM;
        break;
    }
    }
    vmaUnmapMemory(target->allocator, staging.allocation);
    texture.format = format;

    VkImageCreateInfo imageInfo = { VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent.width = width;
    imageInfo.extent.height = height;
    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = mips;
    imageInfo.arrayLayers = 1;
    imageInfo.format = format;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.flags = 0;

    VmaAllocationCreateInfo imageAllocCreateInfo = {};
    imageAllocCreateInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;

    vmaCreateImage(target->allocator, &imageInfo, &imageAllocCreateInfo, &texture.image, &texture.allocation, nullptr);

    // Transition image layouts, copy image.

    target->BeginUploadCommands();

    //createImage(texWidth, texHeight, mipLevels, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_TILING_OPTIMAL, 
    // VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, 
    // VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, textureImage, textureImageMemory);
    VkImageMemoryBarrier imgMemBarrier = { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
    imgMemBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    imgMemBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    imgMemBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    imgMemBarrier.subresourceRange.baseMipLevel = 0;
    imgMemBarrier.subresourceRange.levelCount = mips;
    imgMemBarrier.subresourceRange.baseArrayLayer = 0;
    imgMemBarrier.subresourceRange.layerCount = 1;
    imgMemBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imgMemBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    imgMemBarrier.image = texture.image;
    imgMemBarrier.srcAccessMask = 0;
    imgMemBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    if (false)
    {

        vkCmdPipelineBarrier(
            target->GetUploadCmd(),
            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            0,
            0, nullptr,
            0, nullptr,
            1, &imgMemBarrier);
    }

    VkBufferImageCopy region = {};
    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.layerCount = 1;
    region.imageExtent.width = width;
    region.imageExtent.height = height;
    region.imageExtent.depth = 1;

    vkCmdCopyBufferToImage(target->GetUploadCmd(), staging.buffer, texture.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

    generateMipmaps(target, texture.image, format, width, height, mips);
    
#if 0
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
#endif

    target->PushSingleFrameBuffer(staging);
    // Create ImageView

    VkImageViewCreateInfo textureImageViewInfo = { VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
    textureImageViewInfo.image = texture.image;
    textureImageViewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    textureImageViewInfo.format = format;
    textureImageViewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    textureImageViewInfo.subresourceRange.baseMipLevel = 0;
    textureImageViewInfo.subresourceRange.levelCount = mips;
    textureImageViewInfo.subresourceRange.baseArrayLayer = 0;
    textureImageViewInfo.subresourceRange.layerCount = 1;
    vkCreateImageView(target->device, &textureImageViewInfo, nullptr, &texture.imageView);

    CreateTextureSampler(target, texture, settings, mips);
}
