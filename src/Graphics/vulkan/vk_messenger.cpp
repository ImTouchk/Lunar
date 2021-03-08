#include <vulkan/vulkan.h>

#include "Types.h"
#include "Debug.h"
#include "Graphics/VkCommon.h"
#include "Graphics/Renderer.h"

namespace Lunar {
    void Renderer::SetupMessenger()
    {
#   ifdef VK_USE_DEBUG_LAYERS
        VkDebugUtilsMessengerCreateInfoEXT MessengerCreateInfo;
        MessengerCreateInfo = vk::debugMessengerInfo();

        auto Function =
            (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(
                vk::getInstance(),
                "vkCreateDebugUtilsMessengerEXT"
            );

        if (Function == NULL) {
            return;
        }

        VkDebugUtilsMessengerEXT Messenger = NULL;
        VkResult Result;

        Result = Function(vk::getInstance(), &MessengerCreateInfo, nullptr, &Messenger);
        if (Result != VK_SUCCESS) {
            Lunar::Warn("Renderer> Failed to set up a debug messenger.");
        }
        else Lunar::Print("Renderer> Debug messenger was set up.");

        m_Messenger = Messenger;
#   endif
    }

    void Renderer::DestroyMessenger()
    {
#   ifdef VK_USE_DEBUG_LAYERS
        if (m_Messenger == NULL) {
            return;
        }

        auto Function =
            (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(
                vk::getInstance(),
                "vkDestroyDebugUtilsMessengerEXT"
            );

        if (Function == NULL) {
            return;
        }

        Function(vk::getInstance(), m_Messenger, NULL);
        Lunar::Print("Renderer> Debug messenger destroyed.");
#   endif
    }
}