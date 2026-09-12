#pragma once

#include <cstddef>
#include <vector>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE

#include <glm/glm.hpp>
#include <slang/slang.h>
#include <slang/slang-com-ptr.h>
#include <vulkan/vulkan.h>

#include "vk_mem_alloc.h"

// Forward declarations.
class SDL_Window;

struct ShaderData
{
    glm::mat4 projection;
    glm::mat4 view;
    glm::mat4 model[3];
    glm::vec4 lightPos{ 0.0f, -10.0f, 10.0f, 0.0f };
    uint32_t selected{ 1 };
};

struct ShaderDataBuffer
{
    VmaAllocation allocation{ VK_NULL_HANDLE };
    VmaAllocationInfo allocationInfo{};
    VkBuffer buffer{ VK_NULL_HANDLE };
    VkDeviceAddress deviceAddress{};
};

/**
 * A Space Invaders clone. Only the `Run()` method is designed to be called from the outside. The
 * rest of the class is entirely internally managed.
 */
class SpaceInvadersGame
{
public:
    /**
     * Run the Space Invaders game. Calling this method hands off the application into the hands of
     * this class.
     */
    bool Run();

    /**
     * Get the name of this application. This name is provided internally to the windowing system
     * and the rendering API.
     */
    const char* GetName() const;

private:
    const char* m_pApplicationName = "Space Invaders";

    static constexpr uint32_t m_maxFramesInFlight{ 2 };

    /**
     * Init all dependencies.
     * 
     * As without any of the dependencies this application has no purpose, failure of this method
     * results in immediate termination of the application.
     */
    bool Init();
    bool Cleanup();

    /**
     * Init SDL.
     * 
     * As this is the only windowing library this game accounts for, failure of this method results
     * in immediate termination of the application.
     */
    bool InitSDL();
    bool CleanupSDL();

    /**
     * Init Vulkan.
     * 
     * As this is the only graphics API this game accounts for, failure  of this method results in
     * immediate termination of the application.
     */
    bool InitVulkan();
    bool CleanupVulkan();

    /**
     * This game's main loop. All rendering and game logic is handled in this method.
     */
    bool MainLoop();

    const size_t m_windowHeight = 600;
    const size_t m_windowWidth = 800;
    SDL_Window* m_sdlWindow = nullptr;

    VkInstance m_vkInstance{ VK_NULL_HANDLE };
    VkDevice m_vkDevice{ VK_NULL_HANDLE };
    std::vector<VkPhysicalDevice> m_vkDevices;
    uint32_t m_deviceIndex{ 0 };
    VkQueue m_vkQueue{ VK_NULL_HANDLE };

    VmaAllocator m_vmaAllocator{ VK_NULL_HANDLE };

    VkSurfaceKHR m_vkSurface{ VK_NULL_HANDLE };
    VkSurfaceCapabilitiesKHR m_surfaceCapabilities{};
    glm::ivec2 m_glmWindowSize{};

    VkSwapchainKHR m_vkSwapchain{ VK_NULL_HANDLE };
    VkSwapchainCreateInfoKHR m_swapchainCI;
    uint32_t m_imageCount{ 0 };
    const VkFormat m_imageFormat{ VK_FORMAT_B8G8R8A8_SRGB };
    std::vector<VkImage> m_vkSwapchainImages;
    std::vector<VkImageView> m_vkSwapchainImageViews;
    VkImageCreateInfo m_depthImageCI;
    VkFormat m_depthFormat{ VK_FORMAT_UNDEFINED };
    VkImage m_vkDepthImage;
    VmaAllocation m_vmaDepthImageAllocation;
    VkImageView m_vkDepthImageView;

    VkBuffer m_vkBuffer;
    VmaAllocation m_vmaBufferAllocation;

    VkSemaphoreCreateInfo m_semaphoreCI
    {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO
    };
    std::array<VkFence, m_maxFramesInFlight> m_vkFences;
    std::array<VkSemaphore, m_maxFramesInFlight> m_vkImageAcquiredSemaphores;
    std::vector<VkSemaphore> m_vkRenderCompleteSemaphores;

    VkCommandPool m_vkCommandPool{ VK_NULL_HANDLE };
    std::array<VkCommandBuffer, m_maxFramesInFlight> m_vkCommandBuffers;

    struct Texture
    {
        VmaAllocation allocation{ VK_NULL_HANDLE };
        VkImage image{ VK_NULL_HANDLE };
        VkImageView view{ VK_NULL_HANDLE };
        VkSampler sampler{ VK_NULL_HANDLE };
    };
    std::array<Texture, 3> m_textures{};

    VkDescriptorSetLayout m_vkDescriptorSetLayoutTex{ VK_NULL_HANDLE };
    VkDescriptorSet m_vkDescriptorSetTex{ VK_NULL_HANDLE };
    VkDescriptorPool m_vkDescriptorPool{ VK_NULL_HANDLE };

    Slang::ComPtr<slang::IGlobalSession> m_slangGlobalSession;

    VkPipeline m_vkPipeline{ VK_NULL_HANDLE };
    VkPipelineLayout m_vkPipelineLayout{ VK_NULL_HANDLE };

    uint32_t m_imageIndex{ 0 };
    uint32_t m_frameIndex{ 0 };

    bool m_updateSwapchain{ false };
    VkShaderModule m_vkShaderModule{};
    ShaderData m_shaderData;
    bool CheckSwapchain(VkResult result);

    glm::vec3 m_camPos{ 0.0f, 0.0f, -6.0f };
    glm::vec3 m_objectRotations[3]{};

    std::array<ShaderDataBuffer, m_maxFramesInFlight> m_shaderDataBuffers;
    std::array<VkCommandBuffer, m_maxFramesInFlight> m_commandBuffers;

    VkDeviceSize m_vkBufSize{ 0 };
    VkDeviceSize m_vkIndexCount{ 0 };
};
