#ifndef __BAR_VULKAN_UTILS_H__
#define __BAR_VULKAN_UTILS_H__

#include <android/log.h>
#include <android/asset_manager.h>

#include <vector>
#include <array>
#include <string>

#include <glm/glm.hpp>
#include <vulkan_wrapper.h>

#include "vulkan_data.h"
#include "vulkan_device.h"
#include "vulkan_buffer.h"
#include "vulkan_texture.h"

#define STATE_RUNNING 1
#define STATE_PAUSED 2
#define STATE_EXIT 3

class VulkanUtils {
public:

    VulkanUtils(AAssetManager *assetManager);
    VulkanUtils();
    ~VulkanUtils();

public:
    void OnSurfaceCreated();
    void OnSurfaceChanged();
    void OnDrawFrame();

public:
    void start();
    void pause();

    inline void resume() { state = STATE_RUNNING; }

    inline void stop() { state = STATE_EXIT; }

    inline void initWindow(ANativeWindow *window) { this->window = window; }

    void createSurfaceDevice();

    void cleanUp();

public:
    void createSwapchain();
    void recreateSwapchain();
    void cleanupSwapchain();

    void createImageViews();
    void createRenderPass();
    void createCommandBuffers();


public:
    void createGraphicsPipelineTest();

    void createPipeline();
    void createMVPPipeline();
    void createDescriptorSetLayout();
    void createMVPDescriptorSetLayout();
    VkPipeline mPipeline;
    VkPipelineLayout mPipelineLayout;
    VkDescriptorSetLayout mDescriptorSetLayout;

    VkPipeline mMVPPipeline;
    VkPipelineLayout mMVPPipelineLayout;
    VkDescriptorSetLayout mMVPDescriptorSetLayout;

    void createFramebuffers();

public:
    // no use
    void bindDescriptorSet();
    void bindDescriptorSetTexture(HVkTexture &texImg);
    void bindDescriptorSetTexture1(HVkTexture &texImg);
    VkDescriptorSet mDescriptorSet;
    VkDescriptorSet mDescriptorSet1;

public:


    void createDescriptorPool();
    VkDescriptorSet createDescriptorSet();
    void updateUniformBuffer();
    void drawCommandBuffers();

    VkDescriptorSet createMVPDescriptorSet();
    void updateUniformBufferMVP();
    void drawCommandBuffersMVP();

public:
    void createSemaphores();
    void AcquireNextImage();
    void drawFrame();
    void QueuePresent();
    uint32_t mImageIndex = 0;

    std::vector<char> readAsset(std::string name);

    VkShaderModule createShaderModule(const std::vector<char> &code);
    VkShaderModule createShaderModule(const std::vector<uint32_t> &code);

    AAssetManager *mAssetManager;
    int state;

    ANativeWindow *window;
    VulkanDevice mVKDevice;

    VkSwapchainKHR swapchain;
    VkRenderPass renderPass;
    VkDescriptorPool mDescriptorPool;


    VkFormat swapchainImageFormat;
    VkExtent2D swapchainExtent;
    std::vector<VkImage> mSwapchainImages;
    std::vector<VkImageView> mSwapchainImageViews;
    std::vector<VkFramebuffer> mFramebuffers;
    std::vector<VkCommandBuffer> mCommandBuffers;
    std::vector<VkDescriptorPool >mDescriptorPools;


    std::vector<HVkBuffer*> mVertexBuffers;
    std::vector<HVkBuffer*> mIndexBuffers;
    std::vector<HVkBuffer*> mUniformBuffers;

//    HVkBuffer mVertexBuffer;
//    HVkBuffer mIndexBuffer;
//    HVkBuffer mUniformBuffer;
//    void createVertexBuffer();
//    void createIndexBuffer();
//    void createUniformBuffer();
    void createCacheBuffers();

    HVkTexture mTexImage;
    HVkTexture mTexImage1;

    VkSemaphore mImageAvailableSemaphore;
    VkSemaphore mRenderFinishedSemaphore;
};

#endif //__BAR_VULKAN_UTILS_H__
