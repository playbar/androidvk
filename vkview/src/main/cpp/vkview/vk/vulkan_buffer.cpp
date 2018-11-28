#include <cstring>
#include <mylog.h>
#include "vulkan_buffer.h"
#include "vulkan_utils.h"


HVkBuffer::HVkBuffer(VulkanDevice *device)
{
	mVkDevice = device;
	mMemory = NULL;
	mBuffer = NULL;
	mSize = 0;
	mOffset = OFFSET_VALUE;
}

HVkBuffer::~HVkBuffer()
{
//	destroy();
}


void HVkBuffer::createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties)
{
	float data[2];
	data[0] = 1;
	data[1] = 2;

	uint32_t leng = VK_WHOLE_SIZE;
	leng = sizeof( data);
	mSize = size;
	VkBufferCreateInfo bufferInfo = {
			.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
			.size = size,
			.usage = usage,
			.sharingMode = VK_SHARING_MODE_EXCLUSIVE,
	};
	if (vkCreateBuffer(mVkDevice->mLogicalDevice, &bufferInfo, nullptr, &mBuffer) != VK_SUCCESS) {
		throw std::runtime_error("failed to create buffer!");
	}

	VkMemoryRequirements memRequirements;
	vkGetBufferMemoryRequirements(mVkDevice->mLogicalDevice, mBuffer, &memRequirements);

    mAlignment = memRequirements.alignment;

	LOGE("vkbuffer, size:%lld, align:%lld, membit:%d", memRequirements.size, memRequirements.alignment,
		memRequirements.memoryTypeBits);

	VkMemoryAllocateInfo allocInfo = {
			.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
			.allocationSize = memRequirements.size,
			.memoryTypeIndex = mVkDevice->findMemoryType(memRequirements.memoryTypeBits, properties),
	};

	if (vkAllocateMemory(mVkDevice->mLogicalDevice, &allocInfo, nullptr, &mMemory) != VK_SUCCESS) {
		throw std::runtime_error("failed to allocate buffer memory!");
	}

	vkBindBufferMemory(mVkDevice->mLogicalDevice, mBuffer, mMemory, 0);
//	void* mapped_ptr = nullptr;
//	vkMapMemory(mVkDevice->mLogicalDevice, mMemory, 0, mSize, 0, &mapped_ptr);
//	mpData = (unsigned char*)mapped_ptr;
	return;
}

VkResult HVkBuffer::map(VkDeviceSize size, VkDeviceSize offset)
{
	void* mapped_ptr = nullptr;
	VkResult re = vkMapMemory(mVkDevice->mLogicalDevice, mMemory, offset, size, 0, &mapped_ptr);
	mpData = (unsigned char*)mapped_ptr;
	return re;

}

void HVkBuffer::copyTo(void* data, VkDeviceSize size)
{
    assert(mpData);
    memcpy(mpData, data, size);
}

void HVkBuffer::updateData(void* data, uint32_t length)
{
	void* mapped_ptr = nullptr;
	vkMapMemory(mVkDevice->mLogicalDevice, mMemory, 0, mSize, 0, &mapped_ptr);
	mpData = (unsigned char*)mapped_ptr;

	if( mOffset + length >= mSize )
	{
		mOffset = OFFSET_VALUE;
	}
	memcpy(mpData + mOffset, data, length);

	if (mpData)
	{
		vkUnmapMemory(mVkDevice->mLogicalDevice, mMemory);
		mpData = nullptr;
	}
}

void HVkBuffer::unmap()
{
	if (mpData)
	{
		vkUnmapMemory(mVkDevice->mLogicalDevice, mMemory);
        mpData = nullptr;
	}
}

void HVkBuffer::copyBuffer(HVkBuffer &srcBuffer)
{
	VkCommandBufferAllocateInfo allocInfo = {
			.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
			.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
			.commandPool = mVkDevice->mCommandPool,
			.commandBufferCount = 1,
	};

	VkCommandBuffer commandBuffer;
	vkAllocateCommandBuffers(mVkDevice->mLogicalDevice, &allocInfo, &commandBuffer);

	VkCommandBufferBeginInfo beginInfo = {
			.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
			.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
	};
	vkBeginCommandBuffer(commandBuffer, &beginInfo);

	VkBufferCopy copyRegion = {
			.size = srcBuffer.mSize
	};
	vkCmdCopyBuffer(commandBuffer, srcBuffer.mBuffer, mBuffer, 1, &copyRegion);

	vkEndCommandBuffer(commandBuffer);

	VkSubmitInfo submitInfo = {
			.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
			.commandBufferCount = 1,
			.pCommandBuffers = &commandBuffer,
	};
	vkQueueSubmit(mVkDevice->graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
	vkQueueWaitIdle(mVkDevice->graphicsQueue);

	vkFreeCommandBuffers(mVkDevice->mLogicalDevice, mVkDevice->mCommandPool, 1, &commandBuffer);
	return;
}


VkResult HVkBuffer::bind(VkDeviceSize offset)
{
	return vkBindBufferMemory(mVkDevice->mLogicalDevice, mBuffer, mMemory, offset);
}

void HVkBuffer::setupDescriptor(VkDeviceSize size, VkDeviceSize offset)
{
	mDescriptor.offset = offset;
	mDescriptor.buffer = mBuffer;
	mDescriptor.range = size;
}


static uint32_t AlignUp(uint32_t value, size_t size)
{
	return (value + (size - value % size) % size);
}

uint32_t HVkBuffer::calc_align(uint32_t n,uint32_t align)
{
	return ((n + align - 1) & (~(align - 1)));
//    if ( n / align * align == n)
//        return n;
//
//    return  (n / align + 1) * align;
}

VkResult HVkBuffer::flush(VkDeviceSize size)
{
	VkMappedMemoryRange mappedRange = {};
	mappedRange.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
	mappedRange.memory = mMemory;
	mappedRange.offset = mOffset;
	mappedRange.size = size;
	VkResult re = vkFlushMappedMemoryRanges(mVkDevice->mLogicalDevice, 1, &mappedRange);

	mOffset += calc_align(size, mAlignment);

	if( mOffset >= mSize )
	{
		mOffset = OFFSET_VALUE;
	}
	return re;
}

void HVkBuffer::reset()
{
    mOffset = OFFSET_VALUE;
}


VkResult HVkBuffer::invalidate(VkDeviceSize size, VkDeviceSize offset)
{
	VkMappedMemoryRange mappedRange = {};
	mappedRange.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
	mappedRange.memory = mMemory;
	mappedRange.offset = offset;
	mappedRange.size = size;
	return vkInvalidateMappedMemoryRanges(mVkDevice->mLogicalDevice, 1, &mappedRange);
}


void HVkBuffer::destroy()
{
	if (mpData)
	{
		vkUnmapMemory(mVkDevice->mLogicalDevice, mMemory);
		mpData = nullptr;
	}

	if (mBuffer)
	{
		vkDestroyBuffer(mVkDevice->mLogicalDevice, mBuffer, nullptr);
		mBuffer = NULL;
	}
	if (mMemory)
	{
		vkFreeMemory(mVkDevice->mLogicalDevice, mMemory, nullptr);
		mMemory = NULL;
	}
}

