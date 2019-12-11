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
	mbUseVma = false;
}

HVkBuffer::~HVkBuffer()
{
//	destroy();
}


void HVkBuffer::createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties)
{
	mbUseVma = false;
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

void HVkBuffer::createBuffer(VkDeviceSize size, VkBufferUsageFlags buffer_usage,
				  VmaMemoryUsage memory_usage, VmaAllocationCreateFlags flags)
{
	mSize = size;
//	persistent = (flags & VMA_ALLOCATION_CREATE_MAPPED_BIT) != 0;
	mbUseVma = true;
	mAlignment = 64;

	VkBufferCreateInfo buffer_info{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
	buffer_info.usage = buffer_usage;
	buffer_info.size  = size;

	VmaAllocationCreateInfo memory_info{};
	memory_info.flags = flags;
	memory_info.usage = memory_usage;

	VmaAllocationInfo allocation_info{};
	auto result = vmaCreateBuffer(mVkDevice->get_memory_allocator(),
											   &buffer_info, &memory_info,
											   &mBuffer, &mAllocation,
											   &allocation_info);

	mMemory = allocation_info.deviceMemory;

	if (persistent)
	{
		mpData = static_cast<uint8_t *>(allocation_info.pMappedData);
	}

}

VkResult HVkBuffer::map(VkDeviceSize size, VkDeviceSize offset)
{
	VkResult re = VK_SUCCESS;
	if( mbUseVma )
	{
		if (!mapped && !mpData)
		{
			re = vmaMapMemory(mVkDevice->get_memory_allocator(), mAllocation,
							  reinterpret_cast<void **>(&mpData));
			mapped = true;
		}
	}
	else
	{
		void* mapped_ptr = nullptr;
		re = vkMapMemory(mVkDevice->mLogicalDevice, mMemory, offset, size, 0, &mapped_ptr);
		mpData = (unsigned char*)mapped_ptr;
	}
	return re;

}

void HVkBuffer::copyTo(void* data, VkDeviceSize size)
{
    assert(mpData);
    memcpy(mpData, data, size);
}

void HVkBuffer::updateDataVk(void* data, uint32_t length)
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

void HVkBuffer::updateData(void* data, uint32_t length)
{
	if( mbUseVma )
	{
		if (mOffset + length >= mSize)
		{
			mOffset = OFFSET_VALUE;
		}
		if (persistent) {
			memcpy(mpData + mOffset, data, length);
//		flush(length);
		} else {
			map();
			memcpy(mpData + mOffset, data, length);
//		flush(length);
			unmap();
		}
	}
	else{
		updateDataVk(data, length);
	}
}

void HVkBuffer::unmapVk()
{
	if (mpData)
	{
		vkUnmapMemory(mVkDevice->mLogicalDevice, mMemory);
        mpData = nullptr;
	}
}

void HVkBuffer::unmap()
{
	if( mbUseVma ) {
		if (mapped) {
			vmaUnmapMemory(mVkDevice->get_memory_allocator(), mAllocation);
			mpData = nullptr;
			mapped = false;
		}
	}
	else
	{
		unmapVk();
	}
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

VkResult HVkBuffer::flushVk(VkDeviceSize size)
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

VkResult HVkBuffer::flush(VkDeviceSize size)
{

	VkResult re = VK_SUCCESS;
	if( mbUseVma) {
		vmaFlushAllocation(mVkDevice->get_memory_allocator(), mAllocation, 0, size);

		mOffset += calc_align(size, mAlignment);

		if (mOffset >= mSize) {
			mOffset = OFFSET_VALUE;
		}
	}else{
		flushVk(size);
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
	if( mbUseVma) {
		if (mBuffer != VK_NULL_HANDLE && mAllocation != VK_NULL_HANDLE) {
			unmap();
			vmaDestroyBuffer(mVkDevice->get_memory_allocator(), mBuffer, mAllocation);
		}
	}else {
		if (mpData) {
			vkUnmapMemory(mVkDevice->mLogicalDevice, mMemory);
			mpData = nullptr;
		}

		if (mBuffer) {
			vkDestroyBuffer(mVkDevice->mLogicalDevice, mBuffer, nullptr);
			mBuffer = NULL;
		}
		if (mMemory) {
			vkFreeMemory(mVkDevice->mLogicalDevice, mMemory, nullptr);
			mMemory = NULL;
		}
	}
}

