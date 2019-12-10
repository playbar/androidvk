#ifndef __BAR_VULKAN_DEVICE_H__
#define __BAR_VULKAN_DEVICE_H__

#include <android/log.h>
#include <android/asset_manager.h>
#include <glm/glm.hpp>
#include <vulkan_wrapper.h>
#include "vulkan_data.h"
#include "vk_mem_alloc.h"


class VulkanDevice {
public:
    VulkanDevice();
    ~VulkanDevice();

    void destroy();

public:
    void createInstance();
    void setUpDebugCallback();
    void pickPhysicalDevice();
    void createLogicalDevice();
    void createSurface(ANativeWindow *window);
    void createVmaAlloc();

    bool isDeviceSuitable(VkPhysicalDevice device);
    bool checkDeviceExtensionSupport(VkPhysicalDevice device);
    SwapchainSupportDetails querySwapchainSupport(VkPhysicalDevice device);
    QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device);
    uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);
    void createCommandPool();
    void resetCommandPool();
    VkCommandBuffer beginSingleTimeCommands();
    void endSingleTimeCommands(VkCommandBuffer commandBuffer);

    VmaAllocator get_memory_allocator() const;
    
    VkDeviceSize GetUniformBufferAlignment() const
    {
        return m_device_properties.limits.minUniformBufferOffsetAlignment;
    }
    VkDeviceSize GetTexelBufferAlignment() const
    {
        return m_device_properties.limits.minUniformBufferOffsetAlignment;
    }
    VkDeviceSize GetBufferImageGranularity() const
    {
        return m_device_properties.limits.bufferImageGranularity;
    }
    float GetMaxSamplerAnisotropy() const
    {
        return m_device_properties.limits.maxSamplerAnisotropy;
    }

public:
    VkPhysicalDeviceProperties m_device_properties = {};
    VkDebugReportCallbackEXT callback;
    VkInstance mInstance;
    VkPhysicalDevice mPhysicalDevice = VK_NULL_HANDLE;
    VkDevice mLogicalDevice;
    VkSurfaceKHR mSurface;
    VkCommandPool mCommandPool;

    VkQueue mGraphicsQueue;
    VkQueue mPresentQueue;

    VmaAllocator memory_allocator{VK_NULL_HANDLE};


};


#endif //__BAR_VULKAN_DEVICE_H__
