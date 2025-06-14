#pragma once

#include <vulkan/vulkan_core.h>
struct VKPipelineData
{
    VkDescriptorSetLayout descriptorSetLayout;
    VkPipelineLayout pipelineLayout;
    VkPipeline graphicsPipeline;
    uint32_t subIndex;
};
struct VkUniform
{
    VkBuffer buffer;
};