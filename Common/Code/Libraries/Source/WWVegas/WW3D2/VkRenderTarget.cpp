#define VMA_IMPLEMENTATION
#include "vk_mem_alloc.h"

#include "VkRenderTarget.h"
#include "SDL.h"
#include "SDL_vulkan.h"
#include <VkTexture.h>

#include <thread>
#include <vector>
#include <set>
#include <algorithm>
#include <assert.h>

#include <iostream>
#include <fstream>

const std::vector<const char*> validationLayers = {
	"VK_LAYER_KHRONOS_validation"
};

const std::vector<const char*> deviceExtensions = {
	VK_KHR_SWAPCHAIN_EXTENSION_NAME
};
bool VK_KHR_get_memory_requirements2_enabled = false;
bool VK_KHR_get_physical_device_properties2_enabled = false;
bool VK_KHR_dedicated_allocation_enabled = false;
bool VK_KHR_bind_memory2_enabled = false;
bool VK_EXT_memory_budget_enabled = false;
bool VK_AMD_device_coherent_memory_enabled = false;
bool VK_EXT_buffer_device_address_enabled = false;
bool VK_KHR_buffer_device_address_enabled = false;
bool g_SparseBindingEnabled = false;
bool g_BufferDeviceAddressEnabled = false;


#ifdef NDEBUG
const bool enableValidationLayers = false;
#else
const bool enableValidationLayers = true;
#endif

void VkRenderTarget::InitVulkan(void* window)
{
	createInstance(window);
	setupDebugMessenger();
	createSurface(window);
	pickPhysicalDevice();
	createLogicalDevice();
	createMemAlloc();
	createSwapChain(window);
	createImageViews();
	createRenderPass();
	createFramebuffers();
	createCommandPool();
	createSyncObjects();
	createDescPools();
	currentFrame = 0;
	singleFrame.resize(COMMAND_BUFFER_COUNT);
	window_ = window;
}

void VkRenderTarget::BeginUploadCommands()
{
	if (!uploadStarted_)
	{
		VkCommandBufferBeginInfo cmdBufBeginInfo = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
		cmdBufBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
		vkBeginCommandBuffer(uploadCommandBuffer[currUpload_], &cmdBufBeginInfo);
		uploadStarted_ = true;
	}
}

