#pragma once

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#define VK_USE_PLATFORM_WIN32_KHR

#include <algorithm>
#include <array>
#include <exception>
#include <cstdint>
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

#include <nvsdk_ngx.h>
#include <nvsdk_ngx_vk.h>
#include <nvsdk_ngx_helpers_vk.h>
#include <vulkan/vulkan.h>

#include <Windows.h>
