#!/bin/bash
# LUCID - One-step build for macOS: AU (Logic Pro), VST3 and Standalone.
# Usage:  ./build-mac.sh            (Release build, installs into ~/Library/Audio/Plug-Ins)
#         ./build-mac.sh --validate (additionally runs Apple's AU validation)
set -euo pipefail
cd "$(dirname "$0")"

if ! xcode-select -p >/dev/null 2>&1; then
    echo "Xcode Command Line Tools fehlen. Installiere sie mit:  xcode-select --install"; exit 1
fi
if ! command -v cmake >/dev/null 2>&1; then
    if command -v brew >/dev/null 2>&1; then brew install cmake; else
        echo "CMake fehlt. Installiere Homebrew (https://brew.sh) und dann:  brew install cmake"; exit 1
    fi
fi

echo "==> Konfiguriere (JUCE wird beim ersten Mal heruntergeladen) ..."
cmake -B build -DCMAKE_BUILD_TYPE=Release -DLUCID_BUILD_TESTS=OFF
echo "==> Baue AU / VST3 / Standalone ..."
cmake --build build --config Release -j "$(sysctl -n hw.ncpu)"

echo
echo "Fertig. Installiert nach:"
echo "  ~/Library/Audio/Plug-Ins/Components/LUCID.component   (Logic Pro)"
echo "  ~/Library/Audio/Plug-Ins/VST3/LUCID.vst3"
echo "  build/LucidSynth_artefacts/Release/Standalone/LUCID.app"

if [[ "${1:-}" == "--validate" ]]; then
    echo; echo "==> AU-Validierung ..."
    auval -v aumu Lcd1 Lcda
fi
echo
echo "Logic Pro neu starten. Falls LUCID nicht erscheint: Logic Pro > Einstellungen > Plug-in-Manager > 'Zurücksetzen & erneut scannen'."
