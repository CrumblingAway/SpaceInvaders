#pragma once

#include <cstddef>
#include <vector>

#include <glm/glm.hpp>
#include <vulkan/vulkan.h>

#include "vk_mem_alloc.h"

// Forward declarations.
class SDL_Window;

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
    void Run();

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
    void Init();
    void Cleanup();

    /**
     * Init SDL.
     * 
     * As this is the only windowing library this game accounts for, failure of this method results
     * in immediate termination of the application.
     */
    bool InitSDL();
    void CleanupSDL();

    /**
     * Init Vulkan.
     * 
     * As this is the only graphics API this game accounts for, failure  of this method results in
     * immediate termination of the application.
     */
    bool InitVulkan();
    void CleanupVulkan();

    /**
     * This game's main loop. All rendering and game logic is handled in this method.
     */
    void MainLoop();

    const size_t m_windowHeight = 600;
    const size_t m_windowWidth = 800;
    SDL_Window* m_sdlWindow = nullptr;

    VkInstance m_vkInstance{ VK_NULL_HANDLE };
    VkDevice m_vkDevice{ VK_NULL_HANDLE };
    VkQueue m_vkQueue{ VK_NULL_HANDLE };

    VmaAllocator m_vmaAllocator{ VK_NULL_HANDLE };

    VkSurfaceKHR m_vkSurface{ VK_NULL_HANDLE };
    glm::ivec2 m_glmWindowSize{};

    VkSwapchainKHR m_vkSwapchain{ VK_NULL_HANDLE };
    std::vector<VkImage> m_vkSwapchainImages;
    std::vector<VkImageView> m_vkSwapchainImageViews;
    VkImage m_vkDepthImage;
    VmaAllocation m_vmaDepthImageAllocation;
    VkImageView m_vkDepthImageView;

    VkBuffer m_vkBuffer;
    VmaAllocation m_vmaBufferAllocation;

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
};
