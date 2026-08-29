#include "space_invaders_game.h"

#include <iostream>
#include <vector>

#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>

#define SDL_CRITICAL_ERROR(text)\
    std::cerr << "SDL: " << text << ". Error " << SDL_GetError() << ".\n";\
    std::exit(EXIT_FAILURE);
#define VULKAN_CRITICAL_ERROR(text, error_enum)\
    std::cerr << "Vulkan: " << text << ". Error " << error_enum << ".\n";\
    std::exit(EXIT_FAILURE);

void SpaceInvadersGame::Run()
{
    Init();
    MainLoop();
    Cleanup();
}

const char *SpaceInvadersGame::GetName() const
{
    return m_pApplicationName;
}

void SpaceInvadersGame::Init()
{
    InitSDL();
    InitVulkan();
}

void SpaceInvadersGame::InitSDL()
{
    if (SDL_Init(SDL_INIT_VIDEO) < 0)
    {
        SDL_CRITICAL_ERROR("Failed to initialize");
    }

    m_sdlWindow = SDL_CreateWindow(
        m_pApplicationName,
        m_windowWidth,
        m_windowHeight,
        SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE
    );
    if (m_sdlWindow == nullptr)
    {
        SDL_CRITICAL_ERROR("Failed to create window");
    }
}

void SpaceInvadersGame::CleanupSDL()
{
    SDL_DestroyWindow(m_sdlWindow);
}

void SpaceInvadersGame::InitVulkan()
{
    #pragma region Create instance.
    VkApplicationInfo appInfo
    {
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pApplicationName = m_pApplicationName,
        .apiVersion = VK_API_VERSION_1_4
    };

    uint32_t instanceExtensionsCount{ 0 };
    char const* const* instanceExtensions{ SDL_Vulkan_GetInstanceExtensions(&instanceExtensionsCount) };

    VkInstanceCreateInfo instanceCI
    {
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pApplicationInfo = &appInfo,
        .enabledExtensionCount = instanceExtensionsCount,
        .ppEnabledExtensionNames = instanceExtensions
    };

    VkResult vkResult;
    vkResult = vkCreateInstance(&instanceCI, nullptr, &m_vkInstance); 
    if (vkResult != VK_SUCCESS)
    {
        VULKAN_CRITICAL_ERROR("Failed to init instance", vkResult);
    }
    #pragma endregion Create instance.

    #pragma region Select physical device.
    uint32_t deviceCount{ 0 };
    vkResult = vkEnumeratePhysicalDevices(m_vkInstance, &deviceCount, nullptr);
    std::vector<VkPhysicalDevice> devices(deviceCount);
    vkResult = vkEnumeratePhysicalDevices(m_vkInstance, &deviceCount, devices.data());
    if (vkResult != VK_SUCCESS)
    {
        VULKAN_CRITICAL_ERROR("Failed to enumerate physical devices", vkResult);
    }
    uint32_t deviceIndex{ 0 };
    VkPhysicalDeviceProperties2 deviceProperties
    {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2
    };
    vkGetPhysicalDeviceProperties2(devices[deviceIndex], &deviceProperties);
    std::cout << "Selected device: " << deviceProperties.properties.deviceName <<  "\n";
    #pragma endregion Select physical device.

    #pragma region Get queue family info.
    uint32_t queueFamilyCount{ 0 };
    vkGetPhysicalDeviceQueueFamilyProperties(devices[deviceIndex], &queueFamilyCount, nullptr);
    std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(devices[deviceIndex], &queueFamilyCount, queueFamilies.data());
    uint32_t queueFamily{ 0 };
    for (size_t i = 0; i < queueFamilies.size(); ++i)
    {
        if (queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)
        {
            queueFamily = i;
            break;
        }
    }
    if (!SDL_Vulkan_GetPresentationSupport(m_vkInstance, devices[deviceIndex], queueFamily))
    {
        SDL_CRITICAL_ERROR("Presentation not supported with provided Vulkan physical device and"
        " queue family");
    }
    #pragma endregion Get queue family info.

    #pragma region Create device.
    const float queuePriorities{ 1.0f };
    VkDeviceQueueCreateInfo queueCI
    {
        .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
        .queueFamilyIndex = queueFamily,
        .queueCount = 1,
        .pQueuePriorities = &queuePriorities
    };

    const std::vector<const char*> deviceExtensions{ VK_KHR_SWAPCHAIN_EXTENSION_NAME };
    VkPhysicalDeviceVulkan12Features enabledVk12Features
    {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES,
        .descriptorIndexing = true,
        .shaderSampledImageArrayNonUniformIndexing = true,
        .descriptorBindingVariableDescriptorCount = true,
        .runtimeDescriptorArray = true,
        .bufferDeviceAddress = true
    };
    VkPhysicalDeviceVulkan13Features enabledVk13Features
    {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES,
        .pNext = &enabledVk12Features,
        .synchronization2 = true,
        .dynamicRendering = true
    };
    VkPhysicalDeviceFeatures enabledVk10Features
    {
        .samplerAnisotropy = VK_TRUE
    };

    VkDeviceCreateInfo deviceCI
    {
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .pNext = &enabledVk13Features,
        .queueCreateInfoCount = 1,
        .pQueueCreateInfos = &queueCI,
        .enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size()),
        .ppEnabledExtensionNames = deviceExtensions.data(),
        .pEnabledFeatures = &enabledVk10Features
    };
    vkResult = vkCreateDevice(devices[deviceIndex], &deviceCI, nullptr, &m_vkDevice);
    if (vkResult != VK_SUCCESS)
    {
        VULKAN_CRITICAL_ERROR("Failed to create device handle", vkResult);
    }
    vkGetDeviceQueue(m_vkDevice, queueFamily, 0, &m_vkQueue);
    #pragma endregion Create device.

    #pragma region VMA.
    VmaVulkanFunctions vkFunctions
    {
        .vkGetInstanceProcAddr = vkGetInstanceProcAddr,
        .vkGetDeviceProcAddr = vkGetDeviceProcAddr,
        .vkCreateImage = vkCreateImage
    };
    VmaAllocatorCreateInfo allocatorCI
    {
        .flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT,
        .physicalDevice = devices[deviceIndex],
        .device = m_vkDevice,
        .pVulkanFunctions = &vkFunctions,
        .instance = m_vkInstance
    };
    vkResult = vmaCreateAllocator(&allocatorCI, &m_vmaAllocator);
    if (vkResult != VK_SUCCESS)
    {
        VULKAN_CRITICAL_ERROR("Failed to create VMA allocator", vkResult);
    }
    #pragma endregion VMA.

    #pragma region Create Vulkan surface.
    if (!SDL_Vulkan_CreateSurface(m_sdlWindow, m_vkInstance, nullptr, &m_vkSurface))
    {
        SDL_CRITICAL_ERROR("Failed to create Vulkan surface");
    }
    VkSurfaceCapabilitiesKHR surfaceCapabilities{};
    vkResult = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
        devices[deviceIndex],
        m_vkSurface,
        &surfaceCapabilities
    );
    if (vkResult != VK_SUCCESS)
    {
        VULKAN_CRITICAL_ERROR("Failed to get physical device surface capabilities", vkResult);
    }
    #pragma endregion Create Vulkan surface.
    
}

void SpaceInvadersGame::CleanupVulkan()
{

}

void SpaceInvadersGame::MainLoop()
{

}

void SpaceInvadersGame::Cleanup()
{
    CleanupVulkan();
    CleanupSDL();
}
