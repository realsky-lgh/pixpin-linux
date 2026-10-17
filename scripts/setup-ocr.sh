#!/usr/bin/env bash
# PixPin Linux - RapidOCR Setup Helper
set -e

INSTALL_DIR="$HOME/.local/share/pixpin"
VENV_DIR="$INSTALL_DIR/venv"

echo "=== PixPin Linux - RapidOCR (PaddleOCR/ONNX) 安装工具 ==="

mkdir -p "$INSTALL_DIR"

if ! command -v python3 >/dev/null 2>&1; then
    echo "错误: 未找到 python3，请先安装 Python 3 (例如: sudo apt install python3 python3-venv)"
    exit 1
fi

if [ ! -d "$VENV_DIR" ]; then
    echo "正在创建虚拟环境: $VENV_DIR ..."
    python3 -m venv "$VENV_DIR" || {
        echo "创建虚拟环境失败，请确认已安装 python3-venv (例如: sudo apt install python3-venv)"
        exit 1
    }
fi

echo "正在安装 rapidocr_onnxruntime 及依赖 ..."
"$VENV_DIR/bin/pip" install --upgrade pip
"$VENV_DIR/bin/pip" install rapidocr_onnxruntime

# Copy ocr_worker.py if not present
SCRIPT_SRC="$(dirname "$0")/../src/ocr/ocr_worker.py"
if [ -f "$SCRIPT_SRC" ]; then
    cp -f "$SCRIPT_SRC" "$INSTALL_DIR/ocr_worker.py"
elif [ -f "/usr/share/pixpin/ocr_worker.py" ]; then
    cp -f "/usr/share/pixpin/ocr_worker.py" "$INSTALL_DIR/ocr_worker.py"
fi

echo "=== 安装完成！PixPin 现已支持基于 PaddleOCR ONNX 的高精度离线文字识别 ==="
