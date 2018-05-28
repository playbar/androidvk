# Vulkan Tutorial 02 编写Vulkan应用程序框架原型

##General structure
在上一节中，我们创建了一个正确配置、可运行的的Vulkan应用程序，并使用测试代码进行了测试。本节中我们从头开始，<br>
使用如下代码构建一个基于GLFW的Vulkan应用程序原型框架的雏形。

<tab>
#include <vulkan/vulkan.h>
#include <iostream>
#include <stdexcept>
#include <functional>

class HelloTriangleApplication {
public:
    void run() {
        initVulkan();
        mainLoop();
        cleanup();
    }

private:
    void initVulkan() {}
    
    void mainLoop() { }
    
    void cleanup() { }
    
};

int main() {

    HelloTriangleApplication app;
    try {
        app.run();
    } catch (const std::runtime_error& e) {
        std::cerr << e.what() << std::endl;
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

</tab>

首先从LunarG SDK中添加Vulkan头文件，它提供了购机爱你Vulkan应用程序需要的函数、结构体、和枚举。
我们包含stdexcept和iostream头文件用于抛出异常信息，而functional头文件用于资源管理部分支持lambda表达式。


程序被封装到一个类中，该类结构将会存储Vulkan私有成员对象，并添加基本的函数来初始化他们。
首先会从initVulkan函数开始调用。当一切准备好，我们进入主循环开始渲染帧。我们将会加入mainLoop函数包含loop循环调用，
该循环调用直到GLFW窗体管理才会停止。当窗体关闭并且mainLoop返回时，我们需要释放我们已经申请过的任何资源，
该清理逻辑在cleanup函数中去定义。

程序运行期间，如果发生了任何严重的错误异常，我们会抛出std::runtime_error 并注明异常描述信息,
这个异常信息会被main函数捕获及打印提示。很快你将会遇到一个抛出error的例子，
是关于Vulkan应用程序不支持某个必要的扩展功能。


基本上在之后的每一个小节中都会从initVulkan函数中增加一个新的Vulkan函数调用,
增加的函数会产生Vulkan objects 并保存为类的私有成员，请记得在cleanup中进行资源的清理和释放。

## Resource management
我们知道通过malloc分配的每一个内存快在使用完之后都需要free内存资源，
每一个我们创建的Vulkan object不在使用时都需要明确的销毁。在C++中可以利用<memory> 完成 auto 资源管理，
但是在本节中，选择明确编写所有的内存的分配和释放操作，其主要原因是Vulkan的设计理念就是明确每一步操作，
清楚每一个对象的生命周期，避免可能存在的未知代码造成的异常。

当然在本节之后,我们可以通过重载std::shared_ptr来实现auto 资源管理。
对于更大体量的Vulkan程序,建议遵循RAII的原则维护资源的管理。


Vulkan对象可以直接使用vkCreateXXX系函数创建，也可以通过具有vkAllocateXXX等功能的一个对象进行分配。
确保每一个对象在不使用的时候调用vkDestroyXXX和vkFreeXXX销毁、释放对应的资源。
这些函数的参数通常因不同类型的对象而不同，但是他们共享一个参数:pAllocator。
这是一个可选的参数，Vulkan允许我们自定义内存分配器。我们将在本教程忽略此参数，始终以nullptr作为参数。

## Integrating GLFW

如果我们开发一些不需要基于屏幕显示的程序，那么纯粹的Vulkan本身可以完美的支持开发。
但是如果创建一些让人兴奋的可视化的内容，我们就需要引入窗体系统GLFW，
并将#include <vulkan/vulkan.h> 进行相应的替换。

<tab>
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
</tab>

在新版本的GLFW中已经提供了Vulkan相关的支持，详细的使用建议参阅官方资料。
通过替换，将会使用GLFW对Vulkan的支持，并自动加载Vulkan的头文件。
在run函数中添加一个initWindow函数调用，并确保在其他函数调用前优先调用。
我们将会通过该函数完成GLFW的窗体初始化工作。

void run() {
    initWindow();
    initVulkan();
    mainLoop();
    cleanup();
}

private:
    void initWindow() {

    }

initWindow中的第一个调用是glfwInit(),它会初始化GLFW库。因为最初GLFW是为OpenGL创建上下文，
所以在这里我们需要告诉它不要调用OpenGL相关的初始化操作。

glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
特别注意窗口大小的设置，稍后我们会调用，现在使用另一个窗口提示来仅用它。

glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
现在剩下的就是创建实际的窗体。添加一个GLFWwindow*窗体，私有类成员存储其引用并初始化窗体:

window = glfwCreateWindow(800, 600, "Vulkan", nullptr, nullptr);
前三个参数定义窗体的宽度、高度和Title。第四个参数允许制定一个监听器来打开窗体，
最后一个参数与OpenGL有关，我们选择nullptr。

使用常量代替硬编码宽度和高度，因为我们在后续的内容中会引用该数值多次。在HelloTriangleApplication类定义之上添加以下几行:

const int WIDTH = 800;
const int HEIGHT = 600;
并替换窗体创建的代码语句为:

window = glfwCreateWindow(WIDTH, HEIGHT, "Vulkan", nullptr, nullptr);
你现在应该有一个如下所示的initWindow函数:

<tab>
void initWindow() {
    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
    window = glfwCreateWindow(WIDTH, HEIGHT, "Vulkan", nullptr, nullptr);
}
</tab>

保持程序运行，直到发生错误或者窗体关闭，我们需要向mainLoop函数添加事件循环，如下所示:

void mainLoop() {
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
    }
}
这段代码应该很容易看懂。它循环并检查GLFW事件，直到按下X按钮，或者关闭窗体。该循环结构稍后会调用渲染函数。

一旦窗体关闭，我们需要通过cleanup函数清理资源、结束GLFW本身。

void cleanup() {
    glfwDestroyWindow(window);
    glfwTerminate();
}
运行程序，我们应该会看到一个名为Vulkan的白色窗体，直到关闭窗体终止应用程序。

ok，到现在我们已经完成了一个Vulkan程序的骨架原型，在下一小节我们会创建第一个Vulkan Object!

# Vulkan Tutorial 03 理解Instance

## Creating an instance

与Vulkan打交道，通常的步骤是创建一个intance去初始化Vulkan library。
这个instance是您的应用程序与Vulkan库之间的连接桥梁,通常创建过程中，
需要向驱动程序提供一些应用层的信息。

首先添加一个createInstance函数，并在initVulkan函数中调用。

void initVulkan() {
    createInstance();
}
另外添加一个类成员来保存instance句柄:

private:
VkInstance instance;
现在我们创建一个instance，并且为该数据结构赋予自定义应用程序的信息。
这些数据从技术角度是可选择的，但是它可以为驱动程序提供一些有用的信息来优化程序特殊的使用情景，
比如驱动程序使用一些图形引擎的特殊行为。这个数据结构称为VkApplicationInfo:


VkApplicationInfo appInfo = {};
appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
appInfo.pNext = nullptr;
appInfo.pApplicationName = "Hello Triangle";
appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
appInfo.pEngineName = "No Engine";
appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
appInfo.apiVersion = VK_API_VERSION_1_0;

如前所述，Vulkan中的许多数据结构要求在sType成员中明确的指定类型。
pNext成员可用于指向特定的扩展结构。
我们在这里使用默认初始化，将其设置为nullptr。

Vulkan中的大量信息通过结构体而不是函数参数传递，我们将填充一个结构体以提供足够的信息创建instance。
下一个结构体不是可选的，它需要告知Vulkan驱动程序我们需要使用哪些全局的 extensions 和 
validation layers。这里的全局意味着它适用于整个程序，而不是特定的设备，这些内容将在接下来的小节中说明。

VkInstanceCreateInfo createInfo = {};
createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
createInfo.pApplicationInfo = &appInfo;

前几个参数比较简单。接下来的两个指定需要的全局扩展，Vulakn对于平台特性是零API支持的(至少暂时这样)，
这意味着需要一个扩展才能与不同平台的窗体系统进行交互。GLFW有一个方便的内置函数，返回它有关的扩展信息，
我们可以传递给struct:


unsigned int glfwExtensionCount = 0;
const char** glfwExtensions;
glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
createInfo.enabledExtensionCount = glfwExtensionCount;
createInfo.ppEnabledExtensionNames = glfwExtensions;

结构体的最后两个成员确定需要开启的全局的validation layers。
我们将会在下一节中深入探讨这部分内容，在这一节设置为空。

createInfo.enabledLayerCount = 0;
我们现在已经指定了Vulkan创建一个实例需要的一切信息，调用vkCreateInstance创建属于我们的第一个instance:

VkResult result = vkCreateInstance(&createInfo, nullptr, &instance);
如你所见，Vulkan中创建、实例化相关的函数参数一般遵循如下原则定义:

使用有关creation info 的结构体指针
使用自定义分配器回调的指针
使用保存新对象句柄的指针
如果一切顺利，此刻instance的句柄应该存储在VkInstance类成员中了。
几乎所有的Vulkan函数都返回一个值为VK_SUCCESS或错误代码的VkResult类型的值。
要检查instance是否已经成功创建，我们不需要保存结果，仅仅使用 VK_SUCCESS 值来检测即可：

if (vkCreateInstance(&createInfo, nullptr, &instance) != VK_SUCCESS) {
    throw std::runtime_error("failed to create instance!");
}
现在运行程序，确认我们的instance创建成功。

## Checking for extension support

如果你查看vkCreateInstance的文档，你会看到一个可能出现的错误代码是VK_ERROR_EXTENSION_NOT_PRESENT。
我们可以简单地指定我们需要的扩展，如果该错误代码返回，则终止它们。这对于窗体系统或者诸如此类的扩展是有意义的，
那么如何检查可选功能呢？


在创建instance之前检索支持的扩展列表，通过vkEnumerateInstanceExtensionProperties函数。
它指向一个变量，该变量存储扩展数量和一个VkExtensionProperties数组来存储扩展的详细信息。
它也接受一个可选择的参数，允许我们通过特定的validation layers过滤扩展，现在我们暂时忽略这些。


要分配一个数组来保存扩展的详细信息，我们首先需要知道有多少个扩展存在。可以通过将后一个参数置空来获取扩展数量:

uint32_t extensionCount = 0;
vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);
现在我们分配一个集合去持有扩展的详细信息(include <vector>)

