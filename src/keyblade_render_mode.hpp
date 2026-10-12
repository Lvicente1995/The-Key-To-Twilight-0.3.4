#pragma once
#include <cstdint>
namespace kingdom {
enum class KeybladeRenderMode : std::int64_t {
    Auto = 0,
    FullDetail = 1,
    MultiplayerSafe = 2,
};
inline constexpr const char* kKeybladeRenderOptions[] = {
    "Auto",
    "Full Detail",
    "Multiplayer Safe",
};
inline constexpr bool validKeybladeRenderMode(std::int64_t value) {
    return value >= 0 && value <= 2;
}
inline constexpr KeybladeRenderMode keybladeRenderModeFromId(std::int64_t value) {
    return validKeybladeRenderMode(value)
        ? static_cast<KeybladeRenderMode>(value)
        : KeybladeRenderMode::Auto;
}
}
