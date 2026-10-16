// The page's launcher, built only with Emscripten (CMakeLists.txt). DKR-R's own
// launcher draws through ImGui on an SDL renderer a page doesn't have, and lists
// ROMs from folders a browser can't read. So on a page the game asks for the ROM
// itself, with buttons over the canvas, and keeps the one picked in its config
// folder, which the page keeps in the browser's IndexedDB (N64Recomp-browser's
// persistent_home.js): a later visit starts it with Start Game. The ROM never
// leaves the browser.
#include "web_page.hpp"

#include "game_registration.hpp"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <string>
#include <system_error>
#include <vector>

#include <emscripten.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <emscripten/threading.h>

namespace {

// Shows the launcher on the page's thread and returns at once; the choice comes
// back in *state: 1 Start Game, 2 a file picked and written to <folder>.
// A browser opens its file picker only on a click or a key the player has just
// pressed, so the picker opens from the click's own handler. The buttons are
// hit-tested on the window's mouse events, so a click dispatched on the canvas
// (page_driver.mjs in Firefox) reaches them as a player's does, and Enter is
// Start Game.
EM_JS(void, dkr_web_launcher, (int have_rom, const char* message_ptr, const char* folder_ptr, int32_t* state_ptr), {
    const message = UTF8ToString(message_ptr), folder = UTF8ToString(folder_ptr), state = state_ptr >> 2;
    let panel = document.getElementById('dkr-launcher');
    if (panel) panel.remove();
    panel = document.createElement('div');
    panel.id = 'dkr-launcher';
    panel.style.cssText = 'position:fixed;inset:0;z-index:900;background:#101820;color:#eee;font:16px sans-serif';
    const place = (element, dy) => {
        element.style.cssText += ';position:absolute;left:50%;top:calc(50% + ' + dy + 'px);transform:translate(-50%,-50%)';
        panel.appendChild(element);
        return element;
    };
    const title = place(document.createElement('div'), -90);
    title.textContent = 'Diddy Kong Racing';
    title.style.cssText += ';font-size:32px;font-weight:bold;white-space:nowrap';
    const button = (text, dy) => {
        const b = place(document.createElement('button'), dy);
        b.textContent = text;
        b.style.cssText += ';font-size:20px;padding:10px 28px;min-width:220px;cursor:pointer';
        return b;
    };
    const primary = button(have_rom ? 'Start Game' : 'Load ROM', 0);
    const other = have_rom ? button('Load another ROM', 64) : null;
    const note = place(document.createElement('div'), have_rom ? 124 : 70);
    note.textContent = message || 'The US version 1.0 ROM (.z64, .n64 or .v64). It stays in this browser.';
    note.style.cssText += ';font-size:14px;color:#bbb;width:90%;max-width:640px;text-align:center';

    let done = false;
    const finish = (result) => {
        if (done) return;
        done = true;
        removeEventListener('mouseup', onMouse, true);
        removeEventListener('keydown', onKey, true);
        panel.remove();
        Atomics.store(HEAP32, state, result);
        Atomics.notify(HEAP32, state);
    };
    const pick = () => {
        const input = document.createElement('input');
        input.type = 'file';
        input.addEventListener('change', async () => {
            const file = input.files[0];
            if (!file) return;
            note.textContent = 'Reading ' + file.name + '…';
            FS.mkdirTree(folder);
            FS.writeFile(folder + '/rom', new Uint8Array(await file.arrayBuffer()));
            finish(2);
        });
        input.click();
    };
    const inside = (element, event) => {
        if (!element) return false;
        const r = element.getBoundingClientRect();
        return event.clientX >= r.left && event.clientX < r.right && event.clientY >= r.top && event.clientY < r.bottom;
    };
    const onMouse = (event) => {
        if (inside(primary, event)) {
            have_rom ? finish(1) : pick();
        } else if (inside(other, event)) {
            pick();
        }
    };
    const onKey = (event) => {
        if (have_rom && event.key === 'Enter') {
            event.preventDefault();
            finish(1);
        }
    };
    addEventListener('mouseup', onMouse, true);
    addEventListener('keydown', onKey, true);
    document.body.appendChild(panel);
    const status = document.getElementById('status');
    if (status) status.hidden = true;
});

// Waits on the program's thread until the player chooses.
int Ask(bool have_rom, const std::string& message, const std::string& folder) {
    std::atomic<int32_t> state{0};
    emscripten_async_run_in_main_runtime_thread(EM_FUNC_SIG_VIIII, dkr_web_launcher, int(have_rom),
        message.c_str(), folder.c_str(), reinterpret_cast<int32_t*>(&state));
    while (state.load() == 0) {
        emscripten_futex_wait(&state, 0, INFINITY);
    }
    return state.load();
}

// Copies a file whole. A page's files are in the page's thread's file system, and
// every read or write from another thread waits for that thread: libc++'s
// copy_file goes through fstreams a few KB at a time, thousands of round trips
// for the ROM, and the launcher presents nothing meanwhile. So it is read in
// 4 MB pieces and written in one.
bool CopyWholeFile(const std::filesystem::path& from, const std::filesystem::path& to) {
    const int in = open(from.c_str(), O_RDONLY);
    struct stat st;
    if (in < 0 || fstat(in, &st) != 0) {
        if (in >= 0) close(in);
        return false;
    }
    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(st.st_size));
    std::size_t done = 0;
    while (done < bytes.size()) {
        const ssize_t got = read(in, bytes.data() + done,
            std::min<std::size_t>(bytes.size() - done, 4 << 20));
        if (got <= 0) break;
        done += static_cast<std::size_t>(got);
    }
    close(in);
    if (done != bytes.size()) return false;
    const int out = open(to.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (out < 0) return false;
    done = 0;
    while (done < bytes.size()) {
        const ssize_t put = write(out, bytes.data() + done, bytes.size() - done);
        if (put <= 0) break;
        done += static_cast<std::size_t>(put);
    }
    close(out);
    return done == bytes.size();
}

// A ROM the page can play: the US v1.0 one, the only payload it is built with.
bool Playable(const std::filesystem::path& path, std::string& error) {
    dkr::runtime::rom::Identity identity{};
    if (!dkr::runtime::ValidateRomForLauncher(path, identity, error)) {
        return false;
    }
    if (identity.revision != dkr::runtime::rom::Revision::UsV77) {
        error = "This page plays the US version 1.0 ROM of Diddy Kong Racing; "
                "Rev A (v1.1) isn't built into it.";
        return false;
    }
    return true;
}

} // namespace

std::filesystem::path dkr::runtime::web::choose_rom(const std::filesystem::path& config_directory) {
    const std::filesystem::path kept = config_directory / "page-rom.z64";
    const std::string picked_folder = "/tmp/dkr-picked";
    std::string message;
    for (;;) {
        std::error_code ec;
        std::string error;
        const bool have_rom = std::filesystem::is_regular_file(kept, ec) && Playable(kept, error);
        if (Ask(have_rom, message, picked_folder) == 1) {
            return kept;
        }
        const std::filesystem::path picked = std::filesystem::path(picked_folder) / "rom";
        if (Playable(picked, error)) {
            std::filesystem::create_directories(config_directory, ec);
            const bool copied = CopyWholeFile(picked, kept);
            message = copied ? std::string() : "The ROM could not be kept in this browser.";
            if (!copied) {
                std::filesystem::remove(picked, ec);
                continue;
            }
        } else {
            message = error;
        }
        std::filesystem::remove(picked, ec);
        std::fprintf(stderr, "[boot][page] %s\n", message.empty() ? "ROM kept" : message.c_str());
    }
}
