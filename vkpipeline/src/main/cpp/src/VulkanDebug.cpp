/*
* Vulkan examples debug wrapper
* 
* Appendix for VK_EXT_Debug_Report can be found at https://github.com/KhronosGroup/Vulkan-Docs/blob/1.0-VK_EXT_debug_report/doc/specs/vulkan/appendices/debug_report.txt
*
* Copyright (C) 2016 by Sascha Willems - www.saschawillems.de
*
* This code is licensed under the MIT license (MIT) (http://opensource.org/licenses/MIT)
*/

#include "VulkanDebug.h"
#include <iostream>

namespace vks
{
	namespace debug
	{
		int validationLayerCount = 1;
		const char *validationLayerNames[] =
		{
			// This is a meta layer that enables all of the standard
			// validation layers in the correct order :
			// threading, parameter_validation, device_limits, object_tracker, image, core_validation, swapchain, and unique_objects
			"VK_LAYER_LUNARG_standard_validation"
		};

		PFN_vkCreateDebugReportCallbackEXT CreateDebugReportCallback = VK_NULL_HANDLE;
		PFN_vkDestroyDebugReportCallbackEXT DestroyDebugReportCallback = VK_NULL_HANDLE;
		PFN_vkDebugReportMessageEXT dbgBreakCallback = VK_NULL_HANDLE;

		VkDebugReportCallbackEXT msgCallback;

		VkBool32 VksDebugessageCallback(
                VkDebugReportFlagsEXT flags,
                VkDebugReportObjectTypeEXT objType,
                uint64_t srcObject,
                size_t location,
                int32_t msgCode,
                const char *pLayerPrefix,
                const char *pMsg,
                void *pUserData)
		{
			// Select prefix depending on flags passed to the callback
			// Note that multiple flags may be set for a single validation message
			std::string prefix("");

			// Error that may result in undefined behaviour
			if (flags & VK_DEBUG_REPORT_ERROR_BIT_EXT)
			{
				prefix += "ERROR:";
			};
			// Warnings may hint at unexpected / non-spec API usage
			if (flags & VK_DEBUG_REPORT_WARNING_BIT_EXT)
			{
				prefix += "WARNING:";
			};
			// May indicate sub-optimal usage of the API
			if (flags & VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT)
			{
				prefix += "PERFORMANCE:";
			};
			// Informal messages that may become handy during debugging
			if (flags & VK_DEBUG_REPORT_INFORMATION_BIT_EXT)
			{
				prefix += "INFO:";
			}
			// Diagnostic info from the Vulkan loader and layers
			// Usually not helpful in terms of API usage, but may help to debug layer and loader problems 
			if (flags & VK_DEBUG_REPORT_DEBUG_BIT_EXT)
			{
				prefix += "DEBUG:";
			}

			// Display message to default output (console if activated)
			std::cout << prefix << " [" << pLayerPrefix << "] Code " << msgCode << " : " << pMsg << "\n";

			fflush(stdout);

			// The return value of this callback controls wether the Vulkan call that caused
			// the validation message will be aborted or not
			// We return VK_FALSE as we DON'T want Vulkan calls that cause a validation message 
			// (and return a VkResult) to abort
			// If you instead want to have calls abort, pass in VK_TRUE and the function will 
			// return VK_ERROR_VALIDATION_FAILED_EXT 
			return VK_FALSE;
		}

		void VksDebugSetupDebugging(VkInstance instance, VkDebugReportFlagsEXT flags,
                                    VkDebugReportCallbackEXT callBack)
		{
			CreateDebugReportCallback = reinterpret_cast<PFN_vkCreateDebugReportCallbackEXT>(vkGetInstanceProcAddr(instance, "vkCreateDebugReportCallbackEXT"));
			DestroyDebugReportCallback = reinterpret_cast<PFN_vkDestroyDebugReportCallbackEXT>(vkGetInstanceProcAddr(instance, "vkDestroyDebugReportCallbackEXT"));
			dbgBreakCallback = reinterpret_cast<PFN_vkDebugReportMessageEXT>(vkGetInstanceProcAddr(instance, "vkDebugReportMessageEXT"));

			VkDebugReportCallbackCreateInfoEXT dbgCreateInfo = {};
			dbgCreateInfo.sType = VK_STRUCTURE_TYPE_DEBUG_REPORT_CREATE_INFO_EXT;
			dbgCreateInfo.pfnCallback = (PFN_vkDebugReportCallbackEXT)VksDebugessageCallback;
			dbgCreateInfo.flags = flags;

			VkResult err = CreateDebugReportCallback(
				instance,
				&dbgCreateInfo,
				nullptr,
				(callBack != VK_NULL_HANDLE) ? &callBack : &msgCallback);
			assert(!err);
		}

		void VksDebugFreeDebugCallback(VkInstance instance)
		{
			if (msgCallback != VK_NULL_HANDLE)
			{
				DestroyDebugReportCallback(instance, msgCallback, nullptr);
			}
		}
	}

	namespace debugmarker
	{
		bool active = false;

