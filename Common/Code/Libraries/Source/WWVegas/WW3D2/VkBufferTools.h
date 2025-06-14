#ifndef VK_BUFFER_TOOLS_H
#define VK_BUFFER_TOOLS_H

#include "VkRenderTarget.h"


class VkBufferTools
{
public:
    static void CreateBuffer(VkRenderTarget* target, VkDeviceSize size, void* memory, VkBufferUsageFlags usage, VmaMemoryUsage properties, VK::Buffer& buffer);
    static void CreateVertexBuffer(VkRenderTarget* target, VkDeviceSize size, void* memory, VK::Buffer& buffer);
    static void CreateIndexBuffer(VkRenderTarget* target, VkDeviceSize size, void* memory, VK::Buffer& buffer);
    static void CreateUniformBuffer(VkRenderTarget* target, VkDeviceSize size, void* memory, VK::Buffer& buffer);
};

#endif