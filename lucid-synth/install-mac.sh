#!/bin/bash
# LUCID - installs the downloaded AU/VST3 into the user plug-in folders and removes the
# macOS quarantine flag (needed because the build is not notarised).
# Run from the unpacked LUCID-macOS folder:   bash install-mac.sh
set -e
cd "$(dirname "$0")"
if [ ! -d "AU/LUCID.component" ]; then echo "AU/LUCID.component nicht gefunden. Bitte im entpackten LUCID-macOS-Ordner ausführen."; exit 1; fi
mkdir -p ~/Library/Audio/Plug-Ins/Components ~/Library/Audio/Plug-Ins/VST3
xattr -dr com.apple.quarantine AU VST3 Standalone 2>/dev/null || true
rm -rf ~/Library/Audio/Plug-Ins/Components/LUCID.component ~/Library/Audio/Plug-Ins/VST3/LUCID.vst3
cp -R AU/LUCID.component ~/Library/Audio/Plug-Ins/Components/
cp -R VST3/LUCID.vst3 ~/Library/Audio/Plug-Ins/VST3/
killall -9 AudioComponentRegistrar 2>/dev/null || true
echo "Installiert. AU-Validierung:"
auval -v aumu Lcd1 Lcda 2>&1 | tail -4
echo
echo "Jetzt Logic Pro neu starten. Falls LUCID fehlt: Logic Pro > Einstellungen > Plug-in-Manager > Zurücksetzen & erneut scannen."