std::vector<VkExtensionProperties> extensions(extensionCount);
最后我们可以遍历扩展的详细信息:

vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, extensions.data());
每个VkExtensionProperties结构体包含扩展的名称和版本。我们可以用简单的for循环打印他们(\t是缩进)

std::cout << "available extensions:" << std::endl;

for (const auto& extension : extensions) {
    std::cout << "\t" << extension.extensionName << std::endl;
}
如果需要获取有关Vulkan支持的一些详细信息，可以将此代码添加到createInstance函数。
作为一个尝试，创建一个函数，检查glfwGetRequiredInstanceExtensions返回的所有扩展是否
都包含在受支持的扩展列表中。

##Cleaning up

在程序退出前，请正确销毁VkInstance。这部分可以定义在cleanup函数中，调用vkDestroyInstance函数完成。

void cleanup() {
    vkDestroyInstance(instance, nullptr);
    glfwDestroyWindow(window);
    glfwTerminate();
}

vkDestroyInstance函数的参数很简单。像之前小节提到的，Vulkan中的分配和释放功能有一个可选的分配器回调，
我们通过将nullptr设置忽略。后续小节中创建的所有Vulkan相关资源，集中在cleanup函数中进行清理，
且确保在销毁instance之前销毁。

在进行更复杂的内容之前，是时候了解validation layers了。

