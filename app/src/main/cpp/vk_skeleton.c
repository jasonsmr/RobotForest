#include "vk_skeleton.h"
#include <android/log.h>
#include <string.h>
#include <stdlib.h>

#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, "VKAPP", __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR,"VKAPP", __VA_ARGS__)
#define CE(expr) do{ VkResult r=(expr); if(r!=VK_SUCCESS){LOGE(#expr" -> %d", r); return false;}}while(0)
#define CEV(expr) do{ VkResult r=(expr); if(r!=VK_SUCCESS){LOGE(#expr" -> %d", r); return;}}while(0)

static uint32_t find_gfx_queue(VkPhysicalDevice pd, VkSurfaceKHR surf){
    uint32_t n=0; vkGetPhysicalDeviceQueueFamilyProperties(pd,&n,NULL);
    VkQueueFamilyProperties* props = (VkQueueFamilyProperties*)malloc(sizeof(*props)*n);
    vkGetPhysicalDeviceQueueFamilyProperties(pd,&n,props);
    for(uint32_t i=0;i<n;i++){
        VkBool32 present=VK_FALSE;
        if(surf) vkGetPhysicalDeviceSurfaceSupportKHR(pd,i,surf,&present);
        if((props[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) && present){ free(props); return i; }
    }
    for(uint32_t i=0;i<n;i++){
        if(props[i].queueFlags & VK_QUEUE_GRAPHICS_BIT){ free(props); return i; }
    }
    free(props);
    return UINT32_MAX;
}

static VkSurfaceFormatKHR choose_surface_format(VkPhysicalDevice pd, VkSurfaceKHR surf){
    uint32_t n=0; vkGetPhysicalDeviceSurfaceFormatsKHR(pd,surf,&n,NULL);
    VkSurfaceFormatKHR* fs = (VkSurfaceFormatKHR*)malloc(sizeof(*fs)*n);
    vkGetPhysicalDeviceSurfaceFormatsKHR(pd,surf,&n,fs);
    VkSurfaceFormatKHR best = fs[0];
    for(uint32_t i=0;i<n;i++){
        if(fs[i].format==VK_FORMAT_R8G8B8A8_UNORM || fs[i].format==VK_FORMAT_B8G8R8A8_UNORM){
            best = fs[i]; break;
        }
    }
    VkSurfaceFormatKHR ret = best;
    free(fs);
    return ret;
}

static VkExtent2D choose_extent(VkSurfaceCapabilitiesKHR caps, uint32_t w, uint32_t h){
    if(caps.currentExtent.width != 0xFFFFFFFF){
        return caps.currentExtent;
    }
    VkExtent2D e = { w, h };
    if(e.width  < caps.minImageExtent.width)  e.width  = caps.minImageExtent.width;
    if(e.height < caps.minImageExtent.height) e.height = caps.minImageExtent.height;
    if(e.width  > caps.maxImageExtent.width)  e.width  = caps.maxImageExtent.width;
    if(e.height > caps.maxImageExtent.height) e.height = caps.maxImageExtent.height;
    return e;
}

static bool create_instance(VkApp* a){
    const char* exts[] = {
        "VK_KHR_surface",
        "VK_KHR_android_surface"
    };
    VkApplicationInfo ai = { .sType=VK_STRUCTURE_TYPE_APPLICATION_INFO };
    ai.pApplicationName = "LauncherVK";
    ai.apiVersion = VK_API_VERSION_1_1;

    VkInstanceCreateInfo ci = { .sType=VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO };
    ci.pApplicationInfo = &ai;
    ci.enabledExtensionCount = 2;
    ci.ppEnabledExtensionNames = exts;

    CE(vkCreateInstance(&ci, NULL, &a->instance));
    return true;
}

static bool pick_device_and_queue(VkApp* a){
    uint32_t n=0; vkEnumeratePhysicalDevices(a->instance, &n, NULL);
    if(n==0){ LOGE("No physical devices."); return false; }
    VkPhysicalDevice* list = (VkPhysicalDevice*)malloc(sizeof(*list)*n);
    vkEnumeratePhysicalDevices(a->instance, &n, list);

    int best = -1;
    for(uint32_t i=0;i<n;i++){
        VkPhysicalDeviceProperties p; vkGetPhysicalDeviceProperties(list[i], &p);
        uint32_t q = find_gfx_queue(list[i], a->surface);
        if(q!=UINT32_MAX){
            if(strstr(p.deviceName, "Adreno")){ a->phys=list[i]; a->gfx_idx=q; best=i; break; }
            if(best<0){ a->phys=list[i]; a->gfx_idx=q; best=i; }
        }
    }
    free(list);
    if(best<0){ LOGE("No suitable device."); return false; }

    float prio=1.0f;
    VkDeviceQueueCreateInfo qci = { .sType=VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO };
    qci.queueFamilyIndex = a->gfx_idx;
    qci.queueCount = 1;
    qci.pQueuePriorities = &prio;

    const char* dev_exts[] = { "VK_KHR_swapchain" };

    VkDeviceCreateInfo dci = { .sType=VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO };
    dci.queueCreateInfoCount = 1;
    dci.pQueueCreateInfos = &qci;
    dci.enabledExtensionCount = 1;
    dci.ppEnabledExtensionNames = dev_exts;

    CE(vkCreateDevice(a->phys, &dci, NULL, &a->dev));
    vkGetDeviceQueue(a->dev, a->gfx_idx, 0, &a->gfx_q);
    return true;
}

static bool create_swapchain(VkApp* a, uint32_t fbw, uint32_t fbh){
    VkSurfaceCapabilitiesKHR caps; CE(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(a->phys, a->surface, &caps));
    VkSurfaceFormatKHR sf = choose_surface_format(a->phys, a->surface);
    VkExtent2D extent = choose_extent(caps, fbw, fbh);

    uint32_t img_count = caps.minImageCount + 1;
    if(caps.maxImageCount && img_count > caps.maxImageCount) img_count = caps.maxImageCount;

    VkSwapchainCreateInfoKHR sci = { .sType=VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR };
    sci.surface = a->surface;
    sci.minImageCount = img_count;
    sci.imageFormat = sf.format;
    sci.imageColorSpace = sf.colorSpace;
    sci.imageExtent = extent;
    sci.imageArrayLayers = 1;
    sci.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    sci.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    sci.preTransform = caps.currentTransform;
    sci.compositeAlpha = VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR;
    sci.presentMode = VK_PRESENT_MODE_FIFO_KHR;
    sci.clipped = VK_TRUE;

    CE(vkCreateSwapchainKHR(a->dev, &sci, NULL, &a->swapchain));
    a->swap_format = sf.format;
    a->swap_colorspace = sf.colorSpace;
    a->swap_extent = extent;

    CE(vkGetSwapchainImagesKHR(a->dev, a->swapchain, &a->swap_count, NULL));
    a->swap_images = (VkImage*)malloc(sizeof(VkImage)*a->swap_count);
    CE(vkGetSwapchainImagesKHR(a->dev, a->swapchain, &a->swap_count, a->swap_images));

    a->swap_views = (VkImageView*)malloc(sizeof(VkImageView)*a->swap_count);
    for(uint32_t i=0;i<a->swap_count;i++){
        VkImageViewCreateInfo iv = { .sType=VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
        iv.image = a->swap_images[i];
        iv.viewType = VK_IMAGE_VIEW_TYPE_2D;
        iv.format = a->swap_format;
        iv.components = (VkComponentMapping){VK_COMPONENT_SWIZZLE_IDENTITY,VK_COMPONENT_SWIZZLE_IDENTITY,VK_COMPONENT_SWIZZLE_IDENTITY,VK_COMPONENT_SWIZZLE_IDENTITY};
        iv.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        iv.subresourceRange.baseMipLevel=0; iv.subresourceRange.levelCount=1;
        iv.subresourceRange.baseArrayLayer=0; iv.subresourceRange.layerCount=1;
        CE(vkCreateImageView(a->dev, &iv, NULL, &a->swap_views[i]));
    }
    return true;
}

static bool create_renderpass_and_fbos(VkApp* a){
    VkAttachmentDescription color = {0};
    color.format = a->swap_format;
    color.samples = VK_SAMPLE_COUNT_1_BIT;
    color.loadOp  = VK_ATTACHMENT_LOAD_OP_CLEAR;
    color.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    color.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    color.finalLayout   = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference cref = { .attachment = 0, .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };

    VkSubpassDescription sp = {0};
    sp.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    sp.colorAttachmentCount = 1;
    sp.pColorAttachments = &cref;

    VkSubpassDependency dep = {0};
    dep.srcSubpass = VK_SUBPASS_EXTERNAL;
    dep.dstSubpass = 0;
    dep.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dep.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dep.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    VkRenderPassCreateInfo rpci = { .sType=VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO };
    rpci.attachmentCount = 1;
    rpci.pAttachments = &color;
    rpci.subpassCount = 1;
    rpci.pSubpasses = &sp;
    rpci.dependencyCount = 1;
    rpci.pDependencies = &dep;

    CE(vkCreateRenderPass(a->dev, &rpci, NULL, &a->render_pass));

    a->framebuffers = (VkFramebuffer*)malloc(sizeof(VkFramebuffer)*a->swap_count);
    for(uint32_t i=0;i<a->swap_count;i++){
        VkImageView atts[] = { a->swap_views[i] };
        VkFramebufferCreateInfo fb = { .sType=VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO };
        fb.renderPass = a->render_pass;
        fb.attachmentCount = 1;
        fb.pAttachments = atts;
        fb.width  = a->swap_extent.width;
        fb.height = a->swap_extent.height;
        fb.layers = 1;
        CE(vkCreateFramebuffer(a->dev, &fb, NULL, &a->framebuffers[i]));
    }
    return true;
}

static bool create_commands_sync(VkApp* a){
    VkCommandPoolCreateInfo cp = { .sType=VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO };
    cp.queueFamilyIndex = a->gfx_idx;
    cp.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    CE(vkCreateCommandPool(a->dev, &cp, NULL, &a->cmd_pool));

    a->cmds = (VkCommandBuffer*)malloc(sizeof(VkCommandBuffer)*a->swap_count);
    VkCommandBufferAllocateInfo ai = { .sType=VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
    ai.commandPool = a->cmd_pool;
    ai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    ai.commandBufferCount = a->swap_count;
    CE(vkAllocateCommandBuffers(a->dev, &ai, a->cmds));

    VkSemaphoreCreateInfo si = { .sType=VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
    CE(vkCreateSemaphore(a->dev, &si, NULL, &a->img_avail));
    CE(vkCreateSemaphore(a->dev, &si, NULL, &a->render_done));

    a->in_flight = (VkFence*)malloc(sizeof(VkFence)*a->swap_count);
    VkFenceCreateInfo fi = { .sType=VK_STRUCTURE_TYPE_FENCE_CREATE_INFO, .flags=VK_FENCE_CREATE_SIGNALED_BIT };
    for(uint32_t i=0;i<a->swap_count;i++) CE(vkCreateFence(a->dev, &fi, NULL, &a->in_flight[i]));
    return true;
}

static void record_cmd(VkApp* a, uint32_t idx){
    VkCommandBuffer cb = a->cmds[idx];
    VkCommandBufferBeginInfo bi = { .sType=VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
    CEV(vkBeginCommandBuffer(cb, &bi));
    VkClearValue clr = { .color = {{0.10f, 0.12f, 0.18f, 1.0f}} };
    VkRenderPassBeginInfo rp = { .sType=VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO };
    rp.renderPass = a->render_pass;
    rp.framebuffer = a->framebuffers[idx];
    rp.renderArea.offset = (VkOffset2D){0,0};
    rp.renderArea.extent = a->swap_extent;
    rp.clearValueCount = 1;
    rp.pClearValues = &clr;
    vkCmdBeginRenderPass(cb, &rp, VK_SUBPASS_CONTENTS_INLINE);
    vkCmdEndRenderPass(cb);
    CEV(vkEndCommandBuffer(cb));
}

bool vkapp_init(VkApp* a, ANativeWindow* win){
    memset(a, 0, sizeof(*a));
    a->window = win;

    if(!create_instance(a)) return false;

    // Android surface
    VkAndroidSurfaceCreateInfoKHR sci = { .sType=VK_STRUCTURE_TYPE_ANDROID_SURFACE_CREATE_INFO_KHR };
    sci.window = win;
    CE(vkCreateAndroidSurfaceKHR(a->instance, &sci, NULL, &a->surface));

    if(!pick_device_and_queue(a)) return false;

    uint32_t w = (uint32_t)ANativeWindow_getWidth(win);
    uint32_t h = (uint32_t)ANativeWindow_getHeight(win);
    if(!create_swapchain(a,w,h)) return false;
    if(!create_renderpass_and_fbos(a)) return false;
    if(!create_commands_sync(a)) return false;

    a->ready = true;
    return true;
}

void vkapp_resize(VkApp* a, int32_t w, int32_t h){
    if(!a->ready) return;
    vkDeviceWaitIdle(a->dev);

    for(uint32_t i=0;i<a->swap_count;i++){
        vkDestroyFramebuffer(a->dev, a->framebuffers[i], NULL);
        vkDestroyImageView(a->dev, a->swap_views[i], NULL);
        vkDestroyFence(a->dev, a->in_flight[i], NULL);
    }
    free(a->framebuffers); free(a->swap_views); free(a->swap_images); free(a->in_flight);
    vkDestroyRenderPass(a->dev, a->render_pass, NULL);
    vkDestroyCommandPool(a->dev, a->cmd_pool, NULL);
    vkDestroySwapchainKHR(a->dev, a->swapchain, NULL);

    create_swapchain(a, (uint32_t)w, (uint32_t)h);
    create_renderpass_and_fbos(a);
    create_commands_sync(a);
}

void vkapp_draw(VkApp* a){
    if(!a->ready) return;

    uint32_t img=0;
    VkResult ar = vkAcquireNextImageKHR(a->dev, a->swapchain, UINT64_MAX, a->img_avail, VK_NULL_HANDLE, &img);
    if(ar==VK_ERROR_OUT_OF_DATE_KHR || ar==VK_SUBOPTIMAL_KHR){
        return;
    } else if(ar!=VK_SUCCESS){
        LOGE("Acquire image failed: %d", ar); return;
    }

    vkWaitForFences(a->dev, 1, &a->in_flight[img], VK_TRUE, UINT64_MAX);
    vkResetFences(a->dev, 1, &a->in_flight[img]);

    record_cmd(a, img);

    VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
    VkSubmitInfo si = { .sType=VK_STRUCTURE_TYPE_SUBMIT_INFO };
    si.waitSemaphoreCount = 1; si.pWaitSemaphores = &a->img_avail; si.pWaitDstStageMask = waitStages;
    si.commandBufferCount = 1; si.pCommandBuffers = &a->cmds[img];
    si.signalSemaphoreCount = 1; si.pSignalSemaphores = &a->render_done;
    CEV(vkQueueSubmit(a->gfx_q, 1, &si, a->in_flight[img]));

    VkPresentInfoKHR pi = { .sType=VK_STRUCTURE_TYPE_PRESENT_INFO_KHR };
    pi.waitSemaphoreCount = 1; pi.pWaitSemaphores = &a->render_done;
    pi.swapchainCount = 1; pi.pSwapchains = &a->swapchain; pi.pImageIndices = &img;
    VkResult pr = vkQueuePresentKHR(a->gfx_q, &pi);
    if(!(pr==VK_SUCCESS || pr==VK_SUBOPTIMAL_KHR || pr==VK_ERROR_OUT_OF_DATE_KHR)){
        LOGE("Present failed: %d", pr);
    }
}

void vkapp_destroy(VkApp* a){
    if(!a->instance) return;
    vkDeviceWaitIdle(a->dev);

    if(a->in_flight){ for(uint32_t i=0;i<a->swap_count;i++) vkDestroyFence(a->dev,a->in_flight[i],NULL); free(a->in_flight); }
    if(a->img_avail) vkDestroySemaphore(a->dev,a->img_avail,NULL);
    if(a->render_done) vkDestroySemaphore(a->dev,a->render_done,NULL);

    if(a->cmd_pool) vkDestroyCommandPool(a->dev,a->cmd_pool,NULL);

    if(a->framebuffers){ for(uint32_t i=0;i<a->swap_count;i++) vkDestroyFramebuffer(a->dev,a->framebuffers[i],NULL); free(a->framebuffers); }
    if(a->swap_views){ for(uint32_t i=0;i<a->swap_count;i++) vkDestroyImageView(a->dev,a->swap_views[i],NULL); free(a->swap_views); }
    if(a->swap_images) free(a->swap_images);
    if(a->swapchain) vkDestroySwapchainKHR(a->dev,a->swapchain,NULL);
    if(a->render_pass) vkDestroyRenderPass(a->dev,a->render_pass,NULL);

    if(a->dev) vkDestroyDevice(a->dev, NULL);
    if(a->surface) vkDestroySurfaceKHR(a->instance,a->surface,NULL);
    vkDestroyInstance(a->instance,NULL);
    memset(a,0,sizeof(*a));
}
