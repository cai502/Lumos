#if defined(LUMOS_RENDER_API_VULKAN)

#include "Platform/Vulkan/VKDevice.h"
#include "App/Application.h"
#include "iOSOS.h"

namespace Lumos
{
	vk::SurfaceKHR Graphics::VKDevice::CreatePlatformSurface(vk::Instance vkInstance, Window* window)
	{
		vk::SurfaceKHR surface;

        auto iosView = static_cast<iOSOS*>(OS::Instance())->GetIOSView();

        vk::IOSSurfaceCreateInfoMVK surfaceCreateInfo = {};
        surfaceCreateInfo.pNext = NULL;
        surfaceCreateInfo.pView = iosView;
        surface = vkInstance.createIOSSurfaceMVK(surfaceCreateInfo);

		return surface;
	}

    static const char* GetPlatformSurfaceExtension()
	{
		return VK_MVK_IOS_SURFACE_EXTENSION_NAME;
	}
}

#endif