# Vulkan Tutorial 04 理解Validation layers

## What are validation layers?
Vulkan API的设计核心是尽量最小化驱动程序的额外开销，所谓额外开销更多的是指向渲染以外的运算。
其中一个具体的表现就是默认条件下，Vulkan API的错误检查的支持非常有限。
即使遍历不正确的值或者将需要的参数传递为空指针，也不会有明确的处理逻辑，并且直接导致崩溃或者未定义的异常行为。
之所以这样，是因为Vulkan要求每一个步骤定义都非常明确，导致很容易造成小错误，例如使用新的GPU功能，
但是忘记了逻辑设备创建时请求它。

 
但是，这并不意味着这些检查不能添加到具体的API中。Vulkan推出了一个优化的系统，
这个系统称之为Validation layers。Validation layers是可选组件，
可以挂载到Vulkan函数中调用，以回调其他的操作。Validation layers的常见操作情景有:

1. 根据规范检查参数数值，最终确认是否存与预期不符的情况
2. 跟踪对象的创建和销毁，以查找是否存在资源的泄漏
3. 跟踪线程的调用链，确认线程执行过程中的安全性
4. 将每次函数调用所使用的参数记录到标准的输出中，进行初步的Vulkan概要分析

以下示例代码是一个函数中应用Validation layers的具体实现:


