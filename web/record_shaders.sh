#!/usr/bin/env bash
# Records the page's prebuilt shader list, web/rt64_shaders.txt, from a page built without it or with an older one:
#
#   bash web/record_shaders.sh <build dir> <US v1.0 ROM> [record_shaders.mjs options: --only <section>,... --port <n>]
#
# N64Recomp-browser's tools/record_shaders.mjs opens the page with ?record-shaders in headless Chrome (node and Chrome
# on PATH or where it looks) and plays web/shader_plan.json: a page load per map, each reached by web/shader_survey.js
# through the game's own teleport, since a page records only what it draws. Maps of one world are one section of the
# list (sections that share a name). It takes about an hour. Then rebuild the page: the list is specialized at build
# time (RT64_PREBUILT_SHADER_LIST in runtime-recomp/CMakeLists.txt).
set -euo pipefail
root="$(cd "$(dirname "$0")/.." && pwd)"
[[ $# -ge 2 ]] || { sed -n 4p "$0"; exit 2; }
build="$1" rom="$2"
shift 2
node "$root/lib/N64Recomp-browser/tools/record_shaders.mjs" "$root/web/shader_plan.json" "$build" "$rom" \
    "$root/web/rt64_shaders.txt" "$@"