void VkRenderTarget::BeginFramebuffer(VkExtent2D extent, VkFormat imageFormat)
{
	VK::SingleFrameResources& sfr = singleFrame[currentFrame];
	if (sfr.fboIndex >= sfr.fbos.size())
	{
		sfr.fbos.resize((sfr.fboIndex + 1) * 2);
	}
	VK::FrameBuffer& fbo = sfr.fbos[sfr.fboIndex++];

	VK::CreateTexture(this, fbo.image, extent.width, extent.height, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, 0, imageFormat);
	fbo.image.imageView = createImageView(fbo.image.image, imageFormat);
	VK::CreateTexture(this, fbo.depth, extent.width, extent.height, (uint32_t)0, 0, VK_FORMAT_D32_SFLOAT);
	fbo.depth.imageView = createImageView(fbo.depth.image, VK_FORMAT_D32_SFLOAT);
	
	VkImageView attachments[] = {
		fbo.image.imageView, fbo.depth.imageView
	};

	VkFramebufferCreateInfo framebufferInfo = {};
	framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
	framebufferInfo.renderPass = fboPass;
	framebufferInfo.attachmentCount = 2;
	framebufferInfo.pAttachments = attachments;
	framebufferInfo.width = extent.width;
	framebufferInfo.height = extent.height;
	framebufferInfo.layers = 1;

	if (vkCreateFramebuffer(device, &framebufferInfo, nullptr, &fbo.fbo) != VK_SUCCESS) {
	}


	VkCommandBufferBeginInfo beginInfo = {};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

	if (vkBeginCommandBuffer(fboCmd, &beginInfo) != VK_SUCCESS) {
		throw std::runtime_error("failed to begin recording command buffer!");
	}

	VkRenderPassBeginInfo renderPassInfo = {};
	renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	renderPassInfo.renderPass = fboPass;
	renderPassInfo.framebuffer = fbo.fbo;
	renderPassInfo.renderArea.offset = { 0, 0 };
	renderPassInfo.renderArea.extent = extent;

	VkClearValue clearColor[2];
	clearColor[0] = { 0.0f, 0.0f, 0.0f, 0.0f };
	clearColor[1].depthStencil = { 1.0f, 0 };
	renderPassInfo.clearValueCount = 2;
	renderPassInfo.pClearValues = clearColor;

	VkViewport viewport = {};
	viewport.x = 0.0f;
	viewport.y = 0.0f;
	viewport.width = (float)extent.width;
	viewport.height = (float)extent.height;
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;
	vkCmdSetViewport(fboCmd, 0, 1, &viewport);
	VkRect2D rect = {
		{ 0, 0 },
		extent
	};
	vkCmdSetScissor(
		fboCmd,
		0,
		1,
		&rect);

	vkCmdBeginRenderPass(fboCmd, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
	currentCmd = fboCmd;
	currentImage = fbo.image;
}

void VkRenderTarget::EndFramebuffer()
{
	PushUploads();

	vkCmdEndRenderPass(fboCmd);

	//VkImageMemoryBarrier imgMemBarrier = { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
	//imgMemBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	//imgMemBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	//imgMemBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	//imgMemBarrier.subresourceRange.baseMipLevel = 0;
	//imgMemBarrier.subresourceRange.levelCount = 1;
	//imgMemBarrier.subresourceRange.baseArrayLayer = 0;
	//imgMemBarrier.subresourceRange.layerCount = 1;
	//imgMemBarrier.image = currentImage.image;

	//imgMemBarrier.oldLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
	//imgMemBarrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	//imgMemBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
	//imgMemBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

	//vkCmdPipelineBarrier(
	//	fboCmd,
	//	VK_PIPELINE_STAGE_TRANSFER_BIT,
	//	VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
	//	0,
	//	0, nullptr,
	//	0, nullptr,
	//	1, &imgMemBarrier);

	if (vkEndCommandBuffer(fboCmd) != VK_SUCCESS) {
		throw std::runtime_error("failed to record command buffer!");
	}

	VkSubmitInfo submitInfo = {};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &fboCmd;

	if (vkQueueSubmit(graphicsQueue, 1, &submitInfo, nullptr) != VK_SUCCESS) {
		throw std::runtime_error("failed to submit draw command buffer!");
	}
	//vkQueueWaitIdle(graphicsQueue);

	currentCmd = mainCommandBuffers[currentFrame];
	currentImage = swapChainFBOs[currentFrame].image;
}

void VkRenderTarget::StartRender(VkExtent2D extent)
{
	vkWaitForFences(device, 1, &inFlightFences[currentFrame], VK_TRUE, UINT64_MAX);

	uint32_t lastImage = imageIndex;
	VkResult result = VK_SUCCESS;
	do
	{
		result = vkAcquireNextImageKHR(device, swapChain, 1000 * 1000, imageAvailableSemaphores[currentFrame], VK_NULL_HANDLE, &imageIndex);
	} while (result != VK_SUCCESS);
	
	if (extent.width != UINT32_MAX && extent.height != UINT32_MAX)
		swapChainExtent = extent;
	if (result == VK_ERROR_OUT_OF_DATE_KHR || (extent.width != UINT32_MAX && extent.height != UINT32_MAX)) {
		RecreateSwapChain();
		return;
	}
	else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
		throw std::runtime_error("failed to acquire swap chain image!");
	}

	if (imagesInFlight[imageIndex] != VK_NULL_HANDLE) {
		vkWaitForFences(device, 1, &imagesInFlight[imageIndex], VK_TRUE, UINT64_MAX);
	}
	imagesInFlight[imageIndex] = inFlightFences[currentFrame];

	vkResetFences(device, 1, &inFlightFences[currentFrame]);
	vkResetCommandBuffer(mainCommandBuffers[currentFrame], /*VkCommandBufferResetFlagBits*/ 0);

	VkCommandBufferBeginInfo beginInfo = {};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

	if (vkBeginCommandBuffer(mainCommandBuffers[currentFrame], &beginInfo) != VK_SUCCESS) {
		throw std::runtime_error("failed to begin recording command buffer!");
	}
	VkRenderPassBeginInfo renderPassInfo = {};
	renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	renderPassInfo.renderPass = renderPass;
	renderPassInfo.framebuffer = swapChainFBOs[imageIndex].fbo;
	renderPassInfo.renderArea.offset = { 0, 0 };
	renderPassInfo.renderArea.extent = swapChainExtent;
	VkViewport viewport = {};
	viewport.x = 0.0f;
	viewport.y = 0.0f;
	viewport.width = (float)swapChainExtent.width;
	viewport.height = (float)swapChainExtent.height;
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;
	vkCmdSetViewport(mainCommandBuffers[currentFrame], 0, 1, &viewport);
	VkRect2D rect = {
		{ 0, 0 },
		swapChainExtent
	};
	vkCmdSetScissor(
		mainCommandBuffers[currentFrame],
		0,
		1,
		&rect);

	VkClearValue clearColor[2];
	clearColor[0] = { 0.0f, 1.0f, 0.0f, 1.0f };
	clearColor[1].depthStencil = { 1.0f, 0 };
	renderPassInfo.clearValueCount = 2;
	renderPassInfo.pClearValues = clearColor;

	vkCmdBeginRenderPass(mainCommandBuffers[currentFrame], &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
	currentCmd = mainCommandBuffers[currentFrame];
	currentImage = swapChainFBOs[currentFrame].image;

	//memset(descSets[currentFrame].start, 0, VkDS_MaxType * sizeof(int));
	for (int i = 0; i < VkDS_MaxType; ++i)
	{
		for (auto& set : descSets.subs[i])
		{
			set.start = 0;
		}
	}
	auto& sf = singleFrame[currentFrame];
	for (int i = 0; i < sf.bufferIndex; ++i)
	{
		switch (sf.buffers[i].type)
		{
		case VK::TRANSFER:
			vmaPools_.transfer.free(sf.buffers[i]);
			break;
		case VK::VERTEX:
			vmaPools_.vertex.free(sf.buffers[i]);
			break;
		case VK::INDEX:
			vmaPools_.index.free(sf.buffers[i]);
			break;
		case VK::UNIFORM:
			vmaPools_.uniform.free(sf.buffers[i]);
			break;
		}
		//vmaDestroyBuffer(allocator, sf.buffers[i].buffer, sf.buffers[i].allocation);
	}
	singleFrame[currentFrame].bufferIndex = 0;
	for (int i = 0; i < sf.fboIndex; ++i)
	{
		vkDestroyImageView(device, sf.fbos[i].depth.imageView, 0);
		vkDestroySampler(device, sf.fbos[i].depth.sampler, 0);
		vmaDestroyImage(allocator, sf.fbos[i].depth.image, sf.fbos[i].depth.allocation);
		if (sf.fbos[i].image.image)
		{
			vkDestroyImageView(device, sf.fbos[i].image.imageView, 0);
			vkDestroySampler(device, sf.fbos[i].image.sampler, 0);
			vmaDestroyImage(allocator, sf.fbos[i].image.image, sf.fbos[i].image.allocation);
		}
		vkDestroyFramebuffer(device, sf.fbos[i].fbo, 0);
	}
	singleFrame[currentFrame].fboIndex = 0;
	for (int i = 0; i < sf.textureIndex; ++i)
	{
		if (sf.textures[i].image)
		{
			vkDestroyImageView(device, sf.textures[i].imageView, 0);
			vkDestroySampler(device, sf.textures[i].sampler, 0);
			vmaDestroyImage(allocator, sf.textures[i].image, sf.textures[i].allocation);
		}
	}
	singleFrame[currentFrame].textureIndex = 0;
}

void VkRenderTarget::EndRender()
{
	PushUploads();

	auto start = std::chrono::high_resolution_clock::now();

	vkCmdEndRenderPass(mainCommandBuffers[currentFrame]);

	if (vkEndCommandBuffer(mainCommandBuffers[currentFrame]) != VK_SUCCESS) {
		throw std::runtime_error("failed to record command buffer!");
	}
	VkSubmitInfo submitInfo = {};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

	VkSemaphore waitSemaphores[] = { imageAvailableSemaphores[currentFrame] };
	VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
	submitInfo.waitSemaphoreCount = 1;
	submitInfo.pWaitSemaphores = waitSemaphores;
	submitInfo.pWaitDstStageMask = waitStages;

	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &mainCommandBuffers[currentFrame];

	VkSemaphore signalSemaphores[] = { renderFinishedSemaphores[currentFrame] };
	submitInfo.signalSemaphoreCount = 1;
	submitInfo.pSignalSemaphores = signalSemaphores;

	if (vkQueueSubmit(graphicsQueue, 1, &submitInfo, inFlightFences[currentFrame]) != VK_SUCCESS) {
		throw std::runtime_error("failed to submit draw command buffer!");
	}

	VkPresentInfoKHR presentInfo = {};
	presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

	presentInfo.waitSemaphoreCount = 1;
	presentInfo.pWaitSemaphores = signalSemaphores;

	VkSwapchainKHR swapChains[] = { swapChain };
	presentInfo.swapchainCount = 1;
	presentInfo.pSwapchains = swapChains;

	presentInfo.pImageIndices = &imageIndex;

	VkResult result = vkQueuePresentKHR(presentQueue, &presentInfo);
	//vkQueueWaitIdle(graphicsQueue);
	//vkDeviceWaitIdle(device);

	if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR
		//|| framebufferResized
		) {
		//framebufferResized = false;
		RecreateSwapChain();
	}
	else if (result != VK_SUCCESS) {
		throw std::runtime_error("failed to present swap chain image!");
	}

	currentFrame = (currentFrame + 1) % COMMAND_BUFFER_COUNT;
	currentCmd = 0;
}

