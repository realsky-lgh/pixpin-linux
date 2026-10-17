# PixPin Linux 📌

一款专为 Linux (Ubuntu / Debian 等) 打造的 **PixPin** 原生复刻版，采用 **C++ + Qt 6 + Tesseract OCR** 纯原生构建，具备极致轻量、零延迟启动、深度适配 **Ubuntu Wayland** 与 X11 显示架构等特点。

---

## 🌟 核心特性与功能复刻

1. **Wayland / X11 双架构截屏**
   - 完美适配 Ubuntu 默认的 **GNOME Wayland** 环境（通过 `XDG Desktop Portal` 异步安全总线）及传统 X11 环境。
   - 毫秒级快速唤起全屏遮罩，告别 Wayland 截图黑屏或权限拒绝问题。

2. **智能选区与多功能放大镜（Loupe）**
   - 框选时实时跟随高倍放大镜，显示中心十字准星。
   - 精确显示像素级坐标 `(X, Y)`、RGB 颜色值及标准 HEX 色值（如 `#007AFF`），支持颜色吸取。
   - 8 锚点自由缩放控制与选区整体拖拽移动。

3. **核心精髓：贴图功能（Pin to Desktop）**
   - 点击工具栏 📌 或快捷键 `Ctrl+P`，选区直接脱离生成无边框置顶贴图窗口。
   - **平滑缩放**：鼠标滚轮随意放大/缩小（0.2x ~ 5.0x），采用双线性插值算法保持清晰度。
   - **透明度调节**：按住 `Ctrl + 滚轮` 调节贴图半透明度（20% ~ 100%）。
   - **交互操作**：左键拖拽吸附、双击关闭贴图、右键丰富上下文菜单（复制、保存、文字提取、翻转等）。

4. **纯本地离线文字识别（Instant OCR）**
   - 采用 Tesseract C++ 原生在内存中直接推理（支持中文简体 `chi_sim` 与英文 `eng`）。
   - 截屏或贴图上随时一键触发，自动弹出优雅的暗黑风格提取面板，支持字符统计、一键复制全部文本、保存为 `.txt`。

5. **全套标注与涂鸦工具（Annotations）**
   - 矩形（Rounded Rect）、椭圆（Ellipse）、导向箭头（Arrow）、自由涂鸦画笔（Pen）。
   - **递增序号气球**（Numbered Step 1, 2, 3...）。
   - **实时马赛克 / 模糊**（Mosaic Block 局部像素化处理）。
   - 文本输入与动态选色板（红、黄、绿、蓝、白、黑及粗细调节）。
   - 完善的历史撤销栈（`Ctrl+Z`）。

---

## 🚀 启动与全局快捷键配置

### 命令行唤起
```bash
# 启动常驻系统托盘
pixpin

# 立即开始截屏（推荐绑定到全局快捷键）
pixpin -c
# 或
pixpin --capture

# 贴出当前剪贴板中的图片
pixpin -p
# 或
pixpin --pin
```

### Ubuntu GNOME 绑定全局快捷键（如 `Ctrl + 1` 或 `Ctrl + Alt + A`）
1. 打开系统设置：**设置 (Settings)** -> **键盘 (Keyboard)** -> **键盘快捷键 (Keyboard Shortcuts)** -> **查看及自定义快捷键 (View and Customize Shortcuts)**。
2. 滑到最底部，点击 **自定义快捷键 (Custom Shortcuts)** -> **+** 号添加：
   - **名称**：`PixPin 截屏`
   - **命令**：`pixpin -c`
   - **快捷键**：按下 `Ctrl + 1` 或 `Ctrl + Alt + A`
3. 再添加一个贴图快捷键（可选）：
   - **名称**：`PixPin 贴图`
   - **命令**：`pixpin -p`
   - **快捷键**：按下 `Ctrl + 2`
