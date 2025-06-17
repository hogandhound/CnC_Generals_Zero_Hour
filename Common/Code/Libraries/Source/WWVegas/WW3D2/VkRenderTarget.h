#ifndef VK_RENDER_TARGET_H
#define VK_RENDER_TARGET_H

#include "vulkan/vulkan.h"
#include "vk_mem_alloc.h"
#include <vector>

static const uint32_t COMMAND_BUFFER_COUNT = 2;
struct QueueFamilyIndices {
    uint32_t graphicsFamily = 0xFFFFFFFF;
    uint32_t presentFamily = 0xFFFFFFFF;

    bool isComplete() {
        return graphicsFamily != 0xFFFFFFFF && presentFamily != 0xFFFFFFFF;
    }
};

struct SwapChainSupportDetails {
    VkSurfaceCapabilitiesKHR capabilities;
    std::vector<VkSurfaceFormatKHR> formats;
    std::vector<VkPresentModeKHR> presentModes;
};

enum VkDescFormat
{
    VkDS_T,
    VkDS_TT,
    VkDS_U,
    VkDS_TU,
    VkDS_TTU,
    VkDS_TTTU,
    VkDS_TTTTU,
    VkDS_UU,
    VkDS_TUU,
    VkDS_TTUU,
    VkDS_MaxType,
    VkDS_ = VkDS_MaxType, //Dummy Value
};
struct VkDescSetSubType
{
    std::vector<VkDescriptorSet> sets[COMMAND_BUFFER_COUNT];
    int start;
    std::vector<VkDescriptorSetLayoutBinding> defs;
};
struct VkDescSetCol
{
    std::vector<VkDescSetSubType> subs[VkDS_MaxType];
};

namespace VK
{
    enum BufferType
    {
        UNKNOWN,
        TRANSFER,
        UNIFORM,
        VERTEX,
        INDEX
    };
    struct Buffer
    {
        VkBuffer buffer;
        VmaAllocation allocation;

        BufferType type;
    };
    struct Texture
    {
        VkImage image;
        VkImageView imageView;
        VmaAllocation allocation;
        VkSampler sampler;
    };
    enum TexFlags
    {
        TexNearest = 0x01,
        TexLinear = 0x02,
        TexClamp = 0x10,
        TexRepeat = 0x20,
    };
    struct FrameBuffer
    {
        VkExtent2D extent;
        VK::Texture image;
        VK::Texture depth;
        VkFramebuffer fbo;
    };
    struct SingleFrameResources
    {
        std::vector<VK::Buffer> buffers;
        std::vector<VK::Texture> textures;
        std::vector<VK::FrameBuffer> fbos;
        uint32_t bufferIndex = 0, textureIndex = 0, fboIndex = 0;
    };
}

class VkRenderTarget;
struct MemoryPool
{
    VkRenderTarget* target;
    std::vector<std::vector<VK::Buffer>> stashed;
    VmaPool pool;
    VkBufferUsageFlags usage;
    VK::BufferType type;

    void init(VkRenderTarget* target, VK::BufferType type);
    VK::Buffer alloc(void* memory, size_t size);
    void free(VK::Buffer buffer);
    size_t PoolIndex(size_t);
};
struct MemoryPools
{
    MemoryPool transfer, vertex, index, uniform;
};

class VkRenderTarget
{
public:
    VkInstance instance = 0;
    VkDebugUtilsMessengerEXT debugMessenger;
    VkSurfaceKHR surface;

    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    VkDevice device;
    VmaAllocator allocator;
    MemoryPools vmaPools_;

    VkQueue graphicsQueue;
    VkQueue presentQueue;

    VkSwapchainKHR swapChain;
    VkFormat swapChainImageFormat;
    VkExtent2D swapChainExtent;
    std::vector<VK::FrameBuffer> swapChainFBOs;

    VkCommandPool commandPool;
    VkCommandBuffer mainCommandBuffers[COMMAND_BUFFER_COUNT];
    VkCommandBuffer fboCmd;
    VkCommandBuffer currentCmd;
    VK::Texture currentImage;
    VkCommandBuffer GetUploadCmd() { return uploadCommandBuffer[currUpload_]; }
    
    VkRenderPass renderPass;
    VkRenderPass fboPass;

    std::vector<VkSemaphore> imageAvailableSemaphores;
    std::vector<VkSemaphore> renderFinishedSemaphores;
    std::vector<VkFence> inFlightFences;
    std::vector<VkFence> imagesInFlight;
    VkDescSetCol descSets;
    VkDescriptorPool descPools[VkDS_MaxType];
    size_t currentFrame = -1;

    std::vector<VK::SingleFrameResources> singleFrame;

    void InitVulkan(void* window);

    void BeginUploadCommands();
    void PushUploads();

    void BeginFramebuffer(VkExtent2D extent, VkFormat imageFormat);
    void EndFramebuffer();

    void StartRender(VkExtent2D extent = { UINT32_MAX, UINT32_MAX });
    void RecreateSwapChain();
    void EndRender();
    uint32_t getDescSetSubIndex(VkDescFormat fmt, VkDescriptorSetLayoutBinding* bindings, int bCount);
    VkDescriptorSet getDescSet(VkDescFormat fmt, uint32_t subIndex, VkDescriptorSetLayout* layout);
    void PushSingleFrameBuffer(VK::Buffer staging);
private:
    SwapChainSupportDetails swapChainSupport_;
    uint32_t imageIndex;
    bool uploadStarted_ = false;
    int currUpload_ = 0;
    VkCommandBuffer uploadCommandBuffer[16];
    void* window_;
    
    void destroyTexture(VK::Texture& tex);
    void destroyFrameBuffer(VK::FrameBuffer& fbo);
    void cleanupSwapChain();
    
    bool checkValidationLayerSupport();
    void createInstance(void* window);
    void createSurface(void* window);
    void createMemAlloc();

    void createCommandPool();

    void createFramebuffers();


    QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device);

    void createLogicalDevice();

    VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats);

    VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes);

    VkExtent2D chooseSwapExtent(void* window, const VkSurfaceCapabilitiesKHR& capabilities);

    void createSwapChain(SwapChainSupportDetails swapChainSupport, VkExtent2D extent);

    void createSwapChain(VkExtent2D extent);

    void createSwapChain(void* window);

    bool checkDeviceExtensionSupport(VkPhysicalDevice device);

    SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice device);

    bool IsDeviceSuitable(VkPhysicalDevice device);

    void pickPhysicalDevice();

    void createImageViews();

    void createRenderPass();

    std::vector<const char*> getRequiredExtensions(void* window);

    static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageType, const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData);
    void createSyncObjects();
    void populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo);
    void setupDebugMessenger();

    VkImageView createImageView(VkImage image, VkFormat format);
    void createDescPools();
};

#endif