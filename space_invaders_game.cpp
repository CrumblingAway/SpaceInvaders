#include "space_invaders_game.h"

#include "tiny_obj_loader.h"

#include <iostream>

#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>

#define PRINT_SDL_CRITICAL_ERROR(text)\
    std::cerr << "SDL: " << text << ". Error " << SDL_GetError() << ".\n";
#define PRINT_VULKAN_CRITICAL_ERROR(text, error_enum)\
    std::cerr << "Vulkan: " << text << ". Error " << error_enum << ".\n";
#define PRINT_TINYOBJ_CRITICAL_ERROR(text, string_warn, string_error)\
    std::cerr << "tinyobj: " << text << ". Warning: " << string_warn << ". Error: " << string_error << ".\n";

struct Vertex
{
    glm::vec3 pos;
    glm::vec3 normal;
    glm::vec2 uv;
};

struct ShaderDataBuffer
{
    VmaAllocation allocation{ VK_NULL_HANDLE };
    VmaAllocationInfo allocationInfo{};
    VkBuffer buffer{ VK_NULL_HANDLE };
    VkDeviceAddress deviceAddress{};
};

struct ShaderData
{
    glm::mat4 projection;
    glm::mat4 view;
    glm::mat4 model[3];
    glm::vec4 lightPos{ 0.0f, -10.0f, 10.0f, 0.0f };
    uint32_t selected{ 1 };
};

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
    if (!InitSDL())
    {

    }
    if (!InitVulkan())
    {

    }
}

bool SpaceInvadersGame::InitSDL()
{
    if (SDL_Init(SDL_INIT_VIDEO) < 0)
    {
        PRINT_SDL_CRITICAL_ERROR("Failed to initialize");
        return false;
    }

    m_sdlWindow = SDL_CreateWindow(
        m_pApplicationName,
        m_windowWidth,
        m_windowHeight,
        SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE
    );
    if (m_sdlWindow == nullptr)
    {
        PRINT_SDL_CRITICAL_ERROR("Failed to create window");
        return false;
    }

    return true;
}

void SpaceInvadersGame::CleanupSDL()
{
    SDL_DestroyWindow(m_sdlWindow);
}

