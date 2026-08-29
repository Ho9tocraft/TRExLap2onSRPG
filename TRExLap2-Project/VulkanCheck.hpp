#pragma once

#ifndef VK_USE_PLATFORM_WIN32_KHR
#define VK_USE_PLATFORM_WIN32_KHR
#endif

#include <vulkan/vulkan.h>

#include <source_location>
#include <stdexcept>
#include <string_view>
#include <sstream>
#include <iostream>

namespace TRExLap2::Vulkan {
	/// <summary>
	/// VkResultの主要な列挙値を診断メッセージ用の文字列へ変換する。
	/// </summary>
	inline const char* ToString(VkResult result) noexcept
	{
		switch (result)
		{
		case VK_SUCCESS: return "VK_SUCCESS";
		case VK_NOT_READY: return "VK_NOT_READY";
		case VK_TIMEOUT: return "VK_TIMEOUT";
		case VK_EVENT_SET: return "VK_EVENT_SET";
		case VK_EVENT_RESET: return "VK_EVENT_RESET";
		case VK_INCOMPLETE: return "VK_INCOMPLETE";
		case VK_ERROR_OUT_OF_HOST_MEMORY: return "VK_ERROR_OUT_OF_HOST_MEMORY";
		case VK_ERROR_OUT_OF_DEVICE_MEMORY: return "VK_ERROR_OUT_OF_DEVICE_MEMORY";
		case VK_ERROR_INITIALIZATION_FAILED: return "VK_ERROR_INITIALIZATION_FAILED";
		case VK_ERROR_DEVICE_LOST: return "VK_ERROR_DEVICE_LOST";
		case VK_ERROR_MEMORY_MAP_FAILED: return "VK_ERROR_MEMORY_MAP_FAILED";
		case VK_ERROR_LAYER_NOT_PRESENT: return "VK_ERROR_LAYER_NOT_PRESENT";
		case VK_ERROR_EXTENSION_NOT_PRESENT: return "VK_ERROR_EXTENSION_NOT_PRESENT";
		case VK_ERROR_FEATURE_NOT_PRESENT: return "VK_ERROR_FEATURE_NOT_PRESENT";
		case VK_ERROR_INCOMPATIBLE_DRIVER: return "VK_ERROR_INCOMPATIBLE_DRIVER";
		case VK_ERROR_TOO_MANY_OBJECTS: return "VK_ERROR_TOO_MANY_OBJECTS";
		case VK_ERROR_FORMAT_NOT_SUPPORTED: return "VK_ERROR_FORMAT_NOT_SUPPORTED";
		case VK_ERROR_FRAGMENTED_POOL: return "VK_ERROR_FRAGMENTED_POOL";
		case VK_ERROR_OUT_OF_DATE_KHR: return "VK_ERROR_OUT_OF_DATE_KHR";
		case VK_SUBOPTIMAL_KHR: return "VK_SUBOPTIMAL_KHR";
		default: return "VK_RESULT_UNKNOWN";
		}
	}

	/// <summary>
	/// Vulkan API失敗の結果・式・ソース位置を整形して例外として送出する。
	/// </summary>
	[[noreturn]] inline void ThrowIfFailed(VkResult result, std::string_view expression, std::source_location location = std::source_location::current())
	{
		std::ostringstream message;

		message
			<< "Vulkan call failed: " << expression << '\n'
			<< "Result: " << ToString(result)
			<< " (" << static_cast<int>(result) << ")\n"
			<< "Location: " << location.file_name()
			<< ':' << location.line();

		throw std::runtime_error(message.str());
	}
};

#define VK_CHECK(expression) \
	do \
	{ \
		const VkResult vkResult_ = (expression); \
		if (vkResult_ != VK_SUCCESS) \
		{ \
			TRExLap2::Vulkan::ThrowIfFailed(vkResult_, #expression); \
		} \
	} while (false)