VkDescriptorSet VkRenderTarget::getDescSet(VkDescFormat fmt, uint32_t subIndex, VkDescriptorSetLayout* layout)
{
	if (fmt < 0 || fmt >= VkDS_MaxType)
		return VkDescriptorSet();

	auto& sub = descSets.subs[fmt][subIndex];
	if (sub.start >= sub.sets[currentFrame].size())
	{
		sub.sets[currentFrame].resize(sub.start + 128);
	}
	if (sub.sets[currentFrame][sub.start] == 0)
	{
		VkDescriptorSetAllocateInfo allocInfo = {};
		allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		allocInfo.descriptorPool = descPools[fmt];
		allocInfo.descriptorSetCount = 1;
		allocInfo.pSetLayouts = layout;
		
		if (vkAllocateDescriptorSets(device, &allocInfo, &sub.sets[currentFrame][sub.start]) != VK_SUCCESS) {
			throw std::runtime_error("failed to allocate descriptor sets!");
		}
	}
	return sub.sets[currentFrame][sub.start++];
}

void VkRenderTarget::PushSingleFrameBuffer(VK::Buffer staging)
{
	if (singleFrame[currentFrame].buffers.size() <= singleFrame[currentFrame].bufferIndex)
		singleFrame[currentFrame].buffers.resize(singleFrame[currentFrame].bufferIndex + 1);
	singleFrame[currentFrame].buffers[singleFrame[currentFrame].bufferIndex] = staging;
	singleFrame[currentFrame].bufferIndex++;
}

uint32_t VkRenderTarget::getDescSetSubIndex(VkDescFormat fmt, VkDescriptorSetLayoutBinding* bindings, int bCount)
{
	if (fmt < 0 || fmt >= VkDS_MaxType)
		return -1;
	auto& subs = descSets.subs[fmt];
	for (int i = 0; i < subs.size(); ++i)
	{
		if (subs[i].defs.size() == bCount)
		{
			bool matched = true;
			for (int d = 0; d < bCount; ++d)
			{
				if (subs[i].defs[d].descriptorType != bindings[d].descriptorType ||
					subs[i].defs[d].binding != bindings[d].binding ||
					subs[i].defs[d].descriptorCount != bindings[d].descriptorCount ||
					subs[i].defs[d].pImmutableSamplers != bindings[d].pImmutableSamplers ||
					subs[i].defs[d].stageFlags != bindings[d].stageFlags)
				{
					matched = false;
				}
			}
			if (matched)
				return i;
		}
	}
	VkDescSetSubType toadd = {};
	toadd.defs.resize(bCount);
	memcpy(toadd.defs.data(), bindings, bCount * sizeof(VkDescriptorSetLayoutBinding));
	subs.push_back(toadd);
	return (uint32_t)subs.size() - 1;
}

void VkRenderTarget::destroyFrameBuffer(VK::FrameBuffer& fbo)
{
	destroyTexture(fbo.depth);
	destroyTexture(fbo.image);
	vkDestroyFramebuffer(device, fbo.fbo, 0);
}

void VkRenderTarget::destroyTexture(VK::Texture& tex)
{
	vmaDestroyImage(allocator, tex.image, tex.allocation);
	vkDestroyImageView(device, tex.imageView, 0);
	vkDestroySampler(device, tex.sampler, 0);
}


void VkRenderTarget::cleanupSwapChain() {

	vkDestroySwapchainKHR(device, swapChain, nullptr);
	for (auto sc : swapChainFBOs)
	{
		sc.image.image = 0;
		destroyFrameBuffer(sc);
	}
	swapChainFBOs.clear();
	swapChain = 0;
}

void VkRenderTarget::RecreateSwapChain() {
	vkDeviceWaitIdle(device);
	cleanupSwapChain();
	createSwapChain(window_);
	createImageViews();
	createRenderPass();
	createFramebuffers();
}

