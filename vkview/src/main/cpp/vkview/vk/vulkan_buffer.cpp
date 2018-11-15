#include <cstring>
#include "vulkan_buffer.h"
#include "vulkan_utils.h"


HVkBuffer::HVkBuffer(VulkanDevice *device)
{
	mVkDevice = device;
	mMemory = NULL;
	mBuffer = NULL;
	mSize = 0;
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
	if (vkCreateBuffer(mVkDevice->logicalDevice, &bufferInfo, nullptr, &mBuffer) != VK_SUCCESS) {
		throw std::runtime_error("failed to create buffer!");
	}

	VkMemoryRequirements memRequirements;
	vkGetBufferMemoryRequirements(mVkDevice->logicalDevice, mBuffer, &memRequirements);

	VkMemoryAllocateInfo allocInfo = {
			.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
			.allocationSize = memRequirements.size,
			.memoryTypeIndex = mVkDevice->findMemoryType(memRequirements.memoryTypeBits, properties),
	};

	if (vkAllocateMemory(mVkDevice->logicalDevice, &allocInfo, nullptr, &mMemory) != VK_SUCCESS) {
		throw std::runtime_error("failed to allocate buffer memory!");
	}

	vkBindBufferMemory(mVkDevice->logicalDevice, mBuffer, mMemory, 0);
	void* mapped_ptr = nullptr;
	vkMapMemory(mVkDevice->logicalDevice, mMemory, 0, mSize, 0, &mapped_ptr);
	mpData = (unsigned char*)mapped_ptr;
	return;
}

VkResult HVkBuffer::map(VkDeviceSize size, VkDeviceSize offset)
{
	void* mapped_ptr = nullptr;
	VkResult re = vkMapMemory(mVkDevice->logicalDevice, mMemory, offset, size, 0, &mapped_ptr);
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
	if( mOffset + length >= mSize )
	{
		mOffset = 0;
	}
	memcpy(mpData + mOffset, data, length);
}

void HVkBuffer::unmap()
{
	if (mpData)
	{
		vkUnmapMemory(mVkDevice->logicalDevice, mMemory);
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
	vkAllocateCommandBuffers(mVkDevice->logicalDevice, &allocInfo, &commandBuffer);

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

	vkFreeCommandBuffers(mVkDevice->logicalDevice, mVkDevice->mCommandPool, 1, &commandBuffer);
	return;
}


VkResult HVkBuffer::bind(VkDeviceSize offset)
{
	return vkBindBufferMemory(mVkDevice->logicalDevice, mBuffer, mMemory, offset);
}

void HVkBuffer::setupDescriptor(VkDeviceSize size, VkDeviceSize offset)
{
	mDescriptor.offset = offset;
	mDescriptor.buffer = mBuffer;
	mDescriptor.range = size;
}



VkResult HVkBuffer::flush(VkDeviceSize size)
{
	VkMappedMemoryRange mappedRange = {};
	mappedRange.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
	mappedRange.memory = mMemory;
	mappedRange.offset = mOffset;
	mappedRange.size = size;
	VkResult re = vkFlushMappedMemoryRanges(mVkDevice->logicalDevice, 1, &mappedRange);
	mOffset += size;
	if( mOffset >= mSize )
	{
		mOffset = 0;
	}
	return re;
}

void HVkBuffer::reset()
{
    mOffset = 0;
}


VkResult HVkBuffer::invalidate(VkDeviceSize size, VkDeviceSize offset)
{
	VkMappedMemoryRange mappedRange = {};
	mappedRange.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
	mappedRange.memory = mMemory;
	mappedRange.offset = offset;
	mappedRange.size = size;
	return vkInvalidateMappedMemoryRanges(mVkDevice->logicalDevice, 1, &mappedRange);
}


void HVkBuffer::destroy()
{
	if (mpData)
	{
		vkUnmapMemory(mVkDevice->logicalDevice, mMemory);
		mpData = nullptr;
	}

	if (mBuffer)
	{
		vkDestroyBuffer(mVkDevice->logicalDevice, mBuffer, nullptr);
		mBuffer = NULL;
	}
	if (mMemory)
	{
		vkFreeMemory(mVkDevice->logicalDevice, mMemory, nullptr);
		mMemory = NULL;
	}
}

