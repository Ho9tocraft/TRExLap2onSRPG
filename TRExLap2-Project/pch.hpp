#pragma once

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#define VK_USE_PLATFORM_WIN32_KHR

// TODO: 標準ライブラリをアルファベット順で。
#include <algorithm>
#include <array>
#include <atomic>
#include <bitset>
#include <chrono>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <limits>
#include <map>
#include <optional>
#include <set>
#include <source_location>
#include <stdexcept>
#include <string>
#include <string_view>
#include <sstream>
#include <utility>
#include <vector>

#include <vulkan/vulkan.h>

#include <nvsdk_ngx.h>
#include <nvsdk_ngx_vk.h>
#include <nvsdk_ngx_helpers_vk.h>

#include <Windows.h>
#include <mmsystem.h>
