#ifndef __VULKAN_BUFFER_H__
#define __VULKAN_BUFFER_H__

#include <vector>

#include <vulkan_wrapper.h>
#include "vulkan_device.h"

#define MAX_BUFFER_SIZE 8192
#define OFFSET_VALUE 0

class HVkBuffer
{
public:
	VulkanDevice *mVkDevice;
	VkBuffer mBuffer;
	VkDeviceMemory mMemory;

	VkDescriptorBufferInfo mDescriptor;
	VkDeviceSize mSize = 0;
	VkDeviceSize mOffset = 0;
	VkDeviceSize mAlignment = 0;
	VkBufferUsageFlags usageFlags;
	VkMemoryPropertyFlags memoryPropertyFlags;
	unsigned  char* mpData = nullptr;
public:
	HVkBuffer(VulkanDevice *device);
	~HVkBuffer();

	void createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties);

	void createBuffer(VkDeviceSize size, VkBufferUsageFlags buffer_usage,
					  VmaMemoryUsage memory_usage, VmaAllocationCreateFlags flags);

	VkResult map(VkDeviceSize size = VK_WHOLE_SIZE, VkDeviceSize offset = 0);

	void copyTo(void* data, VkDeviceSize size);

	void updateData(void* data, uint32_t length);

	void unmap();

	void copyBuffer(HVkBuffer &srcBuffer);

	VkResult bind(VkDeviceSize offset = 0);

	void setupDescriptor(VkDeviceSize size = VK_WHOLE_SIZE, VkDeviceSize offset = 0);

	uint32_t calc_align(uint32_t n,uint32_t align);
	VkResult flush(VkDeviceSize size);

	VkResult invalidate(VkDeviceSize size = VK_WHOLE_SIZE, VkDeviceSize offset = 0);

	void reset();

	void destroy();

};

#endif
