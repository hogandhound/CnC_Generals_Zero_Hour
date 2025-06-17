#ifndef VK_TEXTURE_H
#define VK_TEXTURE_H

#include "VkRenderTarget.h"
namespace VK
{
    void CreateTexture(VkRenderTarget* target, VK::Texture& texture, uint32_t width, uint32_t height, uint8_t* rgba, 
        uint32_t flags = (uint32_t)(VK::TexNearest|VK::TexClamp), VkFormat format = VkFormat::VK_FORMAT_R8G8B8A8_UNORM);
    void CreateTexture(VkRenderTarget* target, VK::Texture& texture, uint32_t width, uint32_t height, uint32_t usage, uint32_t flags, VkFormat format);
    void CreateTextureSampler(VkRenderTarget* target, VK::Texture& texture, uint32_t flags);
    void DestroyTexture(VkRenderTarget* target, VK::Texture& texture);
}
#endif