VkResult vkCreateInstance(
    const VkInstanceCreateInfo* pCreateInfo,
    const VkAllocationCallbacks* pAllocator,
    VkInstance* instance) {

    if (pCreateInfo == nullptr || instance == nullptr) {
        log("Null pointer passed to required parameter!");
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    return real_vkCreateInstance(pCreateInfo, pAllocator, instance);
}

这些Validation layers可以随意的堆叠到Vulkan驱动程序中，如果有必要，你甚至可以包含所有的debug功能。
可以简单的开启Validation layers的debug版本，并在release版本中完全禁止，从而为您提供理想的两个版本。

Vulkan没有内置任何Validation layers，但是LunarG Vulkan SDK提供了一系列layers用于检测常规的错误异常。
他们是完全OpenSource的，所以你可以根据你需要的检测需求应用具体的Validation layers。
使用Validation layers是最佳的方式避免你的应用程序在发生未知的行为时收到影响，甚至中断。

Vulkan只能使用已经安装到系统上下文的Validation layers。
例如，LunarG Validation layers仅在安装了Vulkan SDK的PC上可用。


在之前的Vulkan版本中有两种不同类型的Validation layers，分别应用于 instance 和 device specific。
这个设计理念希望instance层只会验证与全局Vulkan对象(例如Instance)有关的调用，
而device specific层只是验证与特定GPU相关的调用。device specific层已经被废弃，
这意味着instance层的Validation layers将应用所有的Vulkan调用。
出于兼容性的考虑，规范文档仍然建议在device specific层开启Validation layers，
这在某些情景下是有必要的。我们将在logic device层指定与instance相同的Validation layers,稍后会看到。

## Using validation layers
在本节中，我们将介绍如何启用Vulkan SDK提供的标准诊断层。就像扩展一样，需要通过指定具体名称来开启validation layers。
SDK通过请求VK_LAYER_LUNARG_standard_validaction层，来隐式的开启有所关于诊断layers，
从而避免明确的指定所有的明确的诊断层。

 
首先在程序中添加两个配置变量来指定要启用的layers以及是否开启它们。
我们选择基于程序是否在调试模式下进行编译。NDEBUG是C++标准宏定义，代表“不调试”。


const int WIDTH = 800;
const int HEIGHT = 600;

const std::vector<const char*> validationLayers = {
    "VK_LAYER_LUNARG_standard_validation"
};
<table>
#ifdef NDEBUG
    const bool enableValidationLayers = false;
#else
    const bool enableValidationLayers = true;
#endif
</table>

我们将添加一个新的函数checkValidationLayerSupport,检测所有请求的layers是否可用。
首先使用vkEnumerateInstanceLayerProperties函数列出所有可用的层。
其用法与vkEnumerateInstanceExtensionProperties相同，在Instance小节中讨论过。


bool checkValidationLayerSupport() {
    uint32_t layerCount;
    vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

    std::vector<VkLayerProperties> availableLayers(layerCount);
    vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

    return false;
}

接下来检查validationLayers中的所有layer是否存在于availableLayers列表中。
我们需要使用strcmp引入<cstring>。


for (const char* layerName : validationLayers) {
    bool layerFound = false;

    for (const auto& layerProperties : availableLayers) {
        if (strcmp(layerName, layerProperties.layerName) == 0) {
            layerFound = true;
            break;
        }
    }

    if (!layerFound) {
        return false;
    }
}
return true;


现在我们在createInstance函数中使用:


void createInstance() {
    if (enableValidationLayers && !checkValidationLayerSupport()) {
        throw std::runtime_error("validation layers requested, but not available!");
    }

    ...
}


现在以调试模式运行程序，并确保不会发生错误。如果发生错误，请确保正确安装Vulkan SDK。
如果没有或者几乎没有layers上报，建议使用最新的SDK，或者到LunarG官方寻求帮助,需要注册帐号。

 
最终，修改VkInstanceCreateInfo结构体，填充当前上下文已经开启的validation layers名称集合。

if (enableValidationLayers) {
    createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
    createInfo.ppEnabledLayerNames = validationLayers.data();
} else {
    createInfo.enabledLayerCount = 0;
}
如果检查成功，vkCreateInstance不会返回VK_ERROR_LAYER_NOT_PRESENT错误，请确保程序运行正确无误。

## Message callback

比较遗憾的是单纯开启validation layers是没有任何帮助的，因为到现在没有任何途径将诊断信息回传给应用程序。
要接受消息，我们必须设置回调，需要VK_EXT_debug_report扩展。

我们新增一个getRequiredExtensions函数，该函数将基于是否开启validation layers返回需要的扩展列表。

std::vector<const char*> getRequiredExtensions() {
    std::vector<const char*> extensions;

    unsigned int glfwExtensionCount = 0;
    const char** glfwExtensions;
    glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

    for (unsigned int i = 0; i < glfwExtensionCount; i++) {
        extensions.push_back(glfwExtensions[i]);
    }

    if (enableValidationLayers) {
        extensions.push_back(VK_EXT_DEBUG_REPORT_EXTENSION_NAME);
    }

    return extensions;
}

GLFW的扩展总是需要的，而debug report扩展是根据编译条件添加。
与此同时我们使用VK_EXT_DEBUG_REPORT_EXTENSION_NAME宏定义，
它等价字面值 "VK_EXT_debug_report"，使用宏定义避免了硬编码。


我们在createInstance函数中调用:

auto extensions = getRequiredExtensions();
createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
createInfo.ppEnabledExtensionNames = extensions.data();
运行程序确保没有收到VK_ERROR_EXTENSION_NOT_PRESENT错误信息，我们不需要去验证扩展是否存在，
因为它会被有效的validation layers引擎的验证。

 

现在让我们看一下callback函数的样子，添加一个静态函数debugCallback,
并使用PFN_vkDebugReportCallbackEXT 原型进行修饰。VKAPI_ATTR和VKAPI_CALL确保了正确的函数签名，
从而被Vulkan调用。


static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
    VkDebugReportFlagsEXT flags,
    VkDebugReportObjectTypeEXT objType,
    uint64_t obj,
    size_t location,
    int32_t code,
    const char* layerPrefix,
    const char* msg,
    void* userData) {

    std::cerr << "validation layer: " << msg << std::endl;

    return VK_FALSE;
}

