#ifndef __VULKANMAIN_HPP__
#define __VULKANMAIN_HPP__

// Initialize vulkan device context
// after return, vulkan is ready to draw

#include <android/asset_manager.h>
#include "android/native_window.h"

bool InitVulkan(ANativeWindow* app, AAssetManager* mgr);

// delete vulkan device context when application goes away
void DeleteVulkan(void);

// Check if vulkan is ready to draw
bool IsVulkanReady(void);

// Ask Vulkan to Render a frame
bool VulkanDrawFrame(void);

#endif // __VULKANMAIN_HPP__