		PFN_vkDebugMarkerSetObjectTagEXT pfnDebugMarkerSetObjectTag = VK_NULL_HANDLE;
		PFN_vkDebugMarkerSetObjectNameEXT pfnDebugMarkerSetObjectName = VK_NULL_HANDLE;
		PFN_vkCmdDebugMarkerBeginEXT pfnCmdDebugMarkerBegin = VK_NULL_HANDLE;
		PFN_vkCmdDebugMarkerEndEXT pfnCmdDebugMarkerEnd = VK_NULL_HANDLE;
		PFN_vkCmdDebugMarkerInsertEXT pfnCmdDebugMarkerInsert = VK_NULL_HANDLE;

		void VksDebugMarkerSetup(VkDevice device)
		{
			pfnDebugMarkerSetObjectTag = reinterpret_cast<PFN_vkDebugMarkerSetObjectTagEXT>(vkGetDeviceProcAddr(device, "vkDebugMarkerSetObjectTagEXT"));
			pfnDebugMarkerSetObjectName = reinterpret_cast<PFN_vkDebugMarkerSetObjectNameEXT>(vkGetDeviceProcAddr(device, "vkDebugMarkerSetObjectNameEXT"));
			pfnCmdDebugMarkerBegin = reinterpret_cast<PFN_vkCmdDebugMarkerBeginEXT>(vkGetDeviceProcAddr(device, "vkCmdDebugMarkerBeginEXT"));
			pfnCmdDebugMarkerEnd = reinterpret_cast<PFN_vkCmdDebugMarkerEndEXT>(vkGetDeviceProcAddr(device, "vkCmdDebugMarkerEndEXT"));
			pfnCmdDebugMarkerInsert = reinterpret_cast<PFN_vkCmdDebugMarkerInsertEXT>(vkGetDeviceProcAddr(device, "vkCmdDebugMarkerInsertEXT"));

			// Set flag if at least one function pointer is present
			active = (pfnDebugMarkerSetObjectName != VK_NULL_HANDLE);
		}

		void VksDebugMarkerSetObjectName(VkDevice device, uint64_t object,
										 VkDebugReportObjectTypeEXT objectType, const char *name)
		{
			// Check for valid function pointer (may not be present if not running in a debugging application)
			if (pfnDebugMarkerSetObjectName)
			{
				VkDebugMarkerObjectNameInfoEXT nameInfo = {};
				nameInfo.sType = VK_STRUCTURE_TYPE_DEBUG_MARKER_OBJECT_NAME_INFO_EXT;
				nameInfo.objectType = objectType;
				nameInfo.object = object;
				nameInfo.pObjectName = name;
				pfnDebugMarkerSetObjectName(device, &nameInfo);
			}
		}

		void VksDebugMarkerSetObjectTag(VkDevice device, uint64_t object,
										VkDebugReportObjectTypeEXT objectType, uint64_t name,
										size_t tagSize, const void *tag)
		{
			// Check for valid function pointer (may not be present if not running in a debugging application)
			if (pfnDebugMarkerSetObjectTag)
			{
				VkDebugMarkerObjectTagInfoEXT tagInfo = {};
				tagInfo.sType = VK_STRUCTURE_TYPE_DEBUG_MARKER_OBJECT_TAG_INFO_EXT;
				tagInfo.objectType = objectType;
				tagInfo.object = object;
				tagInfo.tagName = name;
				tagInfo.tagSize = tagSize;
				tagInfo.pTag = tag;
				pfnDebugMarkerSetObjectTag(device, &tagInfo);
			}
		}

		void VksDebugMarkerBeginRegion(VkCommandBuffer cmdbuffer, const char *pMarkerName,
									   glm::vec4 color)
		{
			// Check for valid function pointer (may not be present if not running in a debugging application)
			if (pfnCmdDebugMarkerBegin)
			{
				VkDebugMarkerMarkerInfoEXT markerInfo = {};
				markerInfo.sType = VK_STRUCTURE_TYPE_DEBUG_MARKER_MARKER_INFO_EXT;
				memcpy(markerInfo.color, &color[0], sizeof(float) * 4);
				markerInfo.pMarkerName = pMarkerName;
				pfnCmdDebugMarkerBegin(cmdbuffer, &markerInfo);
			}
		}

		void VksDebugMarkerInsert(VkCommandBuffer cmdbuffer, std::string markerName, glm::vec4 color)
		{
			// Check for valid function pointer (may not be present if not running in a debugging application)
			if (pfnCmdDebugMarkerInsert)
			{
				VkDebugMarkerMarkerInfoEXT markerInfo = {};
				markerInfo.sType = VK_STRUCTURE_TYPE_DEBUG_MARKER_MARKER_INFO_EXT;
				memcpy(markerInfo.color, &color[0], sizeof(float) * 4);
				markerInfo.pMarkerName = markerName.c_str();
				pfnCmdDebugMarkerInsert(cmdbuffer, &markerInfo);
			}
		}

