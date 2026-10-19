#!/usr/bin/env bash
# PixPin Linux Portable Installer
set -e

DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

if [ "$EUID" -eq 0 ]; then
    PREFIX="/usr/local"
    BIN_DIR="$PREFIX/bin"
    APP_DIR="$PREFIX/share/applications"
    ICON_DIR="$PREFIX/share/icons/hicolor/scalable/apps"
    SHARE_DIR="$PREFIX/share/pixpin"
else
    PREFIX="$HOME/.local"
    BIN_DIR="$PREFIX/bin"
    APP_DIR="$PREFIX/share/applications"
    ICON_DIR="$PREFIX/share/icons/hicolor/scalable/apps"
    SHARE_DIR="$PREFIX/share/pixpin"
fi

echo "=========================================="
echo " 安装 PixPin Linux 到: $PREFIX"
echo "=========================================="

mkdir -p "$BIN_DIR" "$APP_DIR" "$ICON_DIR" "$SHARE_DIR"

# Install binary
install -m 755 "$DIR/bin/pixpin" "$BIN_DIR/pixpin"

# Install desktop file
install -m 644 "$DIR/share/applications/pixpin.desktop" "$APP_DIR/pixpin.desktop"

# Install icon
install -m 644 "$DIR/share/icons/hicolor/scalable/apps/pixpin.svg" "$ICON_DIR/pixpin.svg"

# Install OCR worker script
install -m 755 "$DIR/share/pixpin/ocr_worker.py" "$SHARE_DIR/ocr_worker.py"

# Install tessdata (Traditional & Simplified Chinese, English)
if [ -d "$DIR/share/pixpin/tessdata" ]; then
    mkdir -p "$SHARE_DIR/tessdata"
    cp -rf "$DIR/share/pixpin/tessdata/"* "$SHARE_DIR/tessdata/"
fi

# Update desktop database and icon caches if available
if command -v update-desktop-database >/dev/null 2>&1; then
    update-desktop-database "$APP_DIR" >/dev/null 2>&1 || true
fi
if command -v gtk-update-icon-cache >/dev/null 2>&1; then
    gtk-update-icon-cache -f -t "$PREFIX/share/icons/hicolor" >/dev/null 2>&1 || true
fi

echo ""
echo "✅ PixPin 安装成功！"
echo "程序路径: $BIN_DIR/pixpin"
echo ""
echo "快速使用:"
echo "  pixpin -c   # 截屏"
echo "  pixpin -p   # 贴图"
echo ""
if [ "$EUID" -ne 0 ]; then
    echo "提示: 请确保 $BIN_DIR 已在您的 PATH 环境变量中 (通常 ~/.profile 或 ~/.bashrc 默认包含)。"
fi
