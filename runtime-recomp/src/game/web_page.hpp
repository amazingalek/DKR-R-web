#pragma once

#include <filesystem>

namespace dkr::runtime::web {

// On a page (Emscripten), the launcher: buttons over the canvas that load the
// ROM and start the game. Returns the ROM to start, a copy kept in
// <config_directory>, once the player presses Start Game.
std::filesystem::path choose_rom(const std::filesystem::path& config_directory);

} // namespace dkr::runtime::web
