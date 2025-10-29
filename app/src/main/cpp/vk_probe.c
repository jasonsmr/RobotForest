#include <android/log.h>
#include <android/native_window.h>
#include <android_native_app_glue.h>
#include <vulkan/vulkan.h>
#include <string.h>

#define TAG "SkyrimLauncher/VK"

static const char* vk_err(VkResult r) {
    switch (r) {
        case VK_SUCCESS: return "VK_SUCCESS";
        case VK_ERROR_EXTENSION_NOT_PRESENT: return "VK_ERROR_EXTENSION_NOT_PRESENT";
        case VK_ERROR_INCOMPATIBLE_DRIVER: return "VK_ERROR_INCOMPATIBLE_DRIVER";
        default: return "VK_ERROR";
    }
}

int vk_probe_run(struct android_app* app) {
    // 1) Create instance with android surface extension
    const char* exts[] = {
        "VK_KHR_surface",
        "VK_KHR_android_surface"
    };

    VkApplicationInfo ai = {
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pApplicationName = "SkyrimLauncher",
        .applicationVersion = VK_MAKE_VERSION(0,1,0),
        .pEngineName = "Probe",
        .engineVersion = VK_MAKE_VERSION(0,1,0),
        .apiVersion = VK_API_VERSION_1_1
    };

    VkInstanceCreateInfo ici = {
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pApplicationInfo = &ai,
        .enabledExtensionCount = (uint32_t)(sizeof(exts)/sizeof(exts[0])),
        .ppEnabledExtensionNames = exts
    };

    VkInstance inst = VK_NULL_HANDLE;
    VkResult r = vkCreateInstance(&ici, NULL, &inst);

    if (r != VK_SUCCESS) {
        __android_log_print(ANDROID_LOG_ERROR, TAG, "vkCreateInstance failed: %s", vk_err(r));
        return -1;
    }

    // 2) Enumerate physical devices
    uint32_t count = 0;
    r = vkEnumeratePhysicalDevices(inst, &count, NULL);

    if (r != VK_SUCCESS || count == 0) {
        __android_log_print(ANDROID_LOG_ERROR, TAG, "vkEnumeratePhysicalDevices failed or none");
        vkDestroyInstance(inst, NULL);
        return -2;
    }

    VkPhysicalDevice devs[8];
    if (count > 8) count = 8;
    r = vkEnumeratePhysicalDevices(inst, &count, devs);

    if (r != VK_SUCCESS) {
        __android_log_print(ANDROID_LOG_ERROR, TAG, "vkEnumeratePhysicalDevices(list) failed");
        vkDestroyInstance(inst, NULL);
        return -3;
    }

    // 3) Log properties for the first device
    VkPhysicalDeviceProperties props;
    vkGetPhysicalDeviceProperties(devs[0], &props);
    __android_log_print(ANDROID_LOG_INFO, TAG, "GPU: %s | API %u.%u.%u | Driver 0x%x",
                        props.deviceName,
                        VK_VERSION_MAJOR(props.apiVersion),
                        VK_VERSION_MINOR(props.apiVersion),
                        VK_VERSION_PATCH(props.apiVersion),
                        props.driverVersion);

    // 4) Create a surface from the window (if we have one) to confirm support
    if (!app->window) {
        __android_log_print(ANDROID_LOG_WARN, TAG, "No ANativeWindow yet; skipping surface support check");
    } else {
        VkAndroidSurfaceCreateInfoKHR sci = {
            .sType = VK_STRUCTURE_TYPE_ANDROID_SURFACE_CREATE_INFO_KHR,
            .window = app->window
        };
        VkSurfaceKHR surface = VK_NULL_HANDLE;

        PFN_vkCreateAndroidSurfaceKHR pfnCreateAndroidSurfaceKHR = (PFN_vkCreateAndroidSurfaceKHR)vkGetInstanceProcAddr(inst, "vkCreateAndroidSurfaceKHR");

        if (!pfnCreateAndroidSurfaceKHR) {
            __android_log_print(ANDROID_LOG_ERROR, TAG, "vkCreateAndroidSurfaceKHR not found");
        } else {
            r = pfnCreateAndroidSurfaceKHR(inst, &sci, NULL, &surface);

            if (r == VK_SUCCESS) {
                __android_log_print(ANDROID_LOG_INFO, TAG, "Surface creation OK");

                // Check queue family support
                uint32_t qcount = 0;
                vkGetPhysicalDeviceQueueFamilyProperties(devs[0], &qcount, NULL);
                VkQueueFamilyProperties qprops[16];
                if (qcount > 16) qcount = 16;
                vkGetPhysicalDeviceQueueFamilyProperties(devs[0], &qcount, qprops);
                
                VkBool32 present_ok = VK_FALSE;
                for (uint32_t i=0; i<qcount; ++i) {
                    VkBool32 supp = VK_FALSE;
                    vkGetPhysicalDeviceSurfaceSupportKHR(devs[0], i, surface, &supp);
                    if (supp) {
                        present_ok = VK_TRUE;
                        break;
                    }
                }
                __android_log_print(ANDROID_LOG_INFO, TAG, "Present support: %s", present_ok ? "YES" : "NO");

                vkDestroySurfaceKHR(inst, surface, NULL);
            } else {
                __android_log_print(ANDROID_LOG_ERROR, TAG, "Create surface failed: %s", vk_err(r));
            }
        }
    }

    vkDestroyInstance(inst, NULL);
    return 0;
}

