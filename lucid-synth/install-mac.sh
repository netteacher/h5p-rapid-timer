#!/bin/bash
# LUCID - installs the downloaded AU/VST3 into the user plug-in folders and removes the
# macOS quarantine flag (needed because the build is not notarised).
# Run from the unpacked LUCID-macOS folder:   bash install-mac.sh
set -e
cd "$(dirname "$0")"
if [ ! -d "AU/LUCID.component" ]; then echo "AU/LUCID.component nicht gefunden. Bitte im entpackten LUCID-macOS-Ordner ausführen."; exit 1; fi
mkdir -p ~/Library/Audio/Plug-Ins/Components ~/Library/Audio/Plug-Ins/VST3
xattr -dr com.apple.quarantine AU VST3 Standalone 2>/dev/null || true
for c in AU/*.component; do
    rm -rf ~/Library/Audio/Plug-Ins/Components/"$(basename "$c")"
    cp -R "$c" ~/Library/Audio/Plug-Ins/Components/
done
for v in VST3/*.vst3; do
    rm -rf ~/Library/Audio/Plug-Ins/VST3/"$(basename "$v")"
    cp -R "$v" ~/Library/Audio/Plug-Ins/VST3/
done
killall -9 AudioComponentRegistrar 2>/dev/null || true
echo "Installiert: $(ls AU | tr '\n' ' ')"
echo "AU-Validierung:"
auval -v aumu Lcd1 Lcda 2>&1 | tail -2     # LUCID (Synth)
auval -v aumu Lpu1 Lcda 2>&1 | tail -2     # LUCID Pulse (Drum-Instrument)
auval -v aumi Lpm1 Lcda 2>&1 | tail -2     # LUCID Pulse MIDI (MIDI-Effekt)
echo
echo "Jetzt Logic Pro neu starten. Falls LUCID fehlt: Logic Pro > Einstellungen > Plug-in-Manager > Zurücksetzen & erneut scannen."
