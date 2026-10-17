#!/usr/bin/env bash
# PixPin Linux Uninstaller
set -e

if [ "$EUID" -eq 0 ]; then
    PREFIX="/usr/local"
else
    PREFIX="$HOME/.local"
fi

echo "=========================================="
echo " 卸载 PixPin Linux (目标: $PREFIX)"
echo "=========================================="

rm -f "$PREFIX/bin/pixpin"
rm -f "$PREFIX/share/applications/pixpin.desktop"
rm -f "$PREFIX/share/icons/hicolor/scalable/apps/pixpin.svg"
rm -rf "$PREFIX/share/pixpin"

if command -v update-desktop-database >/dev/null 2>&1; then
    update-desktop-database "$PREFIX/share/applications" >/dev/null 2>&1 || true
fi
if command -v gtk-update-icon-cache >/dev/null 2>&1; then
    gtk-update-icon-cache -f -t "$PREFIX/share/icons/hicolor" >/dev/null 2>&1 || true
fi

echo "✅ PixPin 已成功卸载！"
