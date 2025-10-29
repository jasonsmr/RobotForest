#pragma once
#include <android/asset_manager.h>
#include <android/native_window.h>
#include <vulkan/vulkan.h>
#include <stdbool.h>
#include <stdint.h>

typedef struct VkApp {
    // Android
    AAssetManager* assets;
    ANativeWindow* window;
    bool ready;

    // Vulkan core
    VkInstance instance;
    VkPhysicalDevice phys;
    VkDevice device;
    uint32_t qFamily;
    VkQueue queue;

    // Surface/Swapchain
    VkSurfaceKHR surface;
    VkSwapchainKHR swapchain;
    VkFormat swapFormat;
    VkExtent2D extent;

    // Render stuff
    VkRenderPass renderPass;
    VkPipelineLayout pipelineLayout;
    VkPipeline pipeline;

    // Images/Views/Framebuffers
    uint32_t imageCount;
    VkImage* images;
    VkImageView* views;
    VkFramebuffer* framebuffers;

    // Command/Sync
    VkCommandPool cmdPool;
    VkCommandBuffer* cmdBufs;
    VkSemaphore imageAvailable;
    VkSemaphore renderFinished;
    VkFence inFlight;

} VkApp;

void vkapp_init(VkApp* a, AAssetManager* am, ANativeWindow* win);
void vkapp_term_surface(VkApp* a);
void vkapp_destroy(VkApp* a);
void vkapp_draw_frame(VkApp* a);
