# Vulkan Tutorial 21 Staging buffer

## Introduction
顶点缓冲区现在已经可以正常工作，但相比于显卡内部读取数据，单纯从CPU访问内存数据的方式性能不是最佳的。
最佳的方式是采用VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT标志位，通常来说用在专用的图形卡，CPU是无法访问的。
在本章节我们创建两个顶点缓冲区。一个缓冲区提供给CPU-HOST内存访问使用，用于从顶点数组中提交数据，
另一个顶点缓冲区用于设备local内存。我们将会使用缓冲区拷贝的命令将数据从暂存缓冲区拷贝到实际的图形卡内存中。

## Transfer queue
缓冲区拷贝的命令需要队列簇支持传输操作，可以通过VK_QUEUE_TRANSFER_BIT标志位指定。
好消息是任何支持VK_QUEUE_GRAPHICS_BIT 或者 VK_QUEUE_COMPUTE_BIT标志位功能的队列簇都
默认支持VK_QUEUE_TRANSFER_BIT操作。这部分的实现不需要在queueFlags显示的列出。

如果需要明确化，甚至可以尝试为不同的队列簇指定具体的传输操作。这部分实现需要对代码做出如下修改：

* 修改QueueFamilyIndices和findQueueFamilies，明确指定队列簇需要具备VK_QUEUE_TRANSFER标志位，而不是VK_QUEUE_GRAPHICS_BIT。
* 修改createLogicalDevice函数，请求一个传输队列句柄。
* 创建两个命令对象池分配命令缓冲区，用于向传输队列簇提交命令。
* 修改资源的sharingMode为VK_SHARING_MODE_CONCURRENT，并指定为graphics和transfer队列簇。
* 提交任何传输命令，诸如vkCmdCopyBuffer(本章节使用)到传输队列，而不是图形队列。
* 需要一些额外的工作，但是它我们更清楚的了解资源在不同队列簇如何共享的。

## Abstracting buffer creation
考虑到我们在本章节需要创建多个缓冲区，比较理想的是创建辅助函数来完成。
新增函数createBuffer并将createVertexBuffer中的部分代码(不包括映射)移入该函数。 

<pre>
void createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory) {
    VkBufferCreateInfo bufferInfo = {};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateBuffer(device, &bufferInfo, nullptr, &buffer) != VK_SUCCESS) {
        throw std::runtime_error("failed to create buffer!");
    }

    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(device, buffer, &memRequirements);

    VkMemoryAllocateInfo allocInfo = {};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, properties);

    if (vkAllocateMemory(device, &allocInfo, nullptr, &bufferMemory) != VK_SUCCESS) {
        throw std::runtime_error("failed to allocate buffer memory!");
    }

    vkBindBufferMemory(device, buffer, bufferMemory, 0);
}
</pre>

该函数需要传递缓冲区大小，内存属性和usage最终创建不同类型的缓冲区。最后两个参数保存输出的句柄。

我们可以从createVertexBuffer函数中移除创建缓冲区和分配内存的代码，并使用createBuffer替代：

<pre>
void createVertexBuffer() {
    VkDeviceSize bufferSize = sizeof(vertices[0]) * vertices.size();
    createBuffer(bufferSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, vertexBuffer, vertexBufferMemory);

    void* data;
    vkMapMemory(device, vertexBufferMemory, 0, bufferSize, 0, &data);
    memcpy(data, vertices.data(), (size_t) bufferSize);
    vkUnmapMemory(device, vertexBufferMemory);
}
</pre>

运行程序确保顶点缓冲区仍然正常工作。

## Using a staging buffer
我们现在改变createVertexBuffer函数，仅仅使用host缓冲区作为临时缓冲区，并且使用device缓冲区作为最终的顶点缓冲区。

<pre>
void createVertexBuffer() {
    VkDeviceSize bufferSize = sizeof(vertices[0]) * vertices.size();

    VkBuffer stagingBuffer;
    VkDeviceMemory stagingBufferMemory;
    createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory);

    void* data;
    vkMapMemory(device, stagingBufferMemory, 0, bufferSize, 0, &data);
    memcpy(data, vertices.data(), (size_t) bufferSize);
    vkUnmapMemory(device, stagingBufferMemory);

    createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, vertexBuffer, vertexBufferMemory);
}
</pre>


我们使用stagingBuffer来划分stagingBufferMemory缓冲区用来映射、拷贝顶点数据。在本章节我们使用两个新的缓冲区usage标致类型：

* VK_BUFFER_USAGE_TRANSFER_SRC_BIT：缓冲区可以用于源内存传输操作。
* VK_BUFFER_USAGE_TRANSFER_DST_BIT：缓冲区可以用于目标内存传输操作。

