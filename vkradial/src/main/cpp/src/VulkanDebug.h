#pragma once
#include "vulkan/vulkan.h"

#include <math.h>
#include <stdlib.h>
#include <string>
#include <cstring>
#include <fstream>
#include <assert.h>
#include <stdio.h>
#include <vector>
#ifdef _WIN32
#include <windows.h>
#include <fcntl.h>
#include <io.h>
#endif
#ifdef __ANDROID__
#include "vulkanandroid.h"
#endif
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>


// Default validation layers
// Set to true if function pointer for the debug marker are available
extern bool gbActive;

extern int giValidationLayerCount;
extern const char *gzsValidationLayerNames[];

// Default debug callback
VkBool32 HMessageCallback(
		VkDebugReportFlagsEXT flags,
		VkDebugReportObjectTypeEXT objType,
		uint64_t srcObject,
		size_t location,
		int32_t msgCode,
		const char *pLayerPrefix,
		const char *pMsg,
		void *pUserData);

// Load debug function pointers and set debug callback
// if callBack is NULL, default message callback will be used
void HSetupDebugging(
		VkInstance instance,
		VkDebugReportFlagsEXT flags,
		VkDebugReportCallbackEXT callBack);
// Clear debug callback
void HFreeDebugCallback(VkInstance instance);

// Get function pointers for the debug report extensions from the device
void DebugMarkerSetup(VkDevice device);

// Sets the debug name of an object
// All Objects in Vulkan are represented by their 64-bit handles which are passed into this function
// along with the object type
void DebugMarkerSetObjectName(VkDevice device, uint64_t object,
							  VkDebugReportObjectTypeEXT objectType, const char *name);

// Set the tag for an object
void DebugMarkerSetObjectTag(VkDevice device, uint64_t object,
							 VkDebugReportObjectTypeEXT objectType, uint64_t name,
							 size_t tagSize, const void *tag);

// Start a new debug marker region
void DebugMarkerBeginRegion(VkCommandBuffer cmdbuffer, const char *pMarkerName,
							glm::vec4 color);

// Insert a new debug marker into the command buffer
void DebugMarkerInsert(VkCommandBuffer cmdbuffer, std::string markerName, glm::vec4 color);

// End the current debug marker region
void DebugMarkerEndRegion(VkCommandBuffer cmdBuffer);

// Object specific naming functions
void DebugMarkerSetCommandBufferName(VkDevice device, VkCommandBuffer cmdBuffer,
									 const char *name);
void DebugMarkerSetQueueName(VkDevice device, VkQueue queue, const char *name);
void DebugMarkerSetImageName(VkDevice device, VkImage image, const char *name);
void DebugMarkerSetSamplerName(VkDevice device, VkSampler sampler, const char *name);
void DebugMarkerSetBufferName(VkDevice device, VkBuffer buffer, const char *name);
void DebugMarkerSetDeviceMemoryName(VkDevice device, VkDeviceMemory memory,
									const char *name);
void DebugMarkerSetShaderModuleName(VkDevice device, VkShaderModule shaderModule,
									const char *name);
void DebugMarkerSetPipelineName(VkDevice device, VkPipeline pipeline, const char *name);
void DebugMarkerSetPipelineLayoutName(VkDevice device, VkPipelineLayout pipelineLayout,
									  const char *name);
void DebugMarkerSetRenderPassName(VkDevice device, VkRenderPass renderPass,
								  const char *name);
void DebugMarkerSetFramebufferName(VkDevice device, VkFramebuffer framebuffer,
								   const char *name);
void DebugMarkerSetDescriptorSetLayoutName(VkDevice device,
										   VkDescriptorSetLayout descriptorSetLayout,
										   const char *name);
void DebugMarkerSetDescriptorSetName(VkDevice device, VkDescriptorSet descriptorSet,
									 const char *name);
void DebugMarkerSetSemaphoreName(VkDevice device, VkSemaphore semaphore, const char *name);
void DebugMarkerSetFenceName(VkDevice device, VkFence fence, const char *name);
void DebugMarkerSetEventName(VkDevice device, VkEvent _event, const char *name);
