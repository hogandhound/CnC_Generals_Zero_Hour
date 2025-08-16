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

#ifndef vkCmdSetColorWriteMaskEXT
#define USE_DYNAMIC_COLOR
#ifdef USE_DYNAMIC_COLOR
/* Put this somewhere in a header file and include it alongside (and after) vulkan.h: */
extern PFN_vkCmdSetColorWriteMaskEXT vkCmdSetColorWriteMaskEXT_;
// This #define lets you call the function the same way as if it was coming from the vulkan.h header
#define vkCmdSetColorWriteMaskEXT vkCmdSetColorWriteMaskEXT_
#endif
#endif

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
    VkDS_TTTUU,
    VkDS_TTTTUU,
    VkDS_UUU,
    VkDS_TUUU,
    VkDS_TTUUU,
    VkDS_TTTUUU,
    VkDS_TTTTUUU,
    VkDS_UUUU,
    VkDS_TUUUU,
    VkDS_TTUUUU,
    VkDS_TTTUUUU,
    VkDS_TTTTUUUU,
    VkDS_TTUUUUU,
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

class VkRenderTarget;
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
    struct SamplerSettings
    {
        //Sampler Settings
        uint8_t minF : 1;
        uint8_t maxF : 1;
        uint8_t mipF : 1;
        uint8_t addU : 3;
        uint8_t addV : 3;
        bool operator==(SamplerSettings& other)
        {
            return minF == other.minF &&
                maxF == other.maxF &&
                mipF == other.mipF &&
                addU == other.addU &&
                addV == other.addV;
        }
        bool operator!=(SamplerSettings& other)
        {
            return minF != other.minF ||
                maxF != other.maxF ||
                mipF != other.mipF ||
                addU != other.addU ||
                addV != other.addV;
        }
    };
    struct Texture
    {
        VkImage image;
        VkImageView imageView;
        VmaAllocation allocation;
        VkSampler sampler;
        VkFormat format;
        uint16_t width, height;
        uint8_t mips;
        SamplerSettings sampSettings;
#if _DEBUG
        const char *end = 0;
        int line = 0;
#endif
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
}


class VkRenderTarget
{
public:
    VkInstance instance = 0;
    VkDebugUtilsMessengerEXT debugMessenger;
    VkSurfaceKHR surface;

    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    VkDevice device;
    VmaAllocator allocator;
    VK::MemoryPools vmaPools_;

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
#if _DEBUG
#define PushSingleTexture(A) PushSingleTexture_(A, __FILE__, __LINE__)
    void PushSingleTexture_(VK::Texture staging
        , const char* end, int line
    );
#else
    void PushSingleTexture(VK::Texture staging);
#endif 
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