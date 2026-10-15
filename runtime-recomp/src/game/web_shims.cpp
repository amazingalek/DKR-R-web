// What the page links in place of what Emscripten's libc and the page lack, built only with Emscripten
// (CMakeLists.txt), so the desktop's code is unchanged:
// - nativefiledialog: the page draws none of DKR-R's ImGui windows (runtime_ui.cpp), so no dialog is ever asked for;
//   each one fails as a dialog the platform can't open does.
// - sysinfo: declared by Emscripten's headers but not in its libc. It fails, so the system summary says "Unknown
//   memory" and the online resource cache keeps nothing, as their own error paths do.
// - getrandom: likewise; getentropy is the browser's crypto.getRandomValues, 256 bytes a call.
#include <nfd.h>

#include <algorithm>
#include <cerrno>
#include <cstddef>
#include <sys/random.h>
#include <sys/sysinfo.h>
#include <unistd.h>

namespace {
const char* const kNoDialogs = "A page has no file dialogs.";

nfdresult_t NoDialog(nfdu8char_t** out) {
    if (out) *out = nullptr;
    return NFD_ERROR;
}
} // namespace

extern "C" {
nfdresult_t NFD_Init() { return NFD_OKAY; }
void NFD_Quit() {}
const char* NFD_GetError() { return kNoDialogs; }
void NFD_ClearError() {}
void NFD_FreePathU8(nfdu8char_t*) {}
nfdresult_t NFD_OpenDialogU8(nfdu8char_t** out, const nfdu8filteritem_t*, nfdfiltersize_t, const nfdu8char_t*) {
    return NoDialog(out);
}
nfdresult_t NFD_SaveDialogU8(nfdu8char_t** out, const nfdu8filteritem_t*, nfdfiltersize_t, const nfdu8char_t*,
                             const nfdu8char_t*) {
    return NoDialog(out);
}
nfdresult_t NFD_PickFolderU8(nfdu8char_t** out, const nfdu8char_t*) { return NoDialog(out); }

int sysinfo(struct sysinfo*) {
    errno = ENOSYS;
    return -1;
}

ssize_t getrandom(void* buffer, size_t length, unsigned) {
    length = std::min<size_t>(length, 256);
    return getentropy(buffer, length) == 0 ? ssize_t(length) : -1;
}
}