bool SpaceInvadersGame::InitVulkan()
{
    VkResult vkResult;

    { /* Instance. */
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

    vkResult = vkCreateInstance(&instanceCI, nullptr, &m_vkInstance); 
    if (vkResult != VK_SUCCESS)
    {
        PRINT_VULKAN_CRITICAL_ERROR("Failed to init instance", vkResult);
        return false;
    }
    }

    std::vector<VkPhysicalDevice> devices;
    uint32_t deviceIndex{ 0 };
    { /* Physical device. */
    #pragma region Select physical device.
    uint32_t deviceCount{ 0 };
    vkResult = vkEnumeratePhysicalDevices(m_vkInstance, &deviceCount, nullptr);
    devices.resize(deviceCount);
    vkResult = vkEnumeratePhysicalDevices(m_vkInstance, &deviceCount, devices.data());
    if (vkResult != VK_SUCCESS)
    {
        PRINT_VULKAN_CRITICAL_ERROR("Failed to enumerate physical devices", vkResult);
        return false;
    }
    VkPhysicalDeviceProperties2 deviceProperties
    {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2
    };
    vkGetPhysicalDeviceProperties2(devices[deviceIndex], &deviceProperties);
    std::cout << "Selected device: " << deviceProperties.properties.deviceName <<  "\n";
    #pragma endregion Select physical device.
    }

    uint32_t queueFamily{ 0 };
    { /* Queue family. */
    #pragma region Get queue family info.
    uint32_t queueFamilyCount{ 0 };
    vkGetPhysicalDeviceQueueFamilyProperties(devices[deviceIndex], &queueFamilyCount, nullptr);
    std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(devices[deviceIndex], &queueFamilyCount, queueFamilies.data());
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
        PRINT_SDL_CRITICAL_ERROR("Presentation not supported with provided Vulkan physical device and"
        " queue family");
        return false;
    }
    }

    { /* Device. */
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
        PRINT_VULKAN_CRITICAL_ERROR("Failed to create device handle", vkResult);
        return false;
    }
    vkGetDeviceQueue(m_vkDevice, queueFamily, 0, &m_vkQueue);
    }

    { /* VMA. */
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
        PRINT_VULKAN_CRITICAL_ERROR("Failed to create VMA allocator", vkResult);
        return false;
    }
    }

    VkSurfaceCapabilitiesKHR surfaceCapabilities{};
    { /* Vulkan surface. */
    if (!SDL_Vulkan_CreateSurface(m_sdlWindow, m_vkInstance, nullptr, &m_vkSurface)
        || !SDL_GetWindowSize(m_sdlWindow, &m_glmWindowSize.x, &m_glmWindowSize.y))
    {
        PRINT_SDL_CRITICAL_ERROR("Failed to create Vulkan surface");
        return false;
    }
    vkResult = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
        devices[deviceIndex],
        m_vkSurface,
        &surfaceCapabilities
    );
    if (vkResult != VK_SUCCESS)
    {
        PRINT_VULKAN_CRITICAL_ERROR("Failed to get physical device surface capabilities", vkResult);
        return false;
    }
    }

    { /* Swapchain. */
    VkExtent2D swapchainExtent{ surfaceCapabilities.currentExtent };
    if (surfaceCapabilities.currentExtent.width == 0xFFFFFFFF)
    {
        swapchainExtent = {
            .width = static_cast<uint32_t>(m_glmWindowSize.x),
            .height = static_cast<uint32_t>(m_glmWindowSize.y),
        };
    }

    const VkFormat imageFormat{ VK_FORMAT_B8G8R8A8_SRGB };
    VkSwapchainCreateInfoKHR swapchainCI
    {
        .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
        .surface = m_vkSurface,
        .minImageCount = surfaceCapabilities.minImageCount,
        .imageFormat = imageFormat,
        .imageColorSpace = VK_COLORSPACE_SRGB_NONLINEAR_KHR,
        .imageExtent{ .width = swapchainExtent.width, .height = swapchainExtent.height },
        .imageArrayLayers = 1,
        .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        .preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR,
        .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
        .presentMode = VK_PRESENT_MODE_FIFO_KHR
    };
    vkResult = vkCreateSwapchainKHR(m_vkDevice, &swapchainCI, nullptr, &m_vkSwapchain);
    if (vkResult != VK_SUCCESS)
    {
        PRINT_VULKAN_CRITICAL_ERROR("Failed to create swapchain", vkResult);
        return false;
    }

    uint32_t imageCount{ 0 };
    vkResult = vkGetSwapchainImagesKHR(m_vkDevice, m_vkSwapchain, &imageCount, nullptr);
    if (vkResult != VK_SUCCESS)
    {
        PRINT_VULKAN_CRITICAL_ERROR("Failed to get swapchain images", vkResult);
        return false;
    }
    m_vkSwapchainImages.resize(imageCount);
    vkResult = vkGetSwapchainImagesKHR(m_vkDevice, m_vkSwapchain, &imageCount, m_vkSwapchainImages.data());
    if (vkResult != VK_SUCCESS)
    {
        PRINT_VULKAN_CRITICAL_ERROR("Failed to get swapchain images", vkResult);
        return false;
    }
    m_vkSwapchainImageViews.resize(imageCount);
    }

    { /* Depth attachment. */
    std::vector<VkFormat> depthFormatList{
        VK_FORMAT_D32_SFLOAT_S8_UINT,
        VK_FORMAT_D24_UNORM_S8_UINT
    };
    VkFormat depthFormat{ VK_FORMAT_UNDEFINED };
    for (VkFormat& format : depthFormatList)
    {
        VkFormatProperties2 formatProperties{ .sType = VK_STRUCTURE_TYPE_FORMAT_PROPERTIES_2 };
        vkGetPhysicalDeviceFormatProperties2(devices[deviceIndex], format, &formatProperties);
        if (formatProperties.formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT)
        {
            depthFormat = format;
            break;
        }
    }

    VkImageCreateInfo depthImageCI
    {
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .imageType = VK_IMAGE_TYPE_2D,
        .format = depthFormat,
        .extent{
            .width = static_cast<uint32_t>(m_glmWindowSize.x),
            .height = static_cast<uint32_t>(m_glmWindowSize.y),
            .depth = 1
        },
        .mipLevels = 1,
        .arrayLayers = 1,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .tiling = VK_IMAGE_TILING_OPTIMAL,
        .usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED
    };

    VmaAllocationCreateInfo allocationCI
    {
        .flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT,
        .usage = VMA_MEMORY_USAGE_AUTO
    };
    vkResult = vmaCreateImage(
        m_vmaAllocator,
        &depthImageCI,
        &allocationCI,
        &m_vkDepthImage,
        &m_vmaDepthImageAllocation,
        nullptr
    );
    if (vkResult != VK_SUCCESS)
    {
        PRINT_VULKAN_CRITICAL_ERROR("Failed to create depth image", vkResult);
        return false;
    }

    VkImageViewCreateInfo depthViewCI
    {
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image = m_vkDepthImage,
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = depthFormat,
        .subresourceRange{
            .aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT,
            .levelCount = 1,
            .layerCount = 1
        }
    };
    vkResult = vkCreateImageView(m_vkDevice, &depthViewCI, nullptr, &m_vkDepthImageView);
    if (vkResult != VK_SUCCESS)
    {
        PRINT_VULKAN_CRITICAL_ERROR("Failed to create depth image view", vkResult);
        return false;
    }
    }

    { /* Load meshes. */
    std::string tinyobj_warn;
    std::string tinyobj_error;
    tinyobj::attrib_t attrib;
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;
    if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &tinyobj_warn, &tinyobj_error, "../assets/suzanne.obj"))
    {
        PRINT_TINYOBJ_CRITICAL_ERROR("Failed to load meshes", tinyobj_warn, tinyobj_error);
        return false;
    }

    const VkDeviceSize indexCount{ shapes[0].mesh.indices.size() };
    std::vector<Vertex> vertices{};
    std::vector<uint16_t> indices{};
    for (auto& index : shapes[0].mesh.indices)
    {
        Vertex v{
            .pos = { attrib.vertices[index.vertex_index * 3], -attrib.vertices[index.vertex_index * 3 + 1], attrib.vertices[index.vertex_index * 3 + 2] },
            .normal = { attrib.normals[index.normal_index * 3], -attrib.normals[index.normal_index * 3 + 1], attrib.normals[index.normal_index * 3 + 2] },
            .uv = { attrib.texcoords[index.texcoord_index * 2], 1.0 - attrib.texcoords[index.texcoord_index * 2 + 1] }
        };
        vertices.push_back(v);
        indices.push_back(indices.size());
    }

    VkDeviceSize vBufSize{ sizeof(Vertex) * vertices.size() };
    VkDeviceSize iBufSize{ sizeof(uint16_t) * indices.size() };
    VkBufferCreateInfo bufferCI
    {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = vBufSize + iBufSize,
        .usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT
    };

    VmaAllocationCreateInfo vBufferAllocationCI
    {
        .flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT
                 | VMA_ALLOCATION_CREATE_HOST_ACCESS_ALLOW_TRANSFER_INSTEAD_BIT
                 | VMA_ALLOCATION_CREATE_MAPPED_BIT,
        .usage = VMA_MEMORY_USAGE_AUTO
    };
    VmaAllocationInfo vBufferAllocationInfo{};
    vkResult = vmaCreateBuffer(
        m_vmaAllocator,
        &bufferCI,
        &vBufferAllocationCI,
        &m_vkBuffer,
        &m_vmaBufferAllocation,
        &vBufferAllocationInfo
    );
    if (vkResult != VK_SUCCESS)
    {
        PRINT_VULKAN_CRITICAL_ERROR("Failed to create VMA buffer", vkResult);
        return false;
    }
    memcpy(vBufferAllocationInfo.pMappedData, vertices.data(), vBufSize);
    memcpy(((char*)vBufferAllocationInfo.pMappedData) + vBufSize, indices.data(), iBufSize);
    }

    { /* Parallelism. */
        std::array<ShaderDataBuffer, m_maxFramesInFlight> shaderDataBuffers;
        std::array<VkCommandBuffer, m_maxFramesInFlight> commandBuffers;

        for (uint32_t i = 0; i < m_maxFramesInFlight; ++i)
        {
            VkBufferCreateInfo uBufferCI
            {
                .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
                .size = sizeof(ShaderData),
                .usage = VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT
            };
            VmaAllocationCreateInfo uBufferAllocCI
            {
                .flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT
                         | VMA_ALLOCATION_CREATE_HOST_ACCESS_ALLOW_TRANSFER_INSTEAD_BIT
                         | VMA_ALLOCATION_CREATE_MAPPED_BIT,
                .usage = VMA_MEMORY_USAGE_AUTO
            };
            vkResult = vmaCreateBuffer(
                m_vmaAllocator,
                &uBufferCI,
                &uBufferAllocCI,
                &shaderDataBuffers[i].buffer,
                &shaderDataBuffers[i].allocation,
                &shaderDataBuffers[i].allocationInfo
            );
            if (vkResult != VK_SUCCESS)
            {
                PRINT_VULKAN_CRITICAL_ERROR("Failed to create VMA buffer", vkResult);
                return false;
            }

            VkBufferDeviceAddressInfo uBufferBdaInfo
            {
                .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
                .buffer = shaderDataBuffers[i].buffer
            };
            shaderDataBuffers[i].deviceAddress = vkGetBufferDeviceAddress(m_vkDevice, &uBufferBdaInfo);
        }

        VkSemaphoreCreateInfo semaphoreCI
        {
            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO
        };
        VkFenceCreateInfo fenceCI
        {
            .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
            .flags = VK_FENCE_CREATE_SIGNALED_BIT
        };
        for (uint32_t i = 0; i < m_maxFramesInFlight; ++i)
        {
            vkResult = vkCreateFence(m_vkDevice, &fenceCI, nullptr, &m_vkFences[i]);
            if (vkResult != VK_SUCCESS)
            {
                PRINT_VULKAN_CRITICAL_ERROR("Failed to create fences", vkResult);
                return false;
            }
            vkResult = vkCreateSemaphore(m_vkDevice, &semaphoreCI, nullptr, &m_vkImageAcquiredSemaphores[i]);
            if (vkResult != VK_SUCCESS)
            {
                PRINT_VULKAN_CRITICAL_ERROR("Failed to create semaphores", vkResult);
                return false;
            }
            m_vkRenderCompleteSemaphores.resize(m_vkSwapchainImages.size());
            for (auto& semaphore : m_vkRenderCompleteSemaphores)
            {
                vkResult = vkCreateSemaphore(m_vkDevice, &semaphoreCI, nullptr, &semaphore);
                return false;
            }
        }
    }

    return true;
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
