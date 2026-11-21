#!/bin/bash
# Compila MonoCheck.vst3 y lo instala en tu Mac.
# Uso:  ./install_plugin.sh
set -euo pipefail
cd "$(dirname "$0")"

echo "==> Compilando (instala automáticamente al terminar)..."
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release -j "$(sysctl -n hw.ncpu)"

VST3="$HOME/Library/Audio/Plug-Ins/VST3/MonoCheck.vst3"

echo "==> Firmando (ad-hoc)..."
codesign --force --deep --sign - "$VST3"

echo "==> Instalado en: $VST3"
echo "Reinicia tu DAW y vuelve a escanear plugins."