vertexBuffer现在使用device类型作为分配的内存类型，意味着我们不可以使用vkMapMemory内存映射。
然而我们可以从stagingBuffer向vertexBuffer拷贝数据。我们需要指定stagingBuffer的传输源标志位，
还要为顶点缓冲区vertexBuffer的usage设置传输目标的标志位。

我们新增函数copyBuffer，用于从一个缓冲区拷贝数据到另一个缓冲区。

<pre>
void copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size) {

}
</pre>

使用命令缓冲区执行内存传输的操作命令，就像绘制命令一样。因此我们需要分配一个临时命令缓冲区。
或许在这里希望为短期的缓冲区分别创建command pool，那么可以考虑内存分配的优化策略，
在command pool生成期间使用VK_COMMAND_POOL_CREATE_TRANSIENT_BIT标志位。

<pre>
void copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size) {
    VkCommandBufferAllocateInfo allocInfo = {};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool = commandPool;
    allocInfo.commandBufferCount = 1;

    VkCommandBuffer commandBuffer;
    vkAllocateCommandBuffers(device, &allocInfo, &commandBuffer);
}
</pre>

立即使用命令缓冲过去进行记录：

<pre>
VkCommandBufferBeginInfo beginInfo = {};
beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
</pre>

vkBeginCommandBuffer(commandBuffer, &beginInfo);v
应用于绘制命令缓冲区的VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT标志位在此不必要，
因为我们之需要使用一次命令缓冲区，等待该函数返回，直到复制操作完成。
告知driver驱动程序使用VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT是一个好的习惯。

<pre>
VkBufferCopy copyRegion = {};
copyRegion.srcOffset = 0; // Optional
copyRegion.dstOffset = 0; // Optional
copyRegion.size = size;
vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &copyRegion);
</pre>

缓冲区内容使用vkCmdCopyBuffer命令传输。它使用source和destination缓冲区及一个缓冲区拷贝的区域作为参数。
这个区域被定义在VkBufferCopy结构体中，描述源缓冲区的偏移量，目标缓冲区的偏移量和对应的大小。
与vkMapMemory命令不同，这里不可以指定VK_WHOLE_SIZE。

vkEndCommandBuffer(commandBuffer);  
此命令缓冲区仅包含拷贝命令，因此我们可以在此之后停止记录。现在执行命令缓冲区完成传输：

<pre>
VkSubmitInfo submitInfo = {};
submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
submitInfo.commandBufferCount = 1;
submitInfo.pCommandBuffers = &commandBuffer;

vkQueueSubmit(graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
vkQueueWaitIdle(graphicsQueue);
</pre>

与绘制命令不同的是，这个时候我们不需要等待任何事件。我们只是想立即在缓冲区执行传输命令。
这里有同样有两个方式等待传输命令完成。我们可以使用vkWaitForFences等待栅栏fence，
或者只是使用vkQueueWaitIdle等待传输队列状态变为idle。一个栅栏允许安排多个连续的传输操作，而不是一次执行一个。
这给了驱动程序更多的优化空间。

vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);  
不要忘记清理用于传输命令的命令缓冲区。

我们可以从createVertexBuffer函数中调用copyBuffer，拷贝顶点数据到设备缓冲区中：

createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, vertexBuffer, vertexBufferMemory);

copyBuffer(stagingBuffer, vertexBuffer, bufferSize)  
当从暂存缓冲区拷贝数据到图形卡设备缓冲区完毕后，我们应该清理它：

<pre>
 ...

    copyBuffer(stagingBuffer, vertexBuffer, bufferSize);

    vkDestroyBuffer(device, stagingBuffer, nullptr);
    vkFreeMemory(device, stagingBufferMemory, nullptr);
}
</pre>

运行程序确认三角形绘制正常。性能的提升也许现在不能很好的显现出来，但其顶点数据已经是从高性能的显存中加载。
当我们开始渲染更复杂的几何图形时，这个技术是非常重要。

##  Conclusion
需要了解的是，在真实的生产环境中的应用程序里，不建议为每个缓冲区调用vkAllocateMemory分配内存。
内存分配的最大数量受到maxMemoryAllocationCount物理设备所限，即使像NVIDIA GTX1080这样的高端硬件上，
也只能提供4096的大小。同一时间，为大量对象分配内存的正确方法是创建一个自定义分配器，
通过使用我们在许多函数中用到的偏移量offset，将一个大块的可分配内存区域划分为多个可分配内存块，提供缓冲区使用。

也可以自己实现一个灵活的内存分配器，或者使用GOUOpen提供的VulkanMemoryAllocator库。
然而，对于本教程，我们可以做到为每个资源使用单独的分配，因为我们不会触达任何资源限制条件。 

[代码](src/21.cpp)。

