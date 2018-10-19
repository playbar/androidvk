/*
* Assorted Vulkan helper functions
*
* Copyright (C) 2016 by Sascha Willems - www.saschawillems.de
*
* This code is licensed under the MIT license (MIT) (http://opensource.org/licenses/MIT)
*/

#pragma once

#include "vulkan/vulkan.h"
#include "VulkanInitializers.hpp"

#include <math.h>
#include <stdlib.h>
#include <string>
#include <cstring>
#include <fstream>
#include <assert.h>
#include <stdio.h>
#include <vector>
#include <iostream>
#include <stdexcept>
#if defined(_WIN32)
#include <windows.h>
#include <fcntl.h>
#include <io.h>
#elif defined(__ANDROID__)
#include "vulkanandroid.h"
#include <android/asset_manager.h>
#endif

#define VK_FLAGS_NONE 0
#define DEFAULT_FENCE_TIMEOUT 100000000000

// Macro to check and display Vulkan return results
#define VK_CHECK_RESULT(f)																				\
{																										\
	VkResult res = (f);																					\
	if (res != VK_SUCCESS)																				\
	{																									\
		LOGE("Fatal : VkResult is \" %s \" in %s at line %d", VksErrorString(res).c_str(), __FILE__, __LINE__); \
		assert(res == VK_SUCCESS);																		\
	}																									\
}


/** @brief Returns an error code as a string */
std::string VksErrorString(VkResult errorCode);

/** @brief Returns the device type as a string */
std::string VksPhysicalDeviceTypeString(VkPhysicalDeviceType type);

// Selected a suitable supported depth format starting with 32 bit down to 16 bit
// Returns false if none of the depth formats in the list is supported by the device
VkBool32 VksGetSupportedDepthFormat(VkPhysicalDevice physicalDevice, VkFormat *depthFormat);

// Put an image memory barrier for setting an image layout on the sub resource into the given command buffer
void VksSetImageLayout(
		VkCommandBuffer cmdbuffer,
		VkImage image,
		VkImageAspectFlags aspectMask,
		VkImageLayout oldImageLayout,
		VkImageLayout newImageLayout,
		VkImageSubresourceRange subresourceRange,
		VkPipelineStageFlags srcStageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
		VkPipelineStageFlags dstStageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);

// Uses a fixed sub resource layout with first mip level and layer
void VksSetImageLayout(
		VkCommandBuffer cmdbuffer,
		VkImage image,
		VkImageAspectFlags aspectMask,
		VkImageLayout oldImageLayout,
		VkImageLayout newImageLayout,
		VkPipelineStageFlags srcStageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
		VkPipelineStageFlags dstStageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);

/** @brief Inser an image memory barrier into the command buffer */
void VksInsertImageMemoryBarrier(
		VkCommandBuffer cmdbuffer,
		VkImage image,
		VkAccessFlags srcAccessMask,
		VkAccessFlags dstAccessMask,
		VkImageLayout oldImageLayout,
		VkImageLayout newImageLayout,
		VkPipelineStageFlags srcStageMask,
		VkPipelineStageFlags dstStageMask,
		VkImageSubresourceRange subresourceRange);

// Display error message and exit on fatal error
void VksExitFatal(std::string message, std::string caption);

// Load a SPIR-V shader (binary)
#if defined(__ANDROID__)
VkShaderModule VksLoadShader(AAssetManager *assetManager, const char *fileName, VkDevice device,
							 VkShaderStageFlagBits stage);
#else
VkShaderModule VksLoadShader(const char *fileName, VkDevice device, VkShaderStageFlagBits stage);
#endif

// Load a GLSL shader (text)
// Note: GLSL support requires vendor-specific extensions to be enabled and is not a core-feature of Vulkan
VkShaderModule VksLoadShaderGLSL(const char *fileName, VkDevice device, VkShaderStageFlagBits stage);


