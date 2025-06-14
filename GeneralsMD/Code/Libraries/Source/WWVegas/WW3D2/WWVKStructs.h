#pragma once
#include <vulkan/vulkan.h>
#include <vector>
namespace VK
{
    struct Surface
    {
        uint16_t width, height;
        std::vector<uint8_t> buffer;
        VkFormat format;
    };
}