函数的第一个参数指定了消息的类型，它可以通过一下任意标志位组合:

* VK_DEBUG_REPORT_INFORMATION_BIT_EXT
* VK_DEBUG_REPORT_WARNING_BIT_EXT
* VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT
* VK_DEBUG_REPORT_ERROR_BIT_EXT
* VK_DEBUG_REPORT_DEBUG_BIT_EXT
objType参数描述作为消息主题的对象的类型，比如一个obj是VkPhysicalDevice，
那么objType就是VK_DEBUG_REPORT_OBJECT_TYPE_DEVICE_EXT。
这样做被允许是因为Vulkan的内部句柄都被定义为uint64_t。msg参数包含指向消息的指针。
最后，有一个userData参数可将自定义的数据进行回调。

回调返回一个布尔值，表明触发validation layer消息的Vulkan调用是否应被中止。
如果返回true，则调用将以VK_ERROR_VALIDATION_FAILED_EXT错误中止。
这通常用于测试validation layers本身，所以我们总是返回VK_FALSE。

现在需要告知Vulkan关于定义的回调函数。也许你会比较惊讶，即使是debug 
回调也需要一个明确的创建和销毁句柄的管理工作。添加一个类成员存储回调句柄，在instance下。

VkDebugReportCallbackEXT callback;
现在添加一个函数setupDebugCallback,该函数会在initVulkan函数 调用createInstance之后调用。