void VkRenderTarget::PushUploads()
{
	if (uploadStarted_)
	{
		VkMemoryBarrier2KHR memoryBarrier = { VK_STRUCTURE_TYPE_MEMORY_BARRIER_2 };
		memoryBarrier.srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT_KHR;
		memoryBarrier.srcAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT_KHR;
		memoryBarrier.dstStageMask = VK_PIPELINE_STAGE_2_VERTEX_ATTRIBUTE_INPUT_BIT_KHR;
		memoryBarrier.dstAccessMask = VK_ACCESS_2_MEMORY_READ_BIT_KHR;

		//Debug wait all
		//memoryBarrier.srcStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT_KHR;
		//memoryBarrier.srcAccessMask = VK_ACCESS_2_MEMORY_READ_BIT_KHR |
		//	VK_ACCESS_2_MEMORY_WRITE_BIT_KHR;
		//memoryBarrier.dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT_KHR;
		//memoryBarrier.dstAccessMask = VK_ACCESS_2_MEMORY_READ_BIT_KHR |
		//	VK_ACCESS_2_MEMORY_WRITE_BIT_KHR;

		VkDependencyInfoKHR dependencyInfo = { VK_STRUCTURE_TYPE_DEPENDENCY_INFO_KHR };
		dependencyInfo.memoryBarrierCount = 1;
		dependencyInfo.pMemoryBarriers = &memoryBarrier;

		vkCmdPipelineBarrier2(uploadCommandBuffer[currUpload_], &dependencyInfo);

		vkEndCommandBuffer(uploadCommandBuffer[currUpload_]);

		VkCommandBufferSubmitInfo cmdSubmit = {};

		cmdSubmit.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;
		cmdSubmit.pNext = nullptr;
		cmdSubmit.commandBuffer = GetUploadCmd();
		cmdSubmit.deviceMask = 0;

		VkSubmitInfo2 submitInfo = { VK_STRUCTURE_TYPE_SUBMIT_INFO_2 };
		submitInfo.commandBufferInfoCount = 1;
		submitInfo.pCommandBufferInfos = &cmdSubmit;

		vkQueueSubmit2(graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
		//vkQueueWaitIdle(graphicsQueue);
		uploadStarted_ = false;
		currUpload_ = (currUpload_ + 1) % 0xf;
	}
}

bool VkRenderTarget::checkValidationLayerSupport() {
	uint32_t layerCount;
	vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

	std::vector<VkLayerProperties> availableLayers(layerCount);
	vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

	for (const char* layerName : validationLayers) {
		bool layerFound = false;

		for (const auto& layerProperties : availableLayers) {
			if (strcmp(layerName, layerProperties.layerName) == 0) {
				layerFound = true;
				break;
			}
		}

		if (!layerFound) {
			return false;
		}
	}

	return true;
}

VkBool32 VkRenderTarget::debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageType, const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData) {
	if (strstr(pCallbackData->pMessage, "GalaxyOverlayVkLayer"))
		return VK_TRUE;
	std::cerr << "validation layer: " << pCallbackData->pMessage << std::endl;

	if (messageSeverity == VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT || messageSeverity == VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT)
		return VK_TRUE;
	return VK_FALSE;
}

void VkRenderTarget::createSyncObjects() {
	imageAvailableSemaphores.resize(COMMAND_BUFFER_COUNT);
	renderFinishedSemaphores.resize(COMMAND_BUFFER_COUNT);
	inFlightFences.resize(COMMAND_BUFFER_COUNT);
	imagesInFlight.resize(swapChainFBOs.size(), VK_NULL_HANDLE);

	VkSemaphoreCreateInfo semaphoreInfo = {};
	semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

	VkFenceCreateInfo fenceInfo = {};
	fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

	for (size_t i = 0; i < COMMAND_BUFFER_COUNT; i++) {
		if (vkCreateSemaphore(device, &semaphoreInfo, nullptr, &imageAvailableSemaphores[i]) != VK_SUCCESS ||
			vkCreateSemaphore(device, &semaphoreInfo, nullptr, &renderFinishedSemaphores[i]) != VK_SUCCESS ||
			vkCreateFence(device, &fenceInfo, nullptr, &inFlightFences[i]) != VK_SUCCESS) {
			throw std::runtime_error("failed to create synchronization objects for a frame!");
		}
	}
}

void VkRenderTarget::populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo) {
	createInfo = {};
	createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
	createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
	createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
	createInfo.pfnUserCallback = debugCallback;
}
VkResult CreateDebugUtilsMessengerEXT(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pDebugMessenger) {
	auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
	if (func != nullptr) {
		return func(instance, pCreateInfo, pAllocator, pDebugMessenger);
	}
	else {
		return VK_ERROR_EXTENSION_NOT_PRESENT;
	}
}

void VkRenderTarget::createInstance(void* window) {
	if (enableValidationLayers && !checkValidationLayerSupport()) {
		//
		assert(0);
	}

	VkApplicationInfo appInfo = {};
	appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	appInfo.pApplicationName = "VulkanEngine";
	appInfo.applicationVersion = VK_MAKE_VERSION(1, 3, 0);
	appInfo.pEngineName = "No Engine";
	appInfo.engineVersion = VK_MAKE_VERSION(1, 3, 0);
	appInfo.apiVersion = VK_API_VERSION_1_3;

	VkInstanceCreateInfo createInfo = {};
	createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	createInfo.pApplicationInfo = &appInfo;

	auto extensions = getRequiredExtensions(window);
	createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
	createInfo.ppEnabledExtensionNames = extensions.data();

	VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo;
	if (enableValidationLayers) {
		createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
		createInfo.ppEnabledLayerNames = validationLayers.data();

		populateDebugMessengerCreateInfo(debugCreateInfo);
		createInfo.pNext = (VkDebugUtilsMessengerCreateInfoEXT*)&debugCreateInfo;
	}
	else {
		createInfo.enabledLayerCount = 0;

		createInfo.pNext = nullptr;
	}

	if (vkCreateInstance(&createInfo, nullptr, &instance) != VK_SUCCESS) {
		assert(0);
	}
}

void VkRenderTarget::createSurface(void* window)
{

	// now that you have a window and a vulkan instance you need a surface
	if (!SDL_Vulkan_CreateSurface((SDL_Window*)window, instance, &surface)) {
		// failed to create a surface!
	}
}

static constexpr uint32_t GetVulkanApiVersion()
{
#if VMA_VULKAN_VERSION == 1003000
	return VK_API_VERSION_1_3;
#elif VMA_VULKAN_VERSION == 1002000
	return VK_API_VERSION_1_2;
#elif VMA_VULKAN_VERSION == 1001000
	return VK_API_VERSION_1_1;
#elif VMA_VULKAN_VERSION == 1000000
	return VK_API_VERSION_1_0;
#else
#error Invalid VMA_VULKAN_VERSION.
	return UINT32_MAX;
#endif
}

