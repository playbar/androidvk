# Vulkan Tutorial 02 编写Vulkan应用程序框架原型

## General structure
在上一节中，我们创建了一个正确配置、可运行的的Vulkan应用程序，并使用测试代码进行了测试。
本节中我们从头开始，使用如下代码构建一个基于GLFW的Vulkan应用程序原型框架的雏形。

<pre>
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
    void initVulkan() {

    }

    void mainLoop() {

    }

    void cleanup() {

    }
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
</pre>

首先从LunarG SDK中添加Vulkan头文件，它提供了购机爱你Vulkan应用程序需要的函数、结构体、和枚举。
我们包含stdexcept和iostream头文件用于抛出异常信息，而functional头文件用于资源管理部分支持lambda表达式。

程序被封装到一个类中，该类结构将会存储Vulkan私有成员对象，并添加基本的函数来初始化他们。
首先会从initVulkan函数开始调用。当一切准备好，我们进入主循环开始渲染帧。我们将会加入mainLoop函数包含loop循环调用，
该循环调用直到GLFW窗体管理才会停止。当窗体关闭并且mainLoop返回时，我们需要释放我们已经申请过的任何资源，该清理逻辑在cleanup函数中去定义。

程序运行期间，如果发生了任何严重的错误异常，我们会抛出std::runtime_error 并注明异常描述信息,
这个异常信息会被main函数捕获及打印提示。很快你将会遇到一个抛出error的例子，是关于Vulkan应用程序不支持某个必要的扩展功能。

基本上在之后的每一个小节中都会从initVulkan函数中增加一个新的Vulkan函数调用,
增加的函数会产生Vulkan objects 并保存为类的私有成员，请记得在cleanup中进行资源的清理和释放。

## Resource management
我们知道通过malloc分配的每一个内存快在使用完之后都需要free内存资源，
每一个我们创建的Vulkan object不在使用时都需要明确的销毁。在C++中可以利用<memory> 完成 auto 资源管理，
但是在本节中，选择明确编写所有的内存的分配和释放操作，其主要原因是Vulkan的设计理念就是明确每一步操作，
清楚每一个对象的生命周期，避免可能存在的未知代码造成的异常。

当然在本节之后,我们可以通过重载std::shared_ptr来实现auto 资源管理。对于更大体量的Vulkan程序,建议遵循RAII的原则维护资源的管理。

Vulkan对象可以直接使用vkCreateXXX系函数创建，也可以通过具有vkAllocateXXX等功能的一个对象进行分配。
确保每一个对象在不使用的时候调用vkDestroyXXX和vkFreeXXX销毁、释放对应的资源。
这些函数的参数通常因不同类型的对象而不同，但是他们共享一个参数:pAllocator。这是一个可选的参数，Vulkan允许我们自定义内存分配器。
我们将在本教程忽略此参数，始终以nullptr作为参数。

## Integrating GLFW
如果我们开发一些不需要基于屏幕显示的程序，那么纯粹的Vulkan本身可以完美的支持开发。
但是如果创建一些让人兴奋的可视化的内容，我们就需要引入窗体系统GLFW，并将#include <vulkan/vulkan.h> 进行相应的替换。

<pre>
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
</pre>

在新版本的GLFW中已经提供了Vulkan相关的支持，详细的使用建议参阅官方资料。

通过替换，将会使用GLFW对Vulkan的支持，并自动加载Vulkan的头文件。在run函数中添加一个initWindow函数调用，
并确保在其他函数调用前优先调用。我们将会通过该函数完成GLFW的窗体初始化工作。

<pre>
void run() {
    initWindow();
    initVulkan();
    mainLoop();
    cleanup();
}

private:
    void initWindow() {

    }
</pre>

initWindow中的第一个调用是glfwInit(),它会初始化GLFW库。因为最初GLFW是为OpenGL创建上下文，
所以在这里我们需要告诉它不要调用OpenGL相关的初始化操作。

glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
特别注意窗口大小的设置，稍后我们会调用，现在使用另一个窗口提示来仅用它。

glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
现在剩下的就是创建实际的窗体。添加一个GLFWwindow*窗体，私有类成员存储其引用并初始化窗体:

window = glfwCreateWindow(800, 600, "Vulkan", nullptr, nullptr);
前三个参数定义窗体的宽度、高度和Title。第四个参数允许制定一个监听器来打开窗体，最后一个参数与OpenGL有关，我们选择nullptr。

使用常量代替硬编码宽度和高度，因为我们在后续的内容中会引用该数值多次。在HelloTriangleApplication类定义之上添加以下几行:

<pre>
const int WIDTH = 800;
const int HEIGHT = 600;
</pre>

并替换窗体创建的代码语句为: 

window = glfwCreateWindow(WIDTH, HEIGHT, "Vulkan", nullptr, nullptr);

你现在应该有一个如下所示的initWindow函数:

<pre>
void initWindow() {
    glfwInit();

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

    window = glfwCreateWindow(WIDTH, HEIGHT, "Vulkan", nullptr, nullptr);
}
</pre>

保持程序运行，直到发生错误或者窗体关闭，我们需要向mainLoop函数添加事件循环，如下所示:

<pre>
void mainLoop() {
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
    }
}
</pre>

这段代码应该很容易看懂。它循环并检查GLFW事件，直到按下X按钮，或者关闭窗体。该循环结构稍后会调用渲染函数。

一旦窗体关闭，我们需要通过cleanup函数清理资源、结束GLFW本身。

<pre>
void cleanup() {
    glfwDestroyWindow(window);

    glfwTerminate();
}
</pre>

运行程序，我们应该会看到一个名为Vulkan的白色窗体，直到关闭窗体终止应用程序。

ok，到现在我们已经完成了一个Vulkan程序的骨架原型，在下一小节我们会创建第一个Vulkan Object!

[代码](src/02.cpp)。