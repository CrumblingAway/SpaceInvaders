#include "space_invaders_game.h"

#include "tiny_obj_loader.h"

#include <array>
#include <iostream>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <ktx.h>
#include <ktxvulkan.h>
#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>

#define PRINT_SDL_CRITICAL_ERROR(text)\
    std::cerr << "SDL: " << text << ". Error " << SDL_GetError() << ".\n";
#define PRINT_VULKAN_CRITICAL_ERROR(text, error_enum)\
    std::cerr << "Vulkan: " << text << ". Error " << error_enum << ".\n";
#define PRINT_TINYOBJ_CRITICAL_ERROR(text, string_warn, string_error)\
    std::cerr << "tinyobj: " << text << ". Warning: " << string_warn << ". Error: " << string_error << ".\n";

#define RETURN_FALSE_ON_FAIL_VULKAN(text, error_num)\
    if (error_num != VK_SUCCESS)\
    {\
        PRINT_VULKAN_CRITICAL_ERROR(text, error_num);\
        return false;\
    }

struct Vertex
{
    glm::vec3 pos;
    glm::vec3 normal;
    glm::vec2 uv;
};

bool SpaceInvadersGame::Run()
{
    return Init()
           && MainLoop()
           && Cleanup();
}

const char *SpaceInvadersGame::GetName() const
{
    return m_pApplicationName;
}

bool SpaceInvadersGame::Init()
{
    return InitSDL()
           && InitVulkan();
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

bool SpaceInvadersGame::CleanupSDL()
{
    SDL_DestroyWindow(m_sdlWindow);

    return true;
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
    RETURN_FALSE_ON_FAIL_VULKAN("Failed to init instance", vkResult);
    }

    { /* Physical device. */
    #pragma region Select physical device.
    uint32_t deviceCount{ 0 };
    vkResult = vkEnumeratePhysicalDevices(m_vkInstance, &deviceCount, nullptr);
    m_vkDevices.resize(deviceCount);
    vkResult = vkEnumeratePhysicalDevices(m_vkInstance, &deviceCount, m_vkDevices.data());
    RETURN_FALSE_ON_FAIL_VULKAN("Failed to enumerate physical devices", vkResult);
    VkPhysicalDeviceProperties2 deviceProperties
    {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2
    };
    vkGetPhysicalDeviceProperties2(m_vkDevices[m_deviceIndex], &deviceProperties);
    std::cout << "Selected device: " << deviceProperties.properties.deviceName <<  "\n";
    #pragma endregion Select physical device.
    }

    uint32_t queueFamily{ 0 };
    { /* Queue family. */
    #pragma region Get queue family info.
    uint32_t queueFamilyCount{ 0 };
    vkGetPhysicalDeviceQueueFamilyProperties(m_vkDevices[m_deviceIndex], &queueFamilyCount, nullptr);
    std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(m_vkDevices[m_deviceIndex], &queueFamilyCount, queueFamilies.data());
    for (size_t i = 0; i < queueFamilies.size(); ++i)
    {
        if (queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)
        {
            queueFamily = i;
            break;
        }
    }
    if (!SDL_Vulkan_GetPresentationSupport(m_vkInstance, m_vkDevices[m_deviceIndex], queueFamily))
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
    vkResult = vkCreateDevice(m_vkDevices[m_deviceIndex], &deviceCI, nullptr, &m_vkDevice);
    RETURN_FALSE_ON_FAIL_VULKAN("Failed to create device handle", vkResult);
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
        .physicalDevice = m_vkDevices[m_deviceIndex],
        .device = m_vkDevice,
        .pVulkanFunctions = &vkFunctions,
        .instance = m_vkInstance
    };
    vkResult = vmaCreateAllocator(&allocatorCI, &m_vmaAllocator);
    RETURN_FALSE_ON_FAIL_VULKAN("Failed to create VMA allocator", vkResult);
    }

    { /* Vulkan surface. */
    if (!SDL_Vulkan_CreateSurface(m_sdlWindow, m_vkInstance, nullptr, &m_vkSurface)
        || !SDL_GetWindowSize(m_sdlWindow, &m_glmWindowSize.x, &m_glmWindowSize.y))
    {
        PRINT_SDL_CRITICAL_ERROR("Failed to create Vulkan surface");
        return false;
    }
    vkResult = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
        m_vkDevices[m_deviceIndex],
        m_vkSurface,
        &m_surfaceCapabilities
    );
    RETURN_FALSE_ON_FAIL_VULKAN("Failed to get physical device surface capabilities", vkResult);
    }

    { /* Swapchain. */
    VkExtent2D swapchainExtent{ m_surfaceCapabilities.currentExtent };
    if (m_surfaceCapabilities.currentExtent.width == 0xFFFFFFFF)
    {
        swapchainExtent = {
            .width = static_cast<uint32_t>(m_glmWindowSize.x),
            .height = static_cast<uint32_t>(m_glmWindowSize.y),
        };
    }

    m_swapchainCI = 
    {
        .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
        .surface = m_vkSurface,
        .minImageCount = m_surfaceCapabilities.minImageCount,
        .imageFormat = m_imageFormat,
        .imageColorSpace = VK_COLORSPACE_SRGB_NONLINEAR_KHR,
        .imageExtent{ .width = swapchainExtent.width, .height = swapchainExtent.height },
        .imageArrayLayers = 1,
        .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        .preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR,
        .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
        .presentMode = VK_PRESENT_MODE_FIFO_KHR
    };
    vkResult = vkCreateSwapchainKHR(m_vkDevice, &m_swapchainCI, nullptr, &m_vkSwapchain);
    RETURN_FALSE_ON_FAIL_VULKAN("Failed to create swapchain", vkResult);

    vkResult = vkGetSwapchainImagesKHR(m_vkDevice, m_vkSwapchain, &m_imageCount, nullptr);
    RETURN_FALSE_ON_FAIL_VULKAN("Failed to get swapchain images", vkResult);
    m_vkSwapchainImages.resize(m_imageCount);
    vkResult = vkGetSwapchainImagesKHR(m_vkDevice, m_vkSwapchain, &m_imageCount, m_vkSwapchainImages.data());
    RETURN_FALSE_ON_FAIL_VULKAN("Failed to get swapchain images", vkResult);
    m_vkSwapchainImageViews.resize(m_imageCount);
    }

    { /* Depth attachment. */
    std::vector<VkFormat> depthFormatList{
        VK_FORMAT_D32_SFLOAT_S8_UINT,
        VK_FORMAT_D24_UNORM_S8_UINT
    };
    for (VkFormat& format : depthFormatList)
    {
        VkFormatProperties2 formatProperties{ .sType = VK_STRUCTURE_TYPE_FORMAT_PROPERTIES_2 };
        vkGetPhysicalDeviceFormatProperties2(m_vkDevices[m_deviceIndex], format, &formatProperties);
        if (formatProperties.formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT)
        {
            m_depthFormat = format;
            break;
        }
    }

    m_depthImageCI =
    {
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .imageType = VK_IMAGE_TYPE_2D,
        .format = m_depthFormat,
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
        &m_depthImageCI,
        &allocationCI,
        &m_vkDepthImage,
        &m_vmaDepthImageAllocation,
        nullptr
    );
    RETURN_FALSE_ON_FAIL_VULKAN("Failed to create depth image", vkResult);

    VkImageViewCreateInfo depthViewCI
    {
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image = m_vkDepthImage,
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = m_depthFormat,
        .subresourceRange{
            .aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT,
            .levelCount = 1,
            .layerCount = 1
        }
    };
    vkResult = vkCreateImageView(m_vkDevice, &depthViewCI, nullptr, &m_vkDepthImageView);
    RETURN_FALSE_ON_FAIL_VULKAN("Failed to create depth image view", vkResult);
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

    m_vkIndexCount = shapes[0].mesh.indices.size();
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

    m_vkBufSize = sizeof(Vertex) * vertices.size();
    VkDeviceSize iBufSize{ sizeof(uint16_t) * indices.size() };
    VkBufferCreateInfo bufferCI
    {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = m_vkBufSize + iBufSize,
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
    RETURN_FALSE_ON_FAIL_VULKAN("Failed to create VMA buffer", vkResult);
    memcpy(vBufferAllocationInfo.pMappedData, vertices.data(), m_vkBufSize);
    memcpy(((char*)vBufferAllocationInfo.pMappedData) + m_vkBufSize, indices.data(), iBufSize);
    }

    { /* Parallelism. */
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
            &m_shaderDataBuffers[i].buffer,
            &m_shaderDataBuffers[i].allocation,
            &m_shaderDataBuffers[i].allocationInfo
        );
        RETURN_FALSE_ON_FAIL_VULKAN("Failed to create VMA buffer", vkResult);

        VkBufferDeviceAddressInfo uBufferBdaInfo
        {
            .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
            .buffer = m_shaderDataBuffers[i].buffer
        };
        m_shaderDataBuffers[i].deviceAddress = vkGetBufferDeviceAddress(m_vkDevice, &uBufferBdaInfo);
    }

    VkFenceCreateInfo fenceCI
    {
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
        .flags = VK_FENCE_CREATE_SIGNALED_BIT
    };
    for (uint32_t i = 0; i < m_maxFramesInFlight; ++i)
    {
        vkResult = vkCreateFence(m_vkDevice, &fenceCI, nullptr, &m_vkFences[i]);
        RETURN_FALSE_ON_FAIL_VULKAN("Failed to create fences", vkResult);
        vkResult = vkCreateSemaphore(m_vkDevice, &m_semaphoreCI, nullptr, &m_vkImageAcquiredSemaphores[i]);
        RETURN_FALSE_ON_FAIL_VULKAN("Failed to create semaphores", vkResult);
        m_vkRenderCompleteSemaphores.resize(m_vkSwapchainImages.size());
        for (auto& semaphore : m_vkRenderCompleteSemaphores)
        {
            vkResult = vkCreateSemaphore(m_vkDevice, &m_semaphoreCI, nullptr, &semaphore);
            RETURN_FALSE_ON_FAIL_VULKAN("Failed to create semaphore", vkResult);
        }
    }
    }

    { /* Command buffers. */
        VkCommandPoolCreateInfo commandPoolCI
        {
            .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
            .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
            .queueFamilyIndex = queueFamily
        };
        vkResult = vkCreateCommandPool(m_vkDevice, &commandPoolCI, nullptr, &m_vkCommandPool);
        RETURN_FALSE_ON_FAIL_VULKAN("Failed to create command buffer pool", vkResult);

        VkCommandBufferAllocateInfo commandBufferAllocateCI
        {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
            .commandPool = m_vkCommandPool,
            .commandBufferCount = m_maxFramesInFlight
        };
        vkResult = vkAllocateCommandBuffers(m_vkDevice, &commandBufferAllocateCI, m_vkCommandBuffers.data());
        RETURN_FALSE_ON_FAIL_VULKAN("Failed to allocate command buffers", vkResult);
    }

    { /* Textures. */
    std::vector<VkDescriptorImageInfo> textureDescriptors{};
    for (auto i = 0; i < m_textures.size(); ++i)
    {
        ktxTexture* _ktxTexture{ nullptr };
        std::string filename = "../assets/suzanne" + std::to_string(i) + ".ktx";
        ktxTexture_CreateFromNamedFile(filename.c_str(), KTX_TEXTURE_CREATE_LOAD_IMAGE_DATA_BIT, &_ktxTexture);

        VkImageCreateInfo texImgCI
        {
            .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
            .imageType = VK_IMAGE_TYPE_2D,
            .format = ktxTexture_GetVkFormat(_ktxTexture),
            .extent = {
                .width = _ktxTexture->baseWidth,
                .height = _ktxTexture->baseHeight,
                .depth = 1
            },
            .mipLevels = _ktxTexture->numLevels,
            .arrayLayers = 1,
            .samples = VK_SAMPLE_COUNT_1_BIT,
            .tiling = VK_IMAGE_TILING_OPTIMAL,
            .usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
            .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED
        };
        VmaAllocationCreateInfo texImageAllocationCI{ .usage = VMA_MEMORY_USAGE_AUTO };
        vkResult = vmaCreateImage(
            m_vmaAllocator,
            &texImgCI,
            &texImageAllocationCI,
            &m_textures[i].image,
            &m_textures[i].allocation,
            nullptr
        );
        RETURN_FALSE_ON_FAIL_VULKAN("Failed to create textures", vkResult);

        VkImageViewCreateInfo texViewCI
        {
            .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .image = m_textures[i].image,
            .viewType = VK_IMAGE_VIEW_TYPE_2D,
            .format = texImgCI.format,
            .subresourceRange = {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .levelCount = _ktxTexture->numLevels,
                .layerCount = 1
            }
        };
        vkResult = vkCreateImageView(m_vkDevice, &texViewCI, nullptr, &m_textures[i].view);
        RETURN_FALSE_ON_FAIL_VULKAN("Failed to create texture image views", vkResult);

        VkBuffer imgSrcBuffer{};
        VmaAllocation imgSrcAllocation{};
        VkBufferCreateInfo imgSrcBufferCI
        {
            .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
            .size = (uint32_t)_ktxTexture->dataSize,
            .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT
        };
        VmaAllocationCreateInfo imgSrcAllocationCI
        {
            .flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT
                        | VMA_ALLOCATION_CREATE_MAPPED_BIT,
            .usage = VMA_MEMORY_USAGE_AUTO
        };
        VmaAllocationInfo imgSrcAllocationInfo{};
        vkResult = vmaCreateBuffer(
            m_vmaAllocator,
            &imgSrcBufferCI,
            &imgSrcAllocationCI,
            &imgSrcBuffer,
            &imgSrcAllocation,
            &imgSrcAllocationInfo
        );
        RETURN_FALSE_ON_FAIL_VULKAN("Failed to create texture buffer", vkResult);

        memcpy(imgSrcAllocationInfo.pMappedData, _ktxTexture->pData, _ktxTexture->dataSize);

        VkFenceCreateInfo fenceOneTimeCI
        {
            .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO
        };
        VkFence fenceOneTime{};
        vkResult = vkCreateFence(m_vkDevice, &fenceOneTimeCI, nullptr, &fenceOneTime);
        RETURN_FALSE_ON_FAIL_VULKAN("Failed to create OneTime fence", vkResult);
        VkCommandBuffer cbOneTime{};
        VkCommandBufferAllocateInfo cbOneTimeAI
        {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
            .commandPool = m_vkCommandPool,
            .commandBufferCount = 1
        };
        vkResult = vkAllocateCommandBuffers(m_vkDevice, &cbOneTimeAI, &cbOneTime);
        RETURN_FALSE_ON_FAIL_VULKAN("Failed to create OneTime command buffer", vkResult);

        VkCommandBufferBeginInfo cbOneTimeBI
        {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
            .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
        };
        vkResult = vkBeginCommandBuffer(cbOneTime, &cbOneTimeBI);
        RETURN_FALSE_ON_FAIL_VULKAN("Failed to begin OneTime command buffer", vkResult);
        VkImageMemoryBarrier2 barrierTexImage
        {
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
            .srcStageMask = VK_PIPELINE_STAGE_2_NONE,
            .srcAccessMask = VK_ACCESS_2_NONE,
            .dstStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
            .dstAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT,
            .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            .image = m_textures[i].image,
            .subresourceRange = {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .levelCount = _ktxTexture->numLevels,
                .layerCount = 1
            }
        };
        VkDependencyInfo barrierTexInfo
        {
            .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
            .imageMemoryBarrierCount = 1,
            .pImageMemoryBarriers = &barrierTexImage
        };
        vkCmdPipelineBarrier2(cbOneTime, &barrierTexInfo);
        std::vector<VkBufferImageCopy> copyRegions{};
        for (auto j = 0; j < _ktxTexture->numLevels; ++j)
        {
            ktx_size_t mipOffset{ 0 };
            KTX_error_code ret = ktxTexture_GetImageOffset(_ktxTexture, j, 0, 0, &mipOffset);
            copyRegions.push_back({
                .bufferOffset = mipOffset,
                .imageSubresource{
                    .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                    .mipLevel = (uint32_t)j,
                    .layerCount = 1
                },
                .imageExtent{
                    .width = _ktxTexture->baseWidth >> j,
                    .height = _ktxTexture->baseHeight >> j,
                    .depth = 1
                }
            });
        }
        vkCmdCopyBufferToImage(
            cbOneTime,
            imgSrcBuffer,
            m_textures[i].image,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            static_cast<uint32_t>(copyRegions.size()),
            copyRegions.data()
        );
        VkImageMemoryBarrier2 barrierTexRead
        {
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
            .srcStageMask = VK_PIPELINE_STAGE_TRANSFER_BIT,
            .srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
            .dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            .dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
            .oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            .newLayout = VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL,
            .image = m_textures[i].image,
            .subresourceRange = {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .levelCount = _ktxTexture->numLevels,
                .layerCount = 1
            }
        };
        barrierTexInfo.pImageMemoryBarriers = &barrierTexRead;
        vkCmdPipelineBarrier2(cbOneTime, &barrierTexInfo);
        vkResult = vkEndCommandBuffer(cbOneTime);
        RETURN_FALSE_ON_FAIL_VULKAN("Failed to end OneTime command buffer", vkResult);
        VkCommandBufferSubmitInfo cbOneTimeSubmitInfo
        {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
            .commandBuffer = cbOneTime
        };
        VkSubmitInfo2 oneTimeSI
        {
            .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
            .commandBufferInfoCount = 1,
            .pCommandBufferInfos = &cbOneTimeSubmitInfo
        };
        vkResult = vkQueueSubmit2(m_vkQueue, 1, &oneTimeSI, fenceOneTime);
        RETURN_FALSE_ON_FAIL_VULKAN("Failed to submit OneTime queue", vkResult);
        vkResult = vkWaitForFences(m_vkDevice, 1, &fenceOneTime, VK_TRUE, UINT64_MAX);
        RETURN_FALSE_ON_FAIL_VULKAN("Failed to wait for OneTime fence", vkResult);

        VkSamplerCreateInfo samplerCI
        {
            .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
            .magFilter = VK_FILTER_LINEAR,
            .minFilter = VK_FILTER_LINEAR,
            .mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
            .anisotropyEnable = VK_TRUE,
            .maxAnisotropy = 8.0f, // widely supported value for max anisotropy
            .maxLod = (float)_ktxTexture->numLevels
        };
        vkResult = vkCreateSampler(m_vkDevice, &samplerCI, nullptr, &m_textures[i].sampler);
        RETURN_FALSE_ON_FAIL_VULKAN("Failed to create sampler", vkResult);

        ktxTexture_Destroy(_ktxTexture);
        textureDescriptors.push_back({
            .sampler = m_textures[i].sampler,
            .imageView = m_textures[i].view,
            .imageLayout = VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL
        });
        
        VkDescriptorBindingFlags descVariableFlag{ VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT };
        VkDescriptorSetLayoutBindingFlagsCreateInfo descBindingFlags
        {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO,
            .bindingCount = 1,
            .pBindingFlags = &descVariableFlag
        };
        VkDescriptorSetLayoutBinding descLayoutBindingTex
        {
            .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .descriptorCount = static_cast<uint32_t>(m_textures.size()),
            .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT
        };
        VkDescriptorSetLayoutCreateInfo descLayoutTexCI
        {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
            .pNext = &descBindingFlags,
            .bindingCount = 1,
            .pBindings = &descLayoutBindingTex
        };
        vkResult = vkCreateDescriptorSetLayout(m_vkDevice, &descLayoutTexCI, nullptr, &m_vkDescriptorSetLayoutTex);
        RETURN_FALSE_ON_FAIL_VULKAN("Failed to create descriptor set", vkResult);

        VkDescriptorPoolSize poolSize
        {
            .type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .descriptorCount = static_cast<uint32_t>(m_textures.size())
        };
        VkDescriptorPoolCreateInfo descPoolCI
        {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
            .maxSets = 1,
            .poolSizeCount = 1,
            .pPoolSizes = &poolSize
        };
        vkResult = vkCreateDescriptorPool(m_vkDevice, &descPoolCI, nullptr, &m_vkDescriptorPool);
        RETURN_FALSE_ON_FAIL_VULKAN("Failed to create descriptor pool", vkResult);

        uint32_t variableDescCount{ static_cast<uint32_t>(m_textures.size()) };
        VkDescriptorSetVariableDescriptorCountAllocateInfo variableDescCountAI
        {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_ALLOCATE_INFO_EXT,
            .descriptorSetCount = 1,
            .pDescriptorCounts = &variableDescCount
        };
        VkDescriptorSetAllocateInfo texDescSetAlloc
        {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
            .pNext = &variableDescCountAI,
            .descriptorPool = m_vkDescriptorPool,
            .descriptorSetCount = 1,
            .pSetLayouts = &m_vkDescriptorSetLayoutTex
        };
        vkResult = vkAllocateDescriptorSets(m_vkDevice, &texDescSetAlloc, &m_vkDescriptorSetTex);
        RETURN_FALSE_ON_FAIL_VULKAN("Failed to allocate descriptor sets", vkResult);

        VkWriteDescriptorSet writeDescSet
        {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = m_vkDescriptorSetTex,
            .dstBinding = 0,
            .descriptorCount = static_cast<uint32_t>(textureDescriptors.size()),
            .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .pImageInfo = textureDescriptors.data()
        };
        vkUpdateDescriptorSets(m_vkDevice, 1, &writeDescSet, 0, nullptr);
    }
    }

    {} // Unknown. Removing these braces means vscode does not recognize the braces below as foldable.

    VkShaderModule shaderModule{};
    { /* Shaders. */
    slang::createGlobalSession(m_slangGlobalSession.writeRef());
    auto slangTargets{ std::to_array<slang::TargetDesc>({ {
        .format{ SLANG_SPIRV },
        .profile{ m_slangGlobalSession->findProfile("spirv_1_4") }
    }})};
    auto slangOptions{ std::to_array<slang::CompilerOptionEntry>({ {
        slang::CompilerOptionName::EmitSpirvDirectly,
        { slang::CompilerOptionValueKind::Int, 1 }
    }})};
    slang::SessionDesc slangSessionDesc
    {
        .targets{ slangTargets.data() },
        .targetCount{ SlangInt(slangTargets.size()) },
        .defaultMatrixLayoutMode = SLANG_MATRIX_LAYOUT_COLUMN_MAJOR,
        .compilerOptionEntries{ slangOptions.data() },
        .compilerOptionEntryCount{ uint32_t(slangOptions.size()) }
    };
    Slang::ComPtr<slang::ISession> slangSession;
    m_slangGlobalSession->createSession(slangSessionDesc, slangSession.writeRef());

    Slang::ComPtr<slang::IModule> slangModule
    {
        slangSession->loadModuleFromSource("triangle", "../assets/shader.slang", nullptr, nullptr)
    };
    Slang::ComPtr<ISlangBlob> spirv;
    slangModule->getTargetCode(0, spirv.writeRef());

    VkShaderModuleCreateInfo shaderModuleCI
    {
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = spirv->getBufferSize(),
        .pCode = (uint32_t*)spirv->getBufferPointer()
    };
    vkResult = vkCreateShaderModule(m_vkDevice, &shaderModuleCI, nullptr, &shaderModule);
    RETURN_FALSE_ON_FAIL_VULKAN("Failed to create shader module", vkResult);
    }

    { /* Graphics pipeline. */
    VkPushConstantRange pushConstantRange
    {
        .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
        .size = sizeof(VkDeviceAddress)
    };
    VkPipelineLayoutCreateInfo pipelineLayoutCI
    {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = 1,
        .pSetLayouts = &m_vkDescriptorSetLayoutTex,
        .pushConstantRangeCount = 1,
        .pPushConstantRanges = &pushConstantRange
    };
    vkResult = vkCreatePipelineLayout(m_vkDevice, &pipelineLayoutCI, nullptr, &m_vkPipelineLayout);
    RETURN_FALSE_ON_FAIL_VULKAN("Failed to create pipeline layout", vkResult);

    VkVertexInputBindingDescription vertexBinding
    {
        .binding = 0,
        .stride = sizeof(Vertex),
        .inputRate = VK_VERTEX_INPUT_RATE_VERTEX
    };

    std::vector<VkVertexInputAttributeDescription> vertexAttributes{
        { .location = 0, .binding = 0, .format = VK_FORMAT_R32G32B32_SFLOAT },
        { .location = 1, .binding = 0, .format = VK_FORMAT_R32G32B32_SFLOAT, .offset = offsetof(Vertex, normal) },
        { .location = 2, .binding = 0, .format = VK_FORMAT_R32G32_SFLOAT, .offset = offsetof(Vertex, uv) },
    };

    VkPipelineVertexInputStateCreateInfo vertexInputState
    {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .vertexBindingDescriptionCount = 1,
        .pVertexBindingDescriptions = &vertexBinding,
        .vertexAttributeDescriptionCount = static_cast<uint32_t>(vertexAttributes.size()),
        .pVertexAttributeDescriptions = vertexAttributes.data()
    };

    VkPipelineInputAssemblyStateCreateInfo inputAssemblyState
    {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST
    };

    std::vector<VkPipelineShaderStageCreateInfo> shaderStages
    {
        {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_VERTEX_BIT,
            .module = shaderModule,
            .pName = "main"
        },
        {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
            .module = shaderModule,
            .pName = "main"
        }
    };

    VkPipelineViewportStateCreateInfo viewportState
    {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .viewportCount = 1,
        .scissorCount = 1
    };
    std::vector<VkDynamicState> dynamicStates{
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR
    };
    VkPipelineDynamicStateCreateInfo dynamicState
    {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
        .dynamicStateCount = static_cast<uint32_t>(dynamicStates.size()),
        .pDynamicStates = dynamicStates.data()
    };

    VkPipelineDepthStencilStateCreateInfo depthStencilState
    {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
        .depthTestEnable = VK_TRUE,
        .depthWriteEnable = VK_TRUE,
        .depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL
    };

    VkPipelineRenderingCreateInfo renderingCI
    {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
        .colorAttachmentCount = 1,
        .pColorAttachmentFormats = &m_imageFormat,
        .depthAttachmentFormat = m_depthFormat
    };

    VkPipelineColorBlendAttachmentState blendAttachment{
        .colorWriteMask = 0xF
    };
    VkPipelineColorBlendStateCreateInfo colorBlendState{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .attachmentCount = 1,
        .pAttachments = &blendAttachment
    };
    VkPipelineRasterizationStateCreateInfo rasterizationState{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        .lineWidth = 1.0f
    };
    VkPipelineMultisampleStateCreateInfo multisampleState{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT
    };

    VkGraphicsPipelineCreateInfo pipelineCI
    {
        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .pNext = &renderingCI,
        .stageCount = static_cast<uint32_t>(shaderStages.size()),
        .pStages = shaderStages.data(),
        .pVertexInputState = &vertexInputState,
        .pInputAssemblyState = &inputAssemblyState,
        .pViewportState = &viewportState,
        .pRasterizationState = &rasterizationState,
        .pMultisampleState = &multisampleState,
        .pDepthStencilState = &depthStencilState,
        .pColorBlendState = &colorBlendState,
        .pDynamicState = &dynamicState,
        .layout = m_vkPipelineLayout
    };
    vkResult = vkCreateGraphicsPipelines(
        m_vkDevice,
        VK_NULL_HANDLE,
        1,
        &pipelineCI,
        nullptr,
        &m_vkPipeline
    );
    RETURN_FALSE_ON_FAIL_VULKAN("Failed to create graphics pipeline", vkResult);
    }

    return true;
}

bool SpaceInvadersGame::CleanupVulkan()
{
    return true;
}

bool SpaceInvadersGame::MainLoop()
{
    uint64_t lastTime{ SDL_GetTicks() };
    bool quit{ false };
    VkResult vkResult;

    while (!quit)
    {
        assert(m_frameIndex < m_maxFramesInFlight);

        { /* Wait on fence. */
        vkResult = vkWaitForFences(m_vkDevice, 1, &m_vkFences[m_frameIndex], true, UINT64_MAX);
        RETURN_FALSE_ON_FAIL_VULKAN("Failed to wait for fence", vkResult);
        vkResult = vkResetFences(m_vkDevice, 1, &m_vkFences[m_frameIndex]);
        RETURN_FALSE_ON_FAIL_VULKAN("Failed to reset fence", vkResult);
        }

        { /* Acquire next image. */
        vkResult = vkAcquireNextImageKHR(
            m_vkDevice,
            m_vkSwapchain,
            UINT64_MAX,
            m_vkImageAcquiredSemaphores[m_frameIndex],
            VK_NULL_HANDLE,
            &m_imageIndex
        ); 
        if (!CheckSwapchain(vkResult))
        {
            return false;
        }
        }

        { /* Update shader data. */
        m_shaderData.projection = glm::perspective(
            glm::radians(45.0f),
            (float)m_glmWindowSize.x / (float) m_glmWindowSize.y,
            0.1f,
            32.0f
        );
        m_shaderData.view = glm::translate(glm::mat4(1.0f), m_camPos);
        for (auto i = 0; i < 3; ++i)
        {
            auto instancePos = glm::vec3((float)(i - 1) * 3.0f, 0.0f, 0.0f);
            m_shaderData.model[i] = glm::translate(glm::mat4(1.0f), instancePos)
                                    * glm::mat4_cast(glm::quat(m_objectRotations[i]));
        }
        memcpy(m_shaderDataBuffers[m_frameIndex].allocationInfo.pMappedData, &m_shaderData, sizeof(ShaderData));
        }
        
        auto cb = m_vkCommandBuffers[m_frameIndex];
        { /* Record command buffer. */
        vkResult = vkResetCommandBuffer(cb, 0);
        RETURN_FALSE_ON_FAIL_VULKAN("Failed to reset command buffer", vkResult);

        VkCommandBufferBeginInfo cbBI
        {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
            .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
        };
        vkResult = vkBeginCommandBuffer(cb, &cbBI);
        RETURN_FALSE_ON_FAIL_VULKAN("Failed to begin command buffer", vkResult);

        std::array<VkImageMemoryBarrier2, 2> outputBarriers
        {
            VkImageMemoryBarrier2
            {
                .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
                .srcStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                .srcAccessMask = 0,
                .dstStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                .newLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
                .image = m_vkSwapchainImages[m_imageIndex],
                .subresourceRange{
                    .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                    .levelCount = 1,
                    .layerCount = 1
                }
            },
            VkImageMemoryBarrier2
            {
                .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
                .srcStageMask = VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT,
                .srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
                .dstStageMask = VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT,
                .dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
                .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                .newLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
                .image = m_vkDepthImage,
                .subresourceRange{
                    .aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT,
                    .levelCount = 1,
                    .layerCount = 1
                }
            }
        };
        VkDependencyInfo barrierDependencyInfo
        {
            .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
            .imageMemoryBarrierCount = static_cast<uint32_t>(outputBarriers.size()),
            .pImageMemoryBarriers = outputBarriers.data()
        };
        vkCmdPipelineBarrier2(cb, &barrierDependencyInfo);

        VkRenderingAttachmentInfo colorAttachmentInfo
        {
            .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
            .imageView = m_vkSwapchainImageViews[m_imageIndex],
            .imageLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
            .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
            .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
            .clearValue{ .color{ 0.0f, 0.0f, 0.2f, 1.0f }}
        };
        VkRenderingAttachmentInfo depthAttachmentInfo
        {
            .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
            .imageView = m_vkDepthImageView,
            .imageLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
            .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
            .storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
            .clearValue = { .depthStencil = { 1.0f, 0 } }
        };

        VkRenderingInfo renderingInfo
        {
            .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
            .renderArea{
                .extent {
                    .width = static_cast<uint32_t>(m_glmWindowSize.x),
                    .height = static_cast<uint32_t>(m_glmWindowSize.y)
                }
            },
            .layerCount = 1,
            .colorAttachmentCount = 1,
            .pColorAttachments = &colorAttachmentInfo,
            .pDepthAttachment = &depthAttachmentInfo
        };
        vkCmdBeginRendering(cb, &renderingInfo);

        VkViewport vp
        {
            .width = static_cast<float>(m_glmWindowSize.x),
            .height = static_cast<float>(m_glmWindowSize.y),
            .minDepth = 0.0f,
            .maxDepth = 1.0f
        };
        vkCmdSetViewport(cb, 0, 1, &vp);
        VkRect2D scissor
        {
            .extent{
                .width = static_cast<uint32_t>(m_glmWindowSize.x),
                .height = static_cast<uint32_t>(m_glmWindowSize.y)
            }
        };
        vkCmdSetScissor(cb, 0, 1, &scissor);

        vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, m_vkPipeline);
        VkDeviceSize vOffset{ 0 };
        vkCmdBindDescriptorSets(
            cb,
            VK_PIPELINE_BIND_POINT_GRAPHICS,
            m_vkPipelineLayout,
            0,
            1,
            &m_vkDescriptorSetTex,
            0,
            nullptr
        );
        vkCmdBindVertexBuffers(cb, 0, 1, &m_vkBuffer, &vOffset);
        vkCmdBindIndexBuffer(cb, m_vkBuffer, m_vkBufSize, VK_INDEX_TYPE_UINT16);

        vkCmdPushConstants(
            cb,
            m_vkPipelineLayout,
            VK_SHADER_STAGE_VERTEX_BIT,
            0,
            sizeof(VkDeviceAddress),
            &m_shaderDataBuffers[m_frameIndex].deviceAddress
        );

        vkCmdDrawIndexed(cb, m_vkIndexCount, 3, 0, 0, 0);
        vkCmdEndRendering(cb);

        VkImageMemoryBarrier2 barrierPresent
        {
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
            .srcStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
            .srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
            .dstStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
            .dstAccessMask = 0,
            .oldLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
            .newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
            .image = m_vkSwapchainImages[m_imageIndex],
            .subresourceRange{
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .levelCount = 1,
                .layerCount = 1
            }
        };
        VkDependencyInfo barrierPresentDependencyInfo
        {
            .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
            .imageMemoryBarrierCount = 1,
            .pImageMemoryBarriers = &barrierPresent
        };
        vkCmdPipelineBarrier2(cb, &barrierPresentDependencyInfo);

        vkEndCommandBuffer(cb);
        }

        {} // Unknown. Removing these braces means vscode does not recognize the braces below as foldable.

        { /* Submit command buffer. */
        VkSemaphoreSubmitInfo waitSemaphoreInfo
        {
            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
            .semaphore = m_vkImageAcquiredSemaphores[m_frameIndex],
            .stageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT
        };
        VkCommandBufferSubmitInfo commandBufferSubmitInfo
        {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
            .commandBuffer = cb
        };
        VkSemaphoreSubmitInfo signalSemaphoreInfo
        {
            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
            .semaphore = m_vkRenderCompleteSemaphores[m_imageIndex],
            .stageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT
        };
        VkSubmitInfo2 submitInfo
        {
            .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
            .waitSemaphoreInfoCount = 1,
            .pWaitSemaphoreInfos = &waitSemaphoreInfo,
            .commandBufferInfoCount = 1,
            .pCommandBufferInfos = &commandBufferSubmitInfo,
            .signalSemaphoreInfoCount = 1,
            .pSignalSemaphoreInfos = &signalSemaphoreInfo
        };
        vkResult = vkQueueSubmit2(m_vkQueue, 1, &submitInfo, m_vkFences[m_frameIndex]);
        RETURN_FALSE_ON_FAIL_VULKAN("Failed to submit queue", vkResult);

        m_frameIndex = (m_frameIndex + 1) % m_maxFramesInFlight;
        }

        { /* Present image. */
        VkPresentInfoKHR presentInfo
        {
            .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
            .waitSemaphoreCount = 1,
            .pWaitSemaphores = &m_vkRenderCompleteSemaphores[m_imageIndex],
            .swapchainCount = 1,
            .pSwapchains = &m_vkSwapchain,
            .pImageIndices = &m_imageIndex
        };
        vkResult = vkQueuePresentKHR(m_vkQueue, &presentInfo);
        if (!CheckSwapchain(vkResult))
        {
            return false;
        }
        }
        
        { /* Poll events. */
        float elapsedTime{ (SDL_GetTicks() - lastTime) / 1000.0f };
        lastTime = SDL_GetTicks();
        for (SDL_Event event; SDL_PollEvent(&event);) {

            // Exit loop if the application is about to close
            if (event.type == SDL_EVENT_QUIT) {
                quit = true;
                break;
            }

            // Rotate the selected object with mouse drag
            if (event.type == SDL_EVENT_MOUSE_MOTION) {
                if (event.button.button == SDL_BUTTON_LEFT) {
                    m_objectRotations[m_shaderData.selected].x -= (float)event.motion.yrel * elapsedTime;
                    m_objectRotations[m_shaderData.selected].y += (float)event.motion.xrel * elapsedTime;
                }
            }

            // Zooming with the mouse wheel
            if (event.type == SDL_EVENT_MOUSE_WHEEL) {
                m_camPos.z += (float)event.wheel.y * elapsedTime * 10.0f;
            }

            // Select active model instance
            if (event.type == SDL_EVENT_KEY_DOWN) {
                if (event.key.key == SDLK_PLUS || event.key.key == SDLK_KP_PLUS) {
                    m_shaderData.selected = (m_shaderData.selected < 2) ? m_shaderData.selected + 1 : 0;
                }
                if (event.key.key == SDLK_MINUS || event.key.key == SDLK_KP_MINUS) {
                    m_shaderData.selected = (m_shaderData.selected > 0) ? m_shaderData.selected - 1 : 2;
                }
            }

            // Window resize
            if (event.type == SDL_EVENT_WINDOW_RESIZED) {
                m_updateSwapchain = true;
            }
        }
        }

        { /* Recreate swapchain. */
        if (m_updateSwapchain)
        {
            m_updateSwapchain = false;
            vkResult = vkDeviceWaitIdle(m_vkDevice);
            RETURN_FALSE_ON_FAIL_VULKAN("Failed to wait for device idle", vkResult);
            vkResult = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
                m_vkDevices[m_deviceIndex],
                m_vkSurface,
                &m_surfaceCapabilities
            );
            m_swapchainCI.oldSwapchain = m_vkSwapchain;
            m_swapchainCI.imageExtent = {
                .width = static_cast<uint32_t>(m_glmWindowSize.x),
                .height = static_cast<uint32_t>(m_glmWindowSize.y)
            };
            vkResult = vkCreateSwapchainKHR(m_vkDevice, &m_swapchainCI, nullptr, &m_vkSwapchain);
            RETURN_FALSE_ON_FAIL_VULKAN("Failed to create swapchain", vkResult);
            for (auto i = 0; i < m_imageCount; i++) {
                vkDestroyImageView(m_vkDevice, m_vkSwapchainImageViews[i], nullptr);
            }
            vkResult = vkGetSwapchainImagesKHR(m_vkDevice, m_vkSwapchain, &m_imageCount, nullptr);
            RETURN_FALSE_ON_FAIL_VULKAN("Failed to get swapchain images", vkResult);
            m_vkSwapchainImages.resize(m_imageCount);
            vkResult = vkGetSwapchainImagesKHR(
                m_vkDevice,
                m_vkSwapchain,
                &m_imageCount,
                m_vkSwapchainImages.data()
            );
            RETURN_FALSE_ON_FAIL_VULKAN("Failed to get swapchain images", vkResult);
            m_vkSwapchainImageViews.resize(m_imageCount);
            for (auto i = 0; i < m_imageCount; i++) {
                VkImageViewCreateInfo viewCI{
                    .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
                    .image = m_vkSwapchainImages[i],
                    .viewType = VK_IMAGE_VIEW_TYPE_2D,
                    .format = m_imageFormat,
                    .subresourceRange = {
                        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                        .levelCount = 1,
                        .layerCount = 1
                    }
                };
                vkResult = vkCreateImageView(m_vkDevice, &viewCI, nullptr, &m_vkSwapchainImageViews[i]);
                RETURN_FALSE_ON_FAIL_VULKAN("Failed to create image view", vkResult);
            }
            for (auto& semaphore : m_vkRenderCompleteSemaphores) {
                vkDestroySemaphore(m_vkDevice, semaphore, nullptr);
            }
            m_vkRenderCompleteSemaphores.resize(m_imageCount);
            for (auto& semaphore : m_vkRenderCompleteSemaphores) {
                vkResult = vkCreateSemaphore(m_vkDevice, &m_semaphoreCI, nullptr, &semaphore);
                RETURN_FALSE_ON_FAIL_VULKAN("Failed to create semaphore", vkResult);
            }   
            vkDestroySwapchainKHR(m_vkDevice, m_swapchainCI.oldSwapchain, nullptr);
            vmaDestroyImage(m_vmaAllocator, m_vkDepthImage, m_vmaDepthImageAllocation);
            vkDestroyImageView(m_vkDevice, m_vkDepthImageView, nullptr);
            m_depthImageCI.extent = {
                .width = static_cast<uint32_t>(m_glmWindowSize.x),
                .height = static_cast<uint32_t>(m_glmWindowSize.y),
                .depth = 1
            };
            VmaAllocationCreateInfo allocCI{
                .flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT,
                .usage = VMA_MEMORY_USAGE_AUTO
            };
            vkResult = vmaCreateImage(
                m_vmaAllocator,
                &m_depthImageCI,
                &allocCI,
                &m_vkDepthImage,
                &m_vmaDepthImageAllocation,
                nullptr
            );
            RETURN_FALSE_ON_FAIL_VULKAN("Failed to create image", vkResult);
            VkImageViewCreateInfo viewCI{
                .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
                .image = m_vkDepthImage,
                .viewType = VK_IMAGE_VIEW_TYPE_2D,
                .format = m_depthFormat,
                .subresourceRange = {.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT, .levelCount = 1, .layerCount = 1 }
            };
            vkResult = vkCreateImageView(m_vkDevice, &viewCI, nullptr, &m_vkDepthImageView);
            RETURN_FALSE_ON_FAIL_VULKAN("Failed to create image view", vkResult);
        }
        }
    }

    return true;
}

bool SpaceInvadersGame::CheckSwapchain(VkResult result)
{
    if (result >= VK_SUCCESS)
    {
        return true;
    }

    if (result == VK_ERROR_OUT_OF_DATE_KHR)
    {
        m_updateSwapchain = true;
        return true;
    }

    std::cerr << "Vulkan swapchain call failed. Error " << result << "\n";
    return false;
}

bool SpaceInvadersGame::Cleanup()
{
    return CleanupVulkan()
           && CleanupSDL();
}
