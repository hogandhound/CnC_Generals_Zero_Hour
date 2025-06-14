#include "VkBufferTools.h"
#if 0
#include <unordered_map>
#include <string>
#include <Windows.h>
#endif

void VkBufferTools::CreateBuffer(VkRenderTarget* target, VkDeviceSize size, void* memory, VkBufferUsageFlags usage, VmaMemoryUsage properties, VK::Buffer& buffer)
{
    auto staging = target->vmaPools_.transfer.alloc(memory, size);

    if (usage & VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT)
        buffer = target->vmaPools_.uniform.alloc(memory, size);
    else if (usage & VK_BUFFER_USAGE_VERTEX_BUFFER_BIT)
        buffer = target->vmaPools_.vertex.alloc(memory, size);
    else if (usage & VK_BUFFER_USAGE_INDEX_BUFFER_BIT)
        buffer = target->vmaPools_.index.alloc(memory, size);
    
    // Copy buffers

    target->BeginUploadCommands();

    VkBufferCopy vbCopyRegion = {};
    vbCopyRegion.srcOffset = 0;
    vbCopyRegion.dstOffset = 0;
    vbCopyRegion.size = size;
    vkCmdCopyBuffer(target->GetUploadCmd(), staging.buffer, buffer.buffer, 1, &vbCopyRegion);

    VkBufferMemoryBarrier2KHR bufferMemoryBarrier = { VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2 };
    bufferMemoryBarrier.srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT_KHR;
    bufferMemoryBarrier.srcAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT_KHR;
    bufferMemoryBarrier.dstStageMask = VK_PIPELINE_STAGE_2_VERTEX_ATTRIBUTE_INPUT_BIT_KHR;
    bufferMemoryBarrier.dstAccessMask = VK_ACCESS_2_MEMORY_READ_BIT_KHR;
    bufferMemoryBarrier.buffer = buffer.buffer;
    bufferMemoryBarrier.size = VK_WHOLE_SIZE;

    VkDependencyInfoKHR dependencyInfo = { VK_STRUCTURE_TYPE_DEPENDENCY_INFO_KHR };
    dependencyInfo.pBufferMemoryBarriers = &bufferMemoryBarrier;
    dependencyInfo.bufferMemoryBarrierCount = 1;

    //vkCmdPipelineBarrier2(target->uploadCommandBuffer, &dependencyInfo);

    target->PushSingleFrameBuffer(staging);
}

void VkBufferTools::CreateVertexBuffer(VkRenderTarget* target, VkDeviceSize size, void* memory, VK::Buffer& buffer)
{
    VkBufferTools::CreateBuffer(target, size, memory, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VMA_MEMORY_USAGE_GPU_ONLY, buffer);
}

void VkBufferTools::CreateIndexBuffer(VkRenderTarget* target, VkDeviceSize size, void* memory, VK::Buffer& buffer)
{
    VkBufferTools::CreateBuffer(target, size, memory, VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VMA_MEMORY_USAGE_GPU_ONLY, buffer);
}

void VkBufferTools::CreateUniformBuffer(VkRenderTarget* target, VkDeviceSize size, void* memory, VK::Buffer& buffer)
{
    VkBufferTools::CreateBuffer(target, size, memory, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_GPU_ONLY, buffer);
}
