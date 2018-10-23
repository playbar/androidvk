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
extern bool gVksDebugMarkerActive;

extern int gVksDebugValidationLayerCount;
extern const char *gVksDebugValidationLayerNames[];

// Default debug callback
VkBool32 VksDebugessageCallback(
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
void VksDebugSetupDebugging(
        VkInstance instance,
        VkDebugReportFlagsEXT flags,
        VkDebugReportCallbackEXT callBack);
// Clear debug callback
void VksDebugFreeDebugCallback(VkInstance instance);


// Setup and functions for the VK_EXT_debug_marker_extension
// Extension spec can be found at https://github.com/KhronosGroup/Vulkan-Docs/blob/1.0-VK_EXT_debug_marker/doc/specs/vulkan/appendices/VK_EXT_debug_marker.txt
// Note that the extension will only be present if run from an offline debugging application
// The actual check for extension presence and enabling it on the device is done in the example base class
// See VulkanExampleBase::createInstance and VulkanExampleBase::createDevice (base/vulkanexamplebase.cpp)

// Get function pointers for the debug report extensions from the device
void VksDebugMarkerSetup(VkDevice device);

// Sets the debug name of an object
// All Objects in Vulkan are represented by their 64-bit handles which are passed into this function
// along with the object type
void VksDebugMarkerSetObjectName(VkDevice device, uint64_t object,
                                 VkDebugReportObjectTypeEXT objectType, const char *name);

// Set the tag for an object
void VksDebugMarkerSetObjectTag(VkDevice device, uint64_t object,
                                VkDebugReportObjectTypeEXT objectType, uint64_t name,
                                size_t tagSize, const void *tag);

// Start a new debug marker region
void VksDebugMarkerBeginRegion(VkCommandBuffer cmdbuffer, const char *pMarkerName,
                               glm::vec4 color);

// Insert a new debug marker into the command buffer
void VksDebugMarkerInsert(VkCommandBuffer cmdbuffer, std::string markerName, glm::vec4 color);

// End the current debug marker region
void VksDebugMarkerEndRegion(VkCommandBuffer cmdBuffer);

// Object specific naming functions
void VksDebugMarkerSetCommandBufferName(VkDevice device, VkCommandBuffer cmdBuffer,
                                        const char *name);
void VksDebugMarkerSetQueueName(VkDevice device, VkQueue queue, const char *name);
void VksDebugMarkerSetImageName(VkDevice device, VkImage image, const char *name);
void VksDebugMarkerSetSamplerName(VkDevice device, VkSampler sampler, const char *name);
void VksDebugMarkerSetBufferName(VkDevice device, VkBuffer buffer, const char *name);
void VksDebugMarkerSetDeviceMemoryName(VkDevice device, VkDeviceMemory memory,
                                       const char *name);
void VksDebugMarkerSetShaderModuleName(VkDevice device, VkShaderModule shaderModule,
                                       const char *name);
void VksDebugMarkerSetPipelineName(VkDevice device, VkPipeline pipeline, const char *name);
void VksDebugMarkerSetPipelineLayoutName(VkDevice device, VkPipelineLayout pipelineLayout,
                                         const char *name);
void VksDebugMarkerSetRenderPassName(VkDevice device, VkRenderPass renderPass,
                                     const char *name);
void VksDebugMarkerSetFramebufferName(VkDevice device, VkFramebuffer framebuffer,
                                      const char *name);
void VksDebugMarkerSetDescriptorSetLayoutName(VkDevice device,
                                              VkDescriptorSetLayout descriptorSetLayout,
                                              const char *name);
void VksDebugMarkerSetDescriptorSetName(VkDevice device, VkDescriptorSet descriptorSet,
                                        const char *name);
void VksDebugMarkerSetSemaphoreName(VkDevice device, VkSemaphore semaphore,
                                    const char *name);
void VksDebugMarkerSetFenceName(VkDevice device, VkFence fence, const char *name);
void VksDebugMarkerSetEventName(VkDevice device, VkEvent _event, const char *name);

