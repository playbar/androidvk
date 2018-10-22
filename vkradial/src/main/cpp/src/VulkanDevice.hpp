/*
* Vulkan device class
*
* Encapsulates a physical Vulkan device and it's logical representation
*
* Copyright (C) 2016 by Sascha Willems - www.saschawillems.de
*
* This code is licensed under the MIT license (MIT) (http://opensource.org/licenses/MIT)
*/

#pragma once

#include <exception>
#include <assert.h>
#include <algorithm>
#include "vulkan/vulkan.h"
#include "VulkanTools.h"
#include "VulkanBuffer.hpp"


struct VulkanDevice
{
	VkPhysicalDevice mPhysicalDevice;
	VkDevice mLogicalDevice;
	VkPhysicalDeviceProperties properties;
	VkPhysicalDeviceFeatures features;
	VkPhysicalDeviceMemoryProperties memoryProperties;
	std::vector<VkQueueFamilyProperties> queueFamilyProperties;
	std::vector<std::string> supportedExtensions;

	VkCommandPool commandPool = VK_NULL_HANDLE;

	bool enableDebugMarkers = false;

	struct
	{
		uint32_t graphics;
		uint32_t compute;
		uint32_t transfer;
	} queueFamilyIndices;

	operator VkDevice();

	VulkanDevice(VkPhysicalDevice physicalDevice);

	~VulkanDevice();

	uint32_t getMemoryType(uint32_t typeBits, VkMemoryPropertyFlags properties, VkBool32 *memTypeFound = nullptr);

	uint32_t getQueueFamilyIndex(VkQueueFlagBits queueFlags);

	VkResult createLogicalDevice(VkPhysicalDeviceFeatures enabledFeatures, std::vector<const char*> enabledExtensions, bool useSwapChain = true,
								 VkQueueFlags requestedQueueTypes = VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT);

	VkResult createBuffer(VkBufferUsageFlags usageFlags, VkMemoryPropertyFlags memoryPropertyFlags, VkDeviceSize size,
						  VkBuffer *buffer, VkDeviceMemory *memory, void *data = nullptr);

	VkResult createBuffer(VkBufferUsageFlags usageFlags, VkMemoryPropertyFlags memoryPropertyFlags, VksBuffer *buffer,
						  VkDeviceSize size, void *data = nullptr);

	void copyBuffer(VksBuffer *src, VksBuffer *dst, VkQueue queue, VkBufferCopy *copyRegion = nullptr);

	VkCommandPool createCommandPool(uint32_t queueFamilyIndex, VkCommandPoolCreateFlags createFlags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);

	VkCommandBuffer createCommandBuffer(VkCommandBufferLevel level, bool begin = false);

	void flushCommandBuffer(VkCommandBuffer commandBuffer, VkQueue queue, bool free = true);

	bool extensionSupported(std::string extension);

};