void initVulkan() {
    createInstance();
    setupDebugCallback();
}

void setupDebugCallback() {
    if (!enableValidationLayers) return;

}

现在我们填充有关回调的结构体详细信息:

VkDebugReportCallbackCreateInfoEXT createInfo = {};
createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_REPORT_CALLBACK_CREATE_INFO_EXT;
createInfo.flags = VK_DEBUG_REPORT_ERROR_BIT_EXT | VK_DEBUG_REPORT_WARNING_BIT_EXT;
createInfo.pfnCallback = debugCallback;
标志位允许过滤掉你不希望的消息。pfnCallback字段描述了回调函数的指针。
在这里可以有选择的传递一个pUserData指针，最为回调的自定义数据结构使用，
比如可以传递HelloTriangleApplication类的指针。


该结构体应该传递给vkCreateDebugReportCallbackEXT函数创建VkDebugReportCallbackEXT对象。
不幸的是，因为这个功能是一个扩展功能，它不会被自动加载。所以必须使用vkGetInstanceProcAddr查找函数地址。
我们将在后台创建代理函数。在HelloTriangleApplication类定义之上添加它。


VkResult CreateDebugReportCallbackEXT(VkInstance instance, 
        const VkDebugReportCallbackCreateInfoEXT* pCreateInfo, 
        const VkAllocationCallbacks* pAllocator, 
        VkDebugReportCallbackEXT* pCallback) {
    auto func = (PFN_vkCreateDebugReportCallbackEXT) vkGetInstanceProcAddr(instance, "vkCreateDebugReportCallbackEXT");
    if (func != nullptr) {
        return func(instance, pCreateInfo, pAllocator, pCallback);
    } else {
        return VK_ERROR_EXTENSION_NOT_PRESENT;
    }
}

如果函数无法加载，则vkGetInstanceProcAddr函数返回nullptr。如果非nullptr，就可以调用此函数来创建扩展对象:

if (CreateDebugReportCallbackEXT(instance, &createInfo, nullptr, &callback) != VK_SUCCESS) {
    throw std::runtime_error("failed to set up debug callback!");
}
倒数第二个参数仍然是分配器回调指针，我们仍然设置为nullptr。debug回调与Vulkan instance和layers相对应，
所以需要明确指定第一个参数。现在运行程序，关闭窗口，你会在命令行看到提示信息:

validation layer: Debug Report callbacks not removed before DestroyInstance

现在Vulkan已经在程序中发现了一个错误!需要通过调用vkDestroyDebugReportCallbackEXT
清理VkDebugReportCallbackEXT对象。与vkCreateDebugReportCallbackEXT类似，
该函数需要显性的加载。在CreateDebugReportCallbackEXT下创建另一个代理函数。

void DestroyDebugReportCallbackEXT(VkInstance instance, 
                  VkDebugReportCallbackEXT callback, 
                  const VkAllocationCallbacks* pAllocator) {
    auto func = (PFN_vkDestroyDebugReportCallbackEXT) vkGetInstanceProcAddr(instance, "vkDestroyDebugReportCallbackEXT");
    if (func != nullptr) {
        func(instance, callback, pAllocator);
    }
}
该函数定义为类静态函数或者外部函数，我们在cleanup函数中进行调用:

void cleanup() {
    DestroyDebugReportCallbackEXT(instance, callback, nullptr);
    vkDestroyInstance(instance, nullptr);

    glfwDestroyWindow(window);

    glfwTerminate();
}

再次运行程序，会看到错误信息已经消失。如果要查看哪个调用触发了一条消息，可以向消息回调添加断点，
并查看堆栈调用链。

## Configuration
Validation layers的行为可以有更多的设置，不仅仅是VkDebugReportCallbackCreateInfoEXT结构中指定的标志位信息。
浏览Vulkan SDK的Config目录。找到vk_layer_settings.txt文件，里面有说明如何配置layers。

 

要为自己的应用程序配置layers，请将文件赋值到项目的Debug和Release目录，然后按照说明设置需要的功能特性。除此之外，本教程将使用默认的设置。

现在是时候进入系统的Vulkan devices小节了。