void VkRenderTarget::createMemAlloc()
{
	VmaAllocatorCreateInfo allocatorInfo = {};
	allocatorInfo.physicalDevice = physicalDevice;
	allocatorInfo.device = device;
	allocatorInfo.instance = instance;

	allocatorInfo.vulkanApiVersion = GetVulkanApiVersion();

	if (VK_KHR_dedicated_allocation_enabled)
	{
		allocatorInfo.flags |= VMA_ALLOCATOR_CREATE_KHR_DEDICATED_ALLOCATION_BIT;
	}
	if (VK_KHR_bind_memory2_enabled)
	{
		allocatorInfo.flags |= VMA_ALLOCATOR_CREATE_KHR_BIND_MEMORY2_BIT;
	}
#if !defined(VMA_MEMORY_BUDGET) || VMA_MEMORY_BUDGET == 1
	if (VK_EXT_memory_budget_enabled && (
		GetVulkanApiVersion() >= VK_API_VERSION_1_1 || VK_KHR_get_physical_device_properties2_enabled))
	{
		allocatorInfo.flags |= VMA_ALLOCATOR_CREATE_EXT_MEMORY_BUDGET_BIT;
	}
#endif
	if (VK_AMD_device_coherent_memory_enabled)
	{
		allocatorInfo.flags |= VMA_ALLOCATOR_CREATE_AMD_DEVICE_COHERENT_MEMORY_BIT;
	}
	if (g_BufferDeviceAddressEnabled)
	{
		allocatorInfo.flags |= VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;
	}

	VkResult res = vmaCreateAllocator(&allocatorInfo, &allocator);

	vmaPools_.index.init(this, VK::INDEX);
	vmaPools_.vertex.init(this, VK::VERTEX);
	vmaPools_.uniform.init(this, VK::UNIFORM);
	vmaPools_.transfer.init(this, VK::TRANSFER);
}

void VkRenderTarget::createCommandPool()
{

	QueueFamilyIndices indices = findQueueFamilies(physicalDevice);

	VkCommandPoolCreateInfo commandPoolInfo = { VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO };
	commandPoolInfo.queueFamilyIndex = indices.graphicsFamily;
	commandPoolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
	vkCreateCommandPool(device, &commandPoolInfo, 0, &commandPool);

	{
		VkCommandBufferAllocateInfo commandBufferInfo = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
		commandBufferInfo.commandPool = commandPool;
		commandBufferInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		commandBufferInfo.commandBufferCount = COMMAND_BUFFER_COUNT;
		vkAllocateCommandBuffers(device, &commandBufferInfo, mainCommandBuffers);
	}
	{
		VkCommandBufferAllocateInfo commandBufferInfo = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
		commandBufferInfo.commandPool = commandPool;
		commandBufferInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		commandBufferInfo.commandBufferCount = 16;
		vkAllocateCommandBuffers(device, &commandBufferInfo, uploadCommandBuffer);
	}
	{
		VkCommandBufferAllocateInfo commandBufferInfo = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
		commandBufferInfo.commandPool = commandPool;
		commandBufferInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		commandBufferInfo.commandBufferCount = 1;
		vkAllocateCommandBuffers(device, &commandBufferInfo, &fboCmd);
	}
}

void VkRenderTarget::createLogicalDevice() {
	QueueFamilyIndices indices = findQueueFamilies(physicalDevice);

	std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
	std::set<uint32_t> uniqueQueueFamilies = { indices.graphicsFamily, indices.presentFamily };

	float queuePriority = 1.0f;
	for (uint32_t queueFamily : uniqueQueueFamilies) {
		VkDeviceQueueCreateInfo queueCreateInfo = {};
		queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		queueCreateInfo.queueFamilyIndex = queueFamily;
		queueCreateInfo.queueCount = 1;
		queueCreateInfo.pQueuePriorities = &queuePriority;
		queueCreateInfos.push_back(queueCreateInfo);
	}

	VkPhysicalDeviceFeatures deviceFeatures = {};

	VkPhysicalDeviceSynchronization2Features syncFeatures = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SYNCHRONIZATION_2_FEATURES };
	syncFeatures.synchronization2 = 1;

	VkDeviceCreateInfo createInfo = {};
	createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	createInfo.pNext = &syncFeatures;

	createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
	createInfo.pQueueCreateInfos = queueCreateInfos.data();

	createInfo.pEnabledFeatures = &deviceFeatures;

	createInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
	createInfo.ppEnabledExtensionNames = deviceExtensions.data();

	if (enableValidationLayers) {
		createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
		createInfo.ppEnabledLayerNames = validationLayers.data();
	}
	else {
		createInfo.enabledLayerCount = 0;
	}

	if (vkCreateDevice(physicalDevice, &createInfo, nullptr, &device) != VK_SUCCESS) {
		throw std::runtime_error("failed to create logical device!");
	}

	vkGetDeviceQueue(device, indices.graphicsFamily, 0, &graphicsQueue);
	vkGetDeviceQueue(device, indices.presentFamily, 0, &presentQueue);
}



VkSurfaceFormatKHR VkRenderTarget::chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats) {
	for (const auto& availableFormat : availableFormats) {
		if (availableFormat.format == VK_FORMAT_R8G8B8A8_SRGB && availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
			return availableFormat;
		}
	}

	return availableFormats[0];
}

VkPresentModeKHR VkRenderTarget::chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes) {
	for (const auto& availablePresentMode : availablePresentModes) {
		if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR) {
			return availablePresentMode;
		}
	}

	return VK_PRESENT_MODE_FIFO_KHR;
}

VkExtent2D VkRenderTarget::chooseSwapExtent(void* window, const VkSurfaceCapabilitiesKHR& capabilities) {
	if (capabilities.currentExtent.width != UINT32_MAX) {
		return capabilities.currentExtent;
	}
	else {
		int width, height;
		SDL_GetWindowSize((SDL_Window*)window, &width, &height);

		VkExtent2D actualExtent = {
			static_cast<uint32_t>(width),
			static_cast<uint32_t>(height)
		};

		actualExtent.width = std::max(capabilities.minImageExtent.width, std::min(capabilities.maxImageExtent.width, actualExtent.width));
		actualExtent.height = std::max(capabilities.minImageExtent.height, std::min(capabilities.maxImageExtent.height, actualExtent.height));

		return actualExtent;
	}
}

