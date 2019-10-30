# Vulkan Tutorial 07 Window surface

到目前为止，我们了解到Vulkan是一个与平台特性无关联的API集合。它不能直接与窗口系统进行交互。
为了将渲染结果呈现到屏幕，需要建立Vulkan与窗体系统之间的连接，
我们需要使用WSI(窗体系统集成)扩展。在本小节中，我们将讨论第一个，即VK_KHR_surface。
它暴露了VkSurfaceKHR，它代表surface的一个抽象类型，用以呈现渲染图像使用。
我们程序中将要使用到的surface是由我们已经引入的GLFW扩展及其打开的相关窗体支持的。
简单来说surface就是Vulkan与窗体系统的连接桥梁。

VK_KHR_surface扩展是一个instance级扩展，我们目前为止已经启用过它，
它包含在glfwGetRequiredInstanceExtensions返回的列表中。
该列表还包括将在接下来几小节中使用的一些其他WSI扩展。

需要在instance创建之后立即创建窗体surface，因为它会影响物理设备的选择。
之所以在本小节将surface创建逻辑纳入讨论范围，是因为窗体surface对于渲染、呈现方式是一个比较大的课题，
如果过早的在创建物理设备加入这部分内容，会混淆基本的物理设备设置工作。
另外窗体surface本身对于Vulkan也是非强制的。Vulkan允许这样做，不需要同OpenGL一样必须要创建窗体surface。

## Window surface creation
现在开始着手创建窗体surface，在类成员debugCallback下加入成员变量surface。

VkSurfaceKHR surface;
虽然VkSurfaceKHR对象及其用法与平台无关联，但创建过程需要依赖具体的窗体系统的细节。
比如，在Windows平台中，它需要WIndows上的HWND和HMODULE句柄。因此针对特定平台提供相应的扩展，
在Windows上为VK_KHR_win32_surface，它自动包含在glfwGetRequiredInstanceExtensions列表中。

我们将会演示如何使用特定平台的扩展来创建Windows上的surface桥，但是不会在教程中实际使用它。
使用GLFW这样的库避免了编写没有任何意义的跨平台相关代码。GLFW实际上通过glfwCreateWindowSurface
很好的处理了平台差异性。当然了，比较理想是在依赖它们帮助我们完成具体工作之前，了解一下背后的实现是有帮助的。

因为一个窗体surface是一个Vulkan对象，它需要填充VkWin32SurfaceCreateInfoKHR结构体，
这里有两个比较重要的参数:hwnd和hinstance。如果熟悉windows下开发应该知道，这些是窗口和进程的句柄。

<pre>
VkWin32SurfaceCreateInfoKHR createInfo;
createInfo.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
createInfo.hwnd = glfwGetWin32Window(window);
createInfo.hinstance = GetModuleHandle(nullptr);
</pre>

glfwGetWin32Window函数用于从GLFW窗体对象获取原始的HWND。
GetModuleHandle函数返回当前进程的HINSTANCE句柄。

填充完结构体之后，可以利用vkCreateWin32SurfaceKHR创建surface桥，和之前获取创建、
销毁DebugReportCallEXT一样，这里同样需要通过instance获取创建surface用到的函数。
这里涉及到的参数分别为instance, surface创建的信息，自定义分配器和最终保存surface的句柄变量。

<pre>
auto CreateWin32SurfaceKHR = (PFN_vkCreateWin32SurfaceKHR) vkGetInstanceProcAddr(instance, "vkCreateWin32SurfaceKHR");

if (!CreateWin32SurfaceKHR || CreateWin32SurfaceKHR(instance, &createInfo, nullptr, &surface) != VK_SUCCESS) {
    throw std::runtime_error("failed to create window surface!");
}
</pre>

该过程与其他平台类似，比如Linux，使用X11界面窗体系统，可以通过vkCreateXcbSurfaceKHR函数建立连接。

glfwCreateWindowSurface函数根据不同平台的差异性，在实现细节上会有所不同。
我们现在将其整合到我们的程序中。从initVulkan中添加一个函数createSurface,安排在createInstnace和setupDebugCallback函数之后。

<pre>
void initVulkan() {
    createInstance();
    setupDebugCallback();
    createSurface();
    pickPhysicalDevice();
    createLogicalDevice();
}

void createSurface() {

}
</pre>