		void VksDebugMarkerEndRegion(VkCommandBuffer cmdBuffer)
		{
			// Check for valid function (may not be present if not runnin in a debugging application)
			if (pfnCmdDebugMarkerEnd)
			{
				pfnCmdDebugMarkerEnd(cmdBuffer);
			}
		}

		void VksDebugMarkerSetCommandBufferName(VkDevice device, VkCommandBuffer cmdBuffer,
												const char *name)
		{
			VksDebugMarkerSetObjectName(device, (uint64_t) cmdBuffer,
										VK_DEBUG_REPORT_OBJECT_TYPE_COMMAND_BUFFER_EXT, name);
		}

		void VksDebugMarkerSetQueueName(VkDevice device, VkQueue queue, const char *name)
		{
			VksDebugMarkerSetObjectName(device, (uint64_t) queue,
										VK_DEBUG_REPORT_OBJECT_TYPE_QUEUE_EXT, name);
		}

		void VksDebugMarkerSetImageName(VkDevice device, VkImage image, const char *name)
		{
			VksDebugMarkerSetObjectName(device, (uint64_t) image,
										VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_EXT, name);
		}

		void VksDebugMarkerSetSamplerName(VkDevice device, VkSampler sampler, const char *name)
		{
			VksDebugMarkerSetObjectName(device, (uint64_t) sampler,
										VK_DEBUG_REPORT_OBJECT_TYPE_SAMPLER_EXT, name);
		}

		void VksDebugMarkerSetBufferName(VkDevice device, VkBuffer buffer, const char *name)
		{
			VksDebugMarkerSetObjectName(device, (uint64_t) buffer,
										VK_DEBUG_REPORT_OBJECT_TYPE_BUFFER_EXT, name);
		}

		void VksDebugMarkerSetDeviceMemoryName(VkDevice device, VkDeviceMemory memory,
											   const char *name)
		{
			VksDebugMarkerSetObjectName(device, (uint64_t) memory,
										VK_DEBUG_REPORT_OBJECT_TYPE_DEVICE_MEMORY_EXT, name);
		}

		void VksDebugMarkerSetShaderModuleName(VkDevice device, VkShaderModule shaderModule,
											   const char *name)
		{
			VksDebugMarkerSetObjectName(device, (uint64_t) shaderModule,
										VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT, name);
		}

		void VksDebugMarkerSetPipelineName(VkDevice device, VkPipeline pipeline, const char *name)
		{
			VksDebugMarkerSetObjectName(device, (uint64_t) pipeline,
										VK_DEBUG_REPORT_OBJECT_TYPE_PIPELINE_EXT, name);
		}

		void VksDebugMarkerSetPipelineLayoutName(VkDevice device, VkPipelineLayout pipelineLayout,
												 const char *name)
		{
			VksDebugMarkerSetObjectName(device, (uint64_t) pipelineLayout,
										VK_DEBUG_REPORT_OBJECT_TYPE_PIPELINE_LAYOUT_EXT, name);
		}

		void VksDebugMarkerSetRenderPassName(VkDevice device, VkRenderPass renderPass,
											 const char *name)
		{
			VksDebugMarkerSetObjectName(device, (uint64_t) renderPass,
										VK_DEBUG_REPORT_OBJECT_TYPE_RENDER_PASS_EXT, name);
		}

		void VksDebugMarkerSetFramebufferName(VkDevice device, VkFramebuffer framebuffer,
											  const char *name)
		{
			VksDebugMarkerSetObjectName(device, (uint64_t) framebuffer,
										VK_DEBUG_REPORT_OBJECT_TYPE_FRAMEBUFFER_EXT, name);
		}

		void VksDebugMarkerSetDescriptorSetLayoutName(VkDevice device,
													  VkDescriptorSetLayout descriptorSetLayout,
													  const char *name)
		{
			VksDebugMarkerSetObjectName(device, (uint64_t) descriptorSetLayout,
										VK_DEBUG_REPORT_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT_EXT, name);
		}

		void VksDebugMarkerSetDescriptorSetName(VkDevice device, VkDescriptorSet descriptorSet,
												const char *name)
		{
			VksDebugMarkerSetObjectName(device, (uint64_t) descriptorSet,
										VK_DEBUG_REPORT_OBJECT_TYPE_DESCRIPTOR_SET_EXT, name);
		}

		void VksDebugMarkerSetSemaphoreName(VkDevice device, VkSemaphore semaphore,
											const char *name)
		{
			VksDebugMarkerSetObjectName(device, (uint64_t) semaphore,
										VK_DEBUG_REPORT_OBJECT_TYPE_SEMAPHORE_EXT, name);
		}

		void VksDebugMarkerSetFenceName(VkDevice device, VkFence fence, const char *name)
		{
			VksDebugMarkerSetObjectName(device, (uint64_t) fence,
										VK_DEBUG_REPORT_OBJECT_TYPE_FENCE_EXT, name);
		}

		void VksDebugMarkerSetEventName(VkDevice device, VkEvent _event, const char *name)
		{
			VksDebugMarkerSetObjectName(device, (uint64_t) _event,
										VK_DEBUG_REPORT_OBJECT_TYPE_EVENT_EXT, name);
		}
	};
}

