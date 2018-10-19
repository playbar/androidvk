/*
* Heightmap terrain generator
*
* Copyright (C) 2016 by Sascha Willems - www.saschawillems.de
*
* This code is licensed under the MIT license (MIT) (http://opensource.org/licenses/MIT)
*/

#include <glm/glm.hpp>
#include <glm/glm.hpp>
#include <gli/gli.hpp>

#include "vulkan/vulkan.h"
#include "VulkanDevice.hpp"
#include "VulkanBuffer.hpp"


class VksHeightMap
{
private:
	uint16_t *heightdata;
	uint32_t dim;
	uint32_t scale;

	VulkanDevice *device = nullptr;
	VkQueue copyQueue = VK_NULL_HANDLE;
public:
	enum Topology { topologyTriangles, topologyQuads };

	float heightScale = 1.0f;
	float uvScale = 1.0f;

	VksBuffer vertexBuffer;
	VksBuffer indexBuffer;

	struct Vertex {
		glm::vec3 pos;
		glm::vec3 normal;
		glm::vec2 uv;
	};

	size_t vertexBufferSize = 0;
	size_t indexBufferSize = 0;
	uint32_t indexCount = 0;

	VksHeightMap(VulkanDevice *device, VkQueue copyQueue)
	{
		this->device = device;
		this->copyQueue = copyQueue;
	};

	~VksHeightMap()
	{
		vertexBuffer.destroy();
		indexBuffer.destroy();
		delete[] heightdata;
	}