void VkRenderTarget::createSwapChain(SwapChainSupportDetails swapChainSupport, VkExtent2D extent)
{
	VkSurfaceFormatKHR surfaceFormat = chooseSwapSurfaceFormat(swapChainSupport.formats);
	VkPresentModeKHR presentMode = chooseSwapPresentMode(swapChainSupport.presentModes);

	uint32_t imageCount = swapChainSupport.capabilities.minImageCount + 3;
	if (swapChainSupport.capabilities.maxImageCount > 0 && imageCount > swapChainSupport.capabilities.maxImageCount) {
		imageCount = swapChainSupport.capabilities.maxImageCount;
	}

	VkSwapchainCreateInfoKHR createInfo = {};
	createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	createInfo.surface = surface;

	createInfo.minImageCount = imageCount;
	createInfo.imageFormat = surfaceFormat.format;
	createInfo.imageColorSpace = surfaceFormat.colorSpace;
	createInfo.imageExtent = extent;
	createInfo.imageArrayLayers = 1;
	createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

	QueueFamilyIndices indices = findQueueFamilies(physicalDevice);
	uint32_t queueFamilyIndices[] = { indices.graphicsFamily, indices.presentFamily };

	if (indices.graphicsFamily != indices.presentFamily) {
		createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
		createInfo.queueFamilyIndexCount = 2;
		createInfo.pQueueFamilyIndices = queueFamilyIndices;
	}
	else {
		createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
	}

	createInfo.preTransform = swapChainSupport.capabilities.currentTransform;
	createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	createInfo.presentMode = presentMode;
	createInfo.clipped = VK_TRUE;

	auto result = vkCreateSwapchainKHR(device, &createInfo, nullptr, &swapChain);
	if (result != VK_SUCCESS) {
		throw std::runtime_error("failed to create swap chain!");
	}

	vkGetSwapchainImagesKHR(device, swapChain, &imageCount, nullptr);
	swapChainFBOs.resize(imageCount);
	std::vector<VkImage> images;
	images.resize(imageCount);
	vkGetSwapchainImagesKHR(device, swapChain, &imageCount, images.data());
	for (int i = 0; i < imageCount; ++i)
	{
		swapChainFBOs[i].image.image = images[i];
		VK::CreateTexture(this, swapChainFBOs[i].depth, extent.width, extent.height, (uint32_t)0, 0, VK_FORMAT_D32_SFLOAT);
	}

	swapChainImageFormat = surfaceFormat.format;
	swapChainExtent = extent;
}

void VkRenderTarget::createSwapChain(VkExtent2D extent)
{
	createSwapChain(swapChainSupport_, extent);
}

void VkRenderTarget::createSwapChain(void* window) {
	swapChainSupport_ = querySwapChainSupport(physicalDevice);
	VkExtent2D extent = chooseSwapExtent(window, swapChainSupport_.capabilities);
	createSwapChain(swapChainSupport_, extent);
}


void VkRenderTarget::createFramebuffers() {
	for (size_t i = 0; i < swapChainFBOs.size(); i++) {
		VkImageView attachments[] = {
			swapChainFBOs[i].image.imageView,
			swapChainFBOs[i].depth.imageView
		};

		VkFramebufferCreateInfo framebufferInfo = {};
		framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		framebufferInfo.renderPass = renderPass;
		framebufferInfo.attachmentCount = 2;
		framebufferInfo.pAttachments = attachments;
		framebufferInfo.width = swapChainExtent.width;
		framebufferInfo.height = swapChainExtent.height;
		framebufferInfo.layers = 1;

		if (vkCreateFramebuffer(device, &framebufferInfo, nullptr, &swapChainFBOs[i].fbo) != VK_SUCCESS) {
		}
	}
}

QueueFamilyIndices VkRenderTarget::findQueueFamilies(VkPhysicalDevice device) {
	QueueFamilyIndices indices;

	uint32_t queueFamilyCount = 0;
	vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

	std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
	vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

	int i = 0;
	for (const auto& queueFamily : queueFamilies) {
		if (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
			indices.graphicsFamily = i;
		}

		VkBool32 presentSupport = false;
		vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &presentSupport);

		if (presentSupport) {
			indices.presentFamily = i;
		}

		if (indices.isComplete()) {
			break;
		}

		i++;
	}

	return indices;
}

bool VkRenderTarget::checkDeviceExtensionSupport(VkPhysicalDevice device) {
	uint32_t extensionCount;
	vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);

	std::vector<VkExtensionProperties> availableExtensions(extensionCount);
	vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, availableExtensions.data());

	std::set<std::string> requiredExtensions(deviceExtensions.begin(), deviceExtensions.end());

	for (uint32_t i = 0; i < availableExtensions.size(); ++i)
	{
		if (strcmp(availableExtensions[i].extensionName, VK_KHR_GET_MEMORY_REQUIREMENTS_2_EXTENSION_NAME) == 0)
		{
			if (GetVulkanApiVersion() == VK_API_VERSION_1_0)
			{
				VK_KHR_get_memory_requirements2_enabled = true;
			}
		}
		else if (strcmp(availableExtensions[i].extensionName, VK_KHR_DEDICATED_ALLOCATION_EXTENSION_NAME) == 0)
		{
			if (GetVulkanApiVersion() == VK_API_VERSION_1_0)
			{
				VK_KHR_dedicated_allocation_enabled = true;
			}
		}
		else if (strcmp(availableExtensions[i].extensionName, VK_KHR_BIND_MEMORY_2_EXTENSION_NAME) == 0)
		{
			if (GetVulkanApiVersion() == VK_API_VERSION_1_0)
			{
				VK_KHR_bind_memory2_enabled = true;
			}
		}
		else if (strcmp(availableExtensions[i].extensionName, VK_EXT_MEMORY_BUDGET_EXTENSION_NAME) == 0)
			VK_EXT_memory_budget_enabled = true;
		else if (strcmp(availableExtensions[i].extensionName, VK_AMD_DEVICE_COHERENT_MEMORY_EXTENSION_NAME) == 0)
			VK_AMD_device_coherent_memory_enabled = true;
		else if (strcmp(availableExtensions[i].extensionName, VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME) == 0)
		{
			if (GetVulkanApiVersion() < VK_API_VERSION_1_2)
			{
				VK_KHR_buffer_device_address_enabled = true;
			}
		}
		else if (strcmp(availableExtensions[i].extensionName, VK_EXT_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME) == 0)
		{
			if (GetVulkanApiVersion() < VK_API_VERSION_1_2)
			{
				VK_EXT_buffer_device_address_enabled = true;
			}
		}
	}

	for (const auto& extension : availableExtensions) {
		requiredExtensions.erase(extension.extensionName);
	}

	return requiredExtensions.empty();
}

SwapChainSupportDetails VkRenderTarget::querySwapChainSupport(VkPhysicalDevice device) {
	SwapChainSupportDetails details;

	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &details.capabilities);

	uint32_t formatCount;
	vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, nullptr);

	if (formatCount != 0) {
		details.formats.resize(formatCount);
		vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, details.formats.data());
	}

	uint32_t presentModeCount;
	vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, nullptr);

	if (presentModeCount != 0) {
		details.presentModes.resize(presentModeCount);
		vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, details.presentModes.data());
	}

	return details;
}

