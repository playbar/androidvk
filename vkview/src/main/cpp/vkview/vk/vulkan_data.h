#ifndef __BAR_VULKAN_DATA_H__
#define __BAR_VULKAN_DATA_H__

#include <android/log.h>
#include <android/asset_manager.h>

#include <vector>
#include <array>
#include <string>
#include <glm/glm.hpp>
#include <vulkan_wrapper.h>

#define VERTEXT_BUFFER_ID 1

struct QueueFamilyIndices {
    int graphicsFamily = -1;
    int presentFamily = -1;

    bool isComplete() {
        return graphicsFamily >= 0 && presentFamily >= 0;
    }
};

struct SwapchainSupportDetails {
    VkSurfaceCapabilitiesKHR capabilities;
    std::vector<VkSurfaceFormatKHR> formats;
    std::vector<VkPresentModeKHR> presentModes;
};

struct Vertex {
    glm::vec2 pos;
    glm::vec2 texCoord;

    static VkVertexInputBindingDescription getBindingDescription() {
        VkVertexInputBindingDescription bindingDescription = {};
        bindingDescription.binding = VERTEXT_BUFFER_ID;
        bindingDescription.stride = sizeof(Vertex);
        bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

        return bindingDescription;
    }

    static std::array<VkVertexInputAttributeDescription, 2> getAttributeDescriptions() {
        std::array<VkVertexInputAttributeDescription, 2> attributeDescriptions = {};

        attributeDescriptions[0].binding = VERTEXT_BUFFER_ID;
        attributeDescriptions[0].location = 0;
        attributeDescriptions[0].format = VK_FORMAT_R32G32_SFLOAT;
        attributeDescriptions[0].offset = offsetof(Vertex, pos);

        attributeDescriptions[1].binding = VERTEXT_BUFFER_ID;
        attributeDescriptions[1].location = 1;
        attributeDescriptions[1].format = VK_FORMAT_R32G32_SFLOAT;
        attributeDescriptions[1].offset = offsetof(Vertex, texCoord);

        return attributeDescriptions;
    }

};

struct UniformBufferObject {
    glm::mat4 model;
    glm::mat4 view;
};

struct UniformBufferProj {
    glm::mat4 proj;
};


#endif //__BAR_VULKAN_DATA_H__