GLFW没有使用结构体，而是选择非常直接的参数传递来调用函数。

<pre>
void createSurface() {
    if (glfwCreateWindowSurface(instance, window, nullptr, &surface) != VK_SUCCESS) {
        throw std::runtime_error("failed to create window surface!");
    }
}
</pre>

参数是VkInstance,GLFW窗体的指针，自定义分配器和用于存储VkSurfaceKHR变量的指针。对于不同平台统一返回VkResult。
GLFW没有提供专用的函数销毁surface,但是可以简单的通过Vulkan原始的API完成:

<pre>
void cleanup() {
        ...
        vkDestroySurfaceKHR(instance, surface, nullptr);
        vkDestroyInstance(instance, nullptr);
        ...
    }
</pre>
    
最后请确保surface的清理是在instance销毁之前完成。

## Querying for presentation support
虽然Vulkan的实现支持窗体集成功能，但是并不意味着系统中的每一个物理设备都支持它。
因此，我们需要扩展isDeviceSuitable函数，确保设备可以将图像呈现到我们创建的surface。
由于presentation是一个队列的特性功能，因此解决问题的方法就是找到支持presentation的队列簇，
最终获取队列满足surface创建的需要。

实际情况是，支持graphics命令的的队列簇和支持presentation命令的队列簇可能不是同一个簇。
因此，我们需要修改QueueFamilyIndices结构体，以支持差异化的存储。

<pre>
struct QueueFamilyIndices {
    int graphicsFamily = -1;
    int presentFamily = -1;

    bool isComplete() {
        return graphicsFamily >= 0 && presentFamily >= 0;
    }
};
</pre>

接下来，我们修改findQueueFamilies函数来查找具备presentation功能的队列簇。
函数中用于检查的核心代码是vkGetPhysicalDeviceSurfaceSupportKHR,它将物理设备、队列簇索引和surface作为参数。
在VK_QUEUE_GRAPHICS_BIT相同的循环体中添加函数的调用:

<pre>
VkBool32 presentSupport = false;
vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &presentSupport);
</pre>

然后之需要检查布尔值并存储presentation队列簇的索引:

<pre>
if (queueFamily.queueCount > 0 && presentSupport) {
    indices.presentFamily = i;
}
</pre>

需要注意的是，为了支持graphics和presentation功能，我们实际环境中得到的可能是同一个队列簇，也可能不同，
为此在我们的程序数据结构及选择逻辑中，将按照均来自不同的队列簇分别处理，这样便可以统一处理以上两种情况。
除此之外，出于性能的考虑，我们也可以通过添加逻辑明确的指定物理设备所使用的graphics和presentation功能来自同一个队列簇。

 ![Image](pic/7_1.png)
 
 
## Creating the presentation queue
 剩下的事情是修改逻辑设备创建过程，在于创建presentation队列并获取VkQueue的句柄。添加保存队列句柄的成员变量:

VkQueue presentQueue;
接下来，我们需要多个VkDeviceQueueCreateInfo结构来创建不同功能的队列。一个优雅的方式是针对不同功能的队列簇创建一个set集合确保队列簇的唯一性:


<pre>
#include <set>
...
QueueFamilyIndices indices = findQueueFamilies(physicalDevice);

std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
std::set<int> uniqueQueueFamilies = {indices.graphicsFamily, indices.presentFamily};

float queuePriority = 1.0f;
for (int queueFamily : uniqueQueueFamilies) {
    VkDeviceQueueCreateInfo queueCreateInfo = {};
    queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queueCreateInfo.queueFamilyIndex = queueFamily;
    queueCreateInfo.queueCount = 1;
    queueCreateInfo.pQueuePriorities = &queuePriority;
    queueCreateInfos.push_back(queueCreateInfo);
}
</pre>

同时还要修改VkDeviceCreateInfo指向队列集合:

createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
createInfo.pQueueCreateInfos = queueCreateInfos.data();
如果队列簇相同，那么我们之需要传递一次索引。最后，添加一个调用检索队列句柄:

vkGetDeviceQueue(device, indices.presentFamily, 0, &presentQueue);
在这个例子中，队列簇是相同的，两个句柄可能会有相同的值。在下一个章节中我们会看看交换链，以及它们如何使我们能够将图像呈现给surface。

[代码](src/07.cpp)。

