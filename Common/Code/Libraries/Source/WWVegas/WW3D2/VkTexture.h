#ifndef VK_TEXTURE_H
#define VK_TEXTURE_H

#include "VkRenderTarget.h"
#include <assert.h>
namespace VK
{
    static uint32_t SizeOfFormat(VkFormat f)
    {
        switch (f)
        {
        default:
            assert(false);
            return 4 * sizeof(uint8_t);
        case VK_FORMAT_R32G32B32A32_SFLOAT:
            return 4 * sizeof(float);
        case VK_FORMAT_D32_SFLOAT_S8_UINT:
            return 5 * sizeof(uint8_t);
        case VK_FORMAT_R8G8B8A8_UINT:
        case VK_FORMAT_R8G8B8A8_UNORM:
        case VK_FORMAT_D32_SFLOAT:
        case VK_FORMAT_D24_UNORM_S8_UINT:
        case VK_FORMAT_B8G8R8A8_UNORM:
        case VK_FORMAT_B8G8R8A8_SRGB:
            return 4 * sizeof(uint8_t);
        case VK_FORMAT_R8G8B8_UNORM:
        case VK_FORMAT_B8G8R8_UNORM:
        case VK_FORMAT_R8G8B8_UINT:
            return 3 * sizeof(uint8_t);
        case VK_FORMAT_D16_UNORM:
        case VK_FORMAT_R5G6B5_UNORM_PACK16:
        case VK_FORMAT_R5G5B5A1_UNORM_PACK16:
        case VK_FORMAT_A1R5G5B5_UNORM_PACK16:
        case VK_FORMAT_A4R4G4B4_UNORM_PACK16:
        case VK_FORMAT_R8G8_UINT:
        case VK_FORMAT_BC2_UNORM_BLOCK:
        case VK_FORMAT_BC3_UNORM_BLOCK:
            return 2 * sizeof(uint8_t);
        case VK_FORMAT_R8_UINT:
        case VK_FORMAT_R4G4_UNORM_PACK8:
        case VK_FORMAT_BC1_RGBA_UNORM_BLOCK:
            return 1 * sizeof(uint8_t);
        }
    }

    void CreateTexture(VkRenderTarget* target, VK::Texture& texture, uint32_t width, uint32_t height, uint8_t* rgba, 
        VK::SamplerSettings settings = {}, VkFormat format = VkFormat::VK_FORMAT_R8G8B8A8_UNORM);
    void CreateTexture(VkRenderTarget* target, VK::Texture& texture, uint32_t width, uint32_t height, uint32_t usage, 
        VK::SamplerSettings settings, VkFormat format);
    void CreateTextureSampler(VkRenderTarget* target, VK::Texture& texture, VK::SamplerSettings settings, uint32_t mips = 0);
    void DestroyTexture(VkRenderTarget* target, VK::Texture& texture);

    void CreateTextureMips(VkRenderTarget* target, VK::Texture& texture, uint32_t mips, uint32_t width, uint32_t height, uint8_t* rgba,
        VK::SamplerSettings settings, VkFormat format);
}
#endif