bool VkRenderTarget::IsDeviceSuitable(VkPhysicalDevice device) {
	QueueFamilyIndices indices = findQueueFamilies(device);

	bool extensionsSupported = checkDeviceExtensionSupport(device);

	bool swapChainAdequate = false;
	if (extensionsSupported) {
		SwapChainSupportDetails swapChainSupport = querySwapChainSupport(device);
		swapChainAdequate = !swapChainSupport.formats.empty() && !swapChainSupport.presentModes.empty();
	}

	VkPhysicalDeviceFeatures2 supportedFeatures = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2 };
	VkPhysicalDeviceVulkan13Features vk13 = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES };
	supportedFeatures.pNext = &vk13;
	vkGetPhysicalDeviceFeatures2(device, &supportedFeatures);


	return indices.isComplete() && extensionsSupported && swapChainAdequate && supportedFeatures.features.samplerAnisotropy && vk13.synchronization2;
}

void VkRenderTarget::pickPhysicalDevice() {
	uint32_t deviceCount = 0;
	vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);

	if (deviceCount == 0) {
		throw std::runtime_error("failed to find GPUs with Vulkan support!");
	}

	std::vector<VkPhysicalDevice> devices(deviceCount);
	vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());

	for (const auto& device : devices) {
		if (IsDeviceSuitable(device)) {
			physicalDevice = device;
			break;
		}
	}

	if (physicalDevice == VK_NULL_HANDLE) {
		throw std::runtime_error("failed to find a suitable GPU!");
	}
}

void VkRenderTarget::setupDebugMessenger() {
	if (!enableValidationLayers) return;

	VkDebugUtilsMessengerCreateInfoEXT createInfo;
	populateDebugMessengerCreateInfo(createInfo);

	if (CreateDebugUtilsMessengerEXT(instance, &createInfo, nullptr, &debugMessenger) != VK_SUCCESS) {
		throw std::runtime_error("failed to set up debug messenger!");
	}
}
VkImageView VkRenderTarget::createImageView(VkImage image, VkFormat format) {
	VkImageViewCreateInfo viewInfo = {};
	viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	viewInfo.image = image;
	viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
	viewInfo.format = format;
	viewInfo.subresourceRange.aspectMask = format == VK_FORMAT_D32_SFLOAT? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
	viewInfo.subresourceRange.baseMipLevel = 0;
	viewInfo.subresourceRange.levelCount = 1;
	viewInfo.subresourceRange.baseArrayLayer = 0;
	viewInfo.subresourceRange.layerCount = 1;

	VkImageView imageView;
	if (vkCreateImageView(device, &viewInfo, nullptr, &imageView) != VK_SUCCESS) {
		throw std::runtime_error("failed to create texture image view!");
	}

	return imageView;
}

void VkRenderTarget::createDescPools()
{
	for (int fmt = 0; fmt < VkDS_MaxType; ++fmt)
	{
		int texs = 0, ubos = 0;
		switch (fmt)
		{
		case VkDS_T: texs = 1; ubos = 0; break;
		case VkDS_TT: texs = 2; ubos = 0; break;
		case VkDS_U: texs = 0; ubos = 1; break;
		case VkDS_TU: texs = 1; ubos = 1; break;
		case VkDS_TTU: texs = 2; ubos = 1; break;
		case VkDS_TTTU:texs = 3; ubos = 1; break;
		case VkDS_TTTTU:texs = 4; ubos = 1; break;
		case VkDS_UU: texs = 0; ubos = 2; break;
		case VkDS_TUU: texs = 1; ubos = 2; break;
		case VkDS_TTUU: texs = 2; ubos = 2; break;
		}
		VkDescriptorPoolSize poolSizes[8] = {};
		for (int i = 0; i < texs; ++i)
		{
			poolSizes[i].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			poolSizes[i].descriptorCount = 1;
		}
		for (int i = texs; i < texs+ubos; ++i)
		{
			poolSizes[i].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
			poolSizes[i].descriptorCount = 1;
		}

		VkDescriptorPoolCreateInfo poolInfo = {};
		poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		poolInfo.poolSizeCount = texs+ubos;
		poolInfo.pPoolSizes = poolSizes;
		poolInfo.maxSets = 2048;

		if (vkCreateDescriptorPool(device, &poolInfo, nullptr, &descPools[fmt]) != VK_SUCCESS) {
			throw std::runtime_error("failed to create descriptor pool!");
		}
	}

}

void VkRenderTarget::createImageViews() {
	for (uint32_t i = 0; i < swapChainFBOs.size(); i++) {
		swapChainFBOs[i].image.imageView = createImageView(swapChainFBOs[i].image.image, swapChainImageFormat);
	}
}
void VkRenderTarget::createRenderPass() {
	VkAttachmentDescription colorAttachment = {};
	colorAttachment.format = swapChainImageFormat;
	colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
	colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

	VkAttachmentDescription depthAttachment{};
	depthAttachment.format = VK_FORMAT_D32_SFLOAT;
	depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
	depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	VkAttachmentReference depthAttachmentRef{};
	depthAttachmentRef.attachment = 1;
	depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	VkAttachmentReference colorAttachmentRef = {};
	colorAttachmentRef.attachment = 0;
	colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	VkSubpassDescription subpass = {};
	subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpass.colorAttachmentCount = 1;
	subpass.pColorAttachments = &colorAttachmentRef;
	subpass.pDepthStencilAttachment = &depthAttachmentRef;

	VkSubpassDependency dependency = {};
	dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
	dependency.dstSubpass = 0;
	dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
	dependency.srcAccessMask = 0;
	dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
	dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

	VkAttachmentDescription attachments[2] = { colorAttachment, depthAttachment };
	VkRenderPassCreateInfo renderPassInfo = {};
	renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	renderPassInfo.attachmentCount = 2;
	renderPassInfo.pAttachments = attachments;
	renderPassInfo.subpassCount = 1;
	renderPassInfo.pSubpasses = &subpass;
	renderPassInfo.dependencyCount = 1;
	renderPassInfo.pDependencies = &dependency;

#if 1
	VkSubpassDependency dependencies[2];
	dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
	dependencies[0].dstSubpass = 0;
	dependencies[0].srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
	dependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
	dependencies[0].srcAccessMask = VK_ACCESS_NONE_KHR;
	dependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
	dependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

	dependencies[1].srcSubpass = 0;
	dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
	dependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
	dependencies[1].dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
	dependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
	dependencies[1].dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
	dependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

	renderPassInfo.dependencyCount = 2;
	renderPassInfo.pDependencies = dependencies;
#endif


	if (vkCreateRenderPass(device, &renderPassInfo, nullptr, &renderPass) != VK_SUCCESS) {
		throw std::runtime_error("failed to create render pass!");
	}
	if (vkCreateRenderPass(device, &renderPassInfo, nullptr, &fboPass) != VK_SUCCESS) {
		throw std::runtime_error("failed to create render pass!");
	}
}

