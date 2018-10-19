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
#include "vulkanandroid.h"
#include <android/asset_manager.h>

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


std::string VksErrorString(VkResult errorCode);

std::string VksPhysicalDeviceTypeString(VkPhysicalDeviceType type);

VkBool32 VksGetSupportedDepthFormat(VkPhysicalDevice physicalDevice, VkFormat *depthFormat);

void VksSetImageLayout(
		VkCommandBuffer cmdbuffer,
		VkImage image,
		VkImageAspectFlags aspectMask,
		VkImageLayout oldImageLayout,
		VkImageLayout newImageLayout,
		VkImageSubresourceRange subresourceRange,
		VkPipelineStageFlags srcStageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
		VkPipelineStageFlags dstStageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);

void VksSetImageLayout(
		VkCommandBuffer cmdbuffer,
		VkImage image,
		VkImageAspectFlags aspectMask,
		VkImageLayout oldImageLayout,
		VkImageLayout newImageLayout,
		VkPipelineStageFlags srcStageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
		VkPipelineStageFlags dstStageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);

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


