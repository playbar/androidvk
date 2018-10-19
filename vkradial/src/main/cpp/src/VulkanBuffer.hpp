/*
* Vulkan buffer class
*
* Encapsulates a Vulkan buffer
*
* Copyright (C) 2016 by Sascha Willems - www.saschawillems.de
*
* This code is licensed under the MIT license (MIT) (http://opensource.org/licenses/MIT)
*/

#pragma once

#include <vector>

#include "vulkan/vulkan.h"
#include "VulkanTools.h"


/**
* @brief Encapsulates access to a Vulkan buffer backed up by device memory
* @note To be filled by an external source like the VulkanDevice
*/
struct VksBuffer
{
	VkBuffer mBuffer;
	VkDevice mDevice;
	VkDeviceMemory mMemory;
	VkDescriptorBufferInfo mDescriptor;
	VkDeviceSize mSize = 0;
	VkDeviceSize mAlignment = 0;
	void* mMapped = nullptr;

	VkBufferUsageFlags usageFlags;
	VkMemoryPropertyFlags memoryPropertyFlags;

    VksBuffer()
    {
        mBuffer= NULL;
        mDevice = NULL;
        mMemory = NULL;
		mSize = 0;
        mAlignment = 0;
        mMapped = NULL;
        usageFlags = 0;
        memoryPropertyFlags = 0;
    }

	VkResult map(VkDeviceSize size = VK_WHOLE_SIZE, VkDeviceSize offset = 0)
	{
		return vkMapMemory(mDevice, mMemory, offset, size, 0, &mMapped);
	}

	void unmap()
	{
		if (mMapped)
		{
			vkUnmapMemory(mDevice, mMemory);
			mMapped = nullptr;
		}
	}

	VkResult bind(VkDeviceSize offset = 0)
	{
		return vkBindBufferMemory(mDevice, mBuffer, mMemory, offset);
	}

	void setupDescriptor(VkDeviceSize size = VK_WHOLE_SIZE, VkDeviceSize offset = 0)
	{
		mDescriptor.offset = offset;
		mDescriptor.buffer = mBuffer;
		mDescriptor.range = size;
		if( size < 1000000 ){
			LOGE("size:%d", size);
		}
	}

	void copyTo(void* data, VkDeviceSize size)
	{
		assert(mMapped);
		memcpy(mMapped, data, size);
	}

	VkResult flush(VkDeviceSize size = VK_WHOLE_SIZE, VkDeviceSize offset = 0)
	{
		VkMappedMemoryRange mappedRange = {};
		mappedRange.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
		mappedRange.memory = mMemory;
		mappedRange.offset = offset;
		mappedRange.size = size;
		return vkFlushMappedMemoryRanges(mDevice, 1, &mappedRange);
	}

	VkResult invalidate(VkDeviceSize size = VK_WHOLE_SIZE, VkDeviceSize offset = 0)
	{
		VkMappedMemoryRange mappedRange = {};
		mappedRange.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
		mappedRange.memory = mMemory;
		mappedRange.offset = offset;
		mappedRange.size = size;
		return vkInvalidateMappedMemoryRanges(mDevice, 1, &mappedRange);
	}

	void destroy()
	{
		if (mBuffer)
		{
			vkDestroyBuffer(mDevice, mBuffer, nullptr);
			mBuffer = NULL;
		}
		if (mMemory)
		{
			vkFreeMemory(mDevice, mMemory, nullptr);
			mMemory = NULL;
		}
	}

};