std::vector<const char*> VkRenderTarget::getRequiredExtensions(void* window) {

	uint32_t extensionCount;
	SDL_Vulkan_GetInstanceExtensions((SDL_Window*)window, &extensionCount, nullptr);
	std::vector<const char*> extensionNames(extensionCount);
	SDL_Vulkan_GetInstanceExtensions((SDL_Window*)window, &extensionCount, extensionNames.data());

	if (enableValidationLayers) {
		extensionNames.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
	}

	return extensionNames;
}

uint32_t MemPoolSizes[] = {
	32, 64, 128, 256, 384, 512, 768, 1024, 1536, 2048, 3072,  4096,1024 * 6, 1024 * 10, 1024 * 16, 1024 * 24, 1024 * 36,1024 * 72,1024 * 128
};
uint32_t MemPoolSizeCount = sizeof(MemPoolSizes) / sizeof(uint32_t);
void MemoryPool::init(VkRenderTarget* target, VK::BufferType type)
{
	this->target = target;
	this->type = type;
	//VkBufferUsageFlags usage, 
	VmaMemoryUsage memUsage; VmaAllocationCreateFlags memFlags;
	switch (type)
	{
	case VK::UNIFORM:
		usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
		memFlags = 0;
		memUsage = VMA_MEMORY_USAGE_GPU_ONLY;
		break;
	case VK::VERTEX:
		usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
		memFlags = 0;
		memUsage = VMA_MEMORY_USAGE_GPU_ONLY;
		break;
	case VK::INDEX:
		usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
		memFlags = 0;
		memUsage = VMA_MEMORY_USAGE_GPU_ONLY;
		break;
	case VK::TRANSFER:
		usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
		memFlags = VMA_ALLOCATION_CREATE_MAPPED_BIT;
		memUsage = VMA_MEMORY_USAGE_CPU_ONLY;
		break;
	}
	// Create a pool that can have at most 2 blocks, 128 MiB each.
	VmaPoolCreateInfo poolCreateInfo = {};
	//poolCreateInfo.blockSize = 16ULL * 1024 * 1024;
	//poolCreateInfo.maxBlockCount = 2;
	VmaAllocationCreateInfo sampleAllocCreateInfo = {};
	VkBufferCreateInfo sampleBufCreateInfo = { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
	sampleBufCreateInfo.size = 0x10000; // Doesn't matter.
	sampleBufCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	// Find memoryTypeIndex for the pool.
	sampleBufCreateInfo.usage = usage;

	sampleAllocCreateInfo.usage = memUsage;
	sampleAllocCreateInfo.flags = memFlags;

	uint32_t memTypeIndex;
	auto res = vmaFindMemoryTypeIndexForBufferInfo(target->allocator,
		&sampleBufCreateInfo, &sampleAllocCreateInfo, &memTypeIndex);
	poolCreateInfo.memoryTypeIndex = memTypeIndex;

	//VmaPool pool;
	res = vmaCreatePool(target->allocator, &poolCreateInfo, &pool);

	stashed.resize(MemPoolSizeCount + 1);
}


VK::Buffer MemoryPool::alloc(void* memory, size_t size)
{
	uint32_t index = PoolIndex(size);
	if (!stashed[index].empty())
	{
		VK::Buffer ret = {};
		if (index == MemPoolSizeCount)
		{
			for (int m = 0; m < stashed[index].size(); ++m)
			{
				auto mem = stashed[index][m];
				if (mem.allocation->GetSize() >= size && mem.allocation->GetSize() < size * 3 / 2)
				{
					ret = mem;
					stashed[index].erase(stashed[index].begin() + m);
					break;
				}
			}
		}
		else
		{
			ret = stashed[index].back();
			stashed[index].pop_back();
		}
		if (ret.buffer != nullptr)
		{
			if (usage & VK_BUFFER_USAGE_TRANSFER_SRC_BIT)
			{
				void* mapped = 0;
				VkResult mr = vmaMapMemory(target->allocator, ret.allocation, &mapped);
				memcpy(mapped, memory, size);
				vmaUnmapMemory(target->allocator, ret.allocation);
			}
			return ret;
		}
	}


	if (index < MemPoolSizeCount)
	{
		size = MemPoolSizes[index];
	}

	VkBufferCreateInfo vbInfo = { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
	vbInfo.size = size;
	vbInfo.usage = usage;
	vbInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	VmaAllocationCreateInfo vbAllocCreateInfo = {};
	vbAllocCreateInfo.pool = pool;

	VK::Buffer ret = {};
	ret.type = type;
	VmaAllocationInfo stagingBufferAllocInfo = {};
	vmaCreateBuffer(target->allocator, &vbInfo, &vbAllocCreateInfo, &ret.buffer, &ret.allocation, &stagingBufferAllocInfo);
	if (usage & VK_BUFFER_USAGE_TRANSFER_SRC_BIT)
	{
		if (stagingBufferAllocInfo.pMappedData)
			memcpy(stagingBufferAllocInfo.pMappedData, memory, size);
		else
		{
			void* mapped = 0;
			VkResult mr = vmaMapMemory(target->allocator, ret.allocation, &mapped);
			memcpy(mapped, memory, size);
			vmaUnmapMemory(target->allocator, ret.allocation);
		}
	}
	return ret;
}

void MemoryPool::free(VK::Buffer buffer)
{
	if (buffer.allocation == 0)
	{
		assert(!buffer.buffer);
		return;
	}
	uint32_t index = PoolIndex(buffer.allocation->GetSize());
	stashed[index].push_back(buffer);
}

size_t MemoryPool::PoolIndex(size_t sz)
{
	if (sz > MemPoolSizes[MemPoolSizeCount - 1])
		return MemPoolSizeCount;
	int smallest = 0, biggest = MemPoolSizeCount - 1;
	while (smallest < biggest)
	{
		int mid = (smallest + biggest) / 2;
		if (MemPoolSizes[mid] == sz)
			return mid;
		if (MemPoolSizes[mid] > sz)
			biggest = mid;
		else
			smallest = mid + 1;
	}
	return smallest;
}
