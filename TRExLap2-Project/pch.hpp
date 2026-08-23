#pragma once

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#define VK_USE_PLATFORM_WIN32_KHR

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <exception>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <optional>
#include <set>
#include <source_location>
#include <stdexcept>
#include <string_view>
#include <sstream>
#include <vector>
#include <string>

#include <vulkan/vulkan.h>

/** Vulkan型を定義してから、Vulkan用NGX/DLSS APIを読み込む。 */
#include <nvsdk_ngx.h>
#include <nvsdk_ngx_vk.h>
#include <nvsdk_ngx_helpers_vk.h>

#include <Windows.h>
#include <mmsystem.h>