	float getHeight(uint32_t x, uint32_t y)
	{
		glm::ivec2 rpos = glm::ivec2(x, y) * glm::ivec2(scale);
		rpos.x = std::max(0, std::min(rpos.x, (int)dim - 1));
		rpos.y = std::max(0, std::min(rpos.y, (int)dim - 1));
		rpos /= glm::ivec2(scale);
		return *(heightdata + (rpos.x + rpos.y * dim) * scale) / 65535.0f * heightScale;
	}

#if defined(__ANDROID__)
	void loadFromFile(const std::string filename, uint32_t patchsize, glm::vec3 scale, Topology topology, AAssetManager* assetManager)
#else
	void loadFromFile(const std::string filename, uint32_t patchsize, glm::vec3 scale, Topology topology)
#endif
	{
		assert(device);
		assert(copyQueue != VK_NULL_HANDLE);

#if defined(__ANDROID__)
		AAsset* asset = AAssetManager_open(assetManager, filename.c_str(), AASSET_MODE_STREAMING);
		assert(asset);
		size_t size = AAsset_getLength(asset);
		assert(size > 0);
		void *textureData = malloc(size);
		AAsset_read(asset, textureData, size);
		AAsset_close(asset);
		gli::texture2d heightTex(gli::load((const char*)textureData, size));
		free(textureData);
#else
		gli::texture2d heightTex(gli::load(filename));
#endif
		dim = static_cast<uint32_t>(heightTex.extent().x);
		heightdata = new uint16_t[dim * dim];
		memcpy(heightdata, heightTex.data(), heightTex.size());
		this->scale = dim / patchsize;
		this->heightScale = scale.y;

		// Generate vertices

		Vertex * vertices = new Vertex[patchsize * patchsize * 4];

		const float wx = 2.0f;
		const float wy = 2.0f;

		for (uint32_t x = 0; x < patchsize; x++)
		{
			for (uint32_t y = 0; y < patchsize; y++)
			{
				uint32_t index = (x + y * patchsize);
				vertices[index].pos[0] = (x * wx + wx / 2.0f - (float)patchsize * wx / 2.0f) * scale.x;
				vertices[index].pos[1] = -getHeight(x, y);
				vertices[index].pos[2] = (y * wy + wy / 2.0f - (float)patchsize * wy / 2.0f) * scale.z;
				vertices[index].uv = glm::vec2((float)x / patchsize, (float)y / patchsize) * uvScale;
			}
		}

		for (uint32_t y = 0; y < patchsize; y++)
		{
			for (uint32_t x = 0; x < patchsize; x++)
			{
				float dx = getHeight(x < patchsize - 1 ? x + 1 : x, y) - getHeight(x > 0 ? x - 1 : x, y);
				if (x == 0 || x == patchsize - 1)
					dx *= 2.0f;

				float dy = getHeight(x, y < patchsize - 1 ? y + 1 : y) - getHeight(x, y > 0 ? y - 1 : y);
				if (y == 0 || y == patchsize - 1)
					dy *= 2.0f;

				glm::vec3 A = glm::vec3(1.0f, 0.0f, dx);
				glm::vec3 B = glm::vec3(0.0f, 1.0f, dy);

				glm::vec3 normal = (glm::normalize(glm::cross(A, B)) + 1.0f) * 0.5f;

				vertices[x + y * patchsize].normal = glm::vec3(normal.x, normal.z, normal.y);
			}
		}

		// Generate indices

		const uint32_t w = (patchsize - 1);
		uint32_t *indices;

		switch (topology)
		{
			// Indices for triangles
			case topologyTriangles:
			{
				indices = new uint32_t[w * w * 6];
				for (uint32_t x = 0; x < w; x++)
				{
					for (uint32_t y = 0; y < w; y++)
					{
						uint32_t index = (x + y * w) * 6;
						indices[index] = (x + y * patchsize);
						indices[index + 1] = indices[index] + patchsize;
						indices[index + 2] = indices[index + 1] + 1;

						indices[index + 3] = indices[index + 1] + 1;
						indices[index + 4] = indices[index] + 1;
						indices[index + 5] = indices[index];
					}
				}
				indexCount = (patchsize - 1) * (patchsize - 1) * 6;
				indexBufferSize = (w * w * 6) * sizeof(uint32_t);
				break;
			}
				// Indices for quad patches (tessellation)
			case topologyQuads:
			{

				indices = new uint32_t[w * w * 4];
				for (uint32_t x = 0; x < w; x++)
				{
					for (uint32_t y = 0; y < w; y++)
					{
						uint32_t index = (x + y * w) * 4;
						indices[index] = (x + y * patchsize);
						indices[index + 1] = indices[index] + patchsize;
						indices[index + 2] = indices[index + 1] + 1;
						indices[index + 3] = indices[index] + 1;
					}
				}
				indexCount = (patchsize - 1) * (patchsize - 1) * 4;
				indexBufferSize = (w * w * 4) * sizeof(uint32_t);
				break;
			}

		}

		assert(indexBufferSize > 0);

		vertexBufferSize = (patchsize * patchsize * 4) * sizeof(Vertex);

		// Generate Vulkan buffers

		VksBuffer vertexStaging, indexStaging;

		// Create staging buffers
		device->createBuffer(
				VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
				VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
				&vertexStaging,
				vertexBufferSize,
				vertices);

		device->createBuffer(
				VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
				VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
				&indexStaging,
				indexBufferSize,
				indices);

		// Device local (target) buffer
		device->createBuffer(
				VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
				VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
				&vertexBuffer,
				vertexBufferSize);

		device->createBuffer(
				VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
				VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
				&indexBuffer,
				indexBufferSize);

		// Copy from staging buffers
		VkCommandBuffer copyCmd = device->createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);

		VkBufferCopy copyRegion = {};

		copyRegion.size = vertexBufferSize;
		vkCmdCopyBuffer(
				copyCmd,
				vertexStaging.mBuffer,
				vertexBuffer.mBuffer,
				1,
				&copyRegion);

		copyRegion.size = indexBufferSize;
		vkCmdCopyBuffer(
				copyCmd,
				indexStaging.mBuffer,
				indexBuffer.mBuffer,
				1,
				&copyRegion);

		device->flushCommandBuffer(copyCmd, copyQueue, true);

		vkDestroyBuffer(device->logicalDevice, vertexStaging.mBuffer, nullptr);
		vkFreeMemory(device->logicalDevice, vertexStaging.mMemory, nullptr);
		vkDestroyBuffer(device->logicalDevice, indexStaging.mBuffer, nullptr);
		vkFreeMemory(device->logicalDevice, indexStaging.mMemory, nullptr);
	}
};

