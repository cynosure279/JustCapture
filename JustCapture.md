JustCapture Common：JustShot / JustRecord 公共基础设施设计草案

1. 定位

"JustCapture" 是 JustShot 与 JustRecord 共用的底层库，不是独立应用，也不运行后台服务。

目标：

JustShot ─────┐
              ├── libjustcapture
JustRecord ───┘

负责：

- XDG Desktop Portal 通用访问
- Portal Request / Session 生命周期辅助
- 能力探测
- Wayland/桌面环境能力抽象
- 公共错误模型
- 输出路径与文件命名
- 通用异步任务辅助

不负责：

- 图像编辑
- 视频编码
- PipeWire 视频消费
- 音频
- GStreamer
- 录制状态机
- GTK 页面
- Phosh Quick Setting
- 后台 daemon

---

2. 总体责任划分

libjustcapture
│
├── Portal
│   ├── Screenshot protocol wrapper
│   ├── ScreenCast protocol wrapper
│   ├── Request lifecycle
│   └── Capability detection
│
├── Output
│   ├── XDG directories
│   ├── filename generation
│   └── collision avoidance
│
├── Environment
│   ├── desktop/session detection
│   └── backend availability
│
└── Common
    ├── errors
    ├── async helpers
    └── shared data types

产品层：

JustShot
├── 截图业务
├── 图片处理
├── 图片保存
├── GTK UI
└── Phosh 集成

JustRecord
├── 录制 Session
├── PipeWire
├── Audio
├── GStreamer
├── Encoder/Muxer
├── 后台 service
├── GTK UI
└── Phosh 集成

---

3. Portal 层

Screenshot

封装：

org.freedesktop.portal.Screenshot

提供：

query_screenshot_capabilities()
request_screenshot()
cancel_request()

当前 Screenshot Portal v3 可以公开：

- Screen
- Window
- Area
- Active Window

因此由公共层查询 "AvailableTargets"，JustShot 根据返回结果决定显示什么。

公共层只返回：

ScreenshotResult
├── URI
├── target
└── metadata

不负责把图片变成 PNG/JPEG，也不负责编辑。

---

ScreenCast

封装：

org.freedesktop.portal.ScreenCast

负责协议流程：

CreateSession
    ↓
SelectSources
    ↓
Start
    ↓
OpenPipeWireRemote

返回：

ScreenCastSession
├── portal session handle
├── PipeWire FD
├── stream descriptors
└── restore token

但 不连接 PipeWire、不读取 frame。

这些属于 JustRecord。

ScreenCast Portal 当前为 v6；v6 开始应优先根据 "pipewire-serial" / "PW_KEY_TARGET_OBJECT" 连接 stream，而不是长期依赖可能被复用的 node ID。

---

4. Capability 模型

不要让产品代码自己读取 Portal property。

提供统一对象：

JustCaptureCapabilities

Screenshot:
    screen
    window
    area
    active_window

ScreenCast:
    monitor
    window
    virtual
    cursor_hidden
    cursor_embedded
    cursor_metadata

例如：

JustShot
    ↓
caps.screenshot.area == true
    ↓
显示“区域截图”

JustRecord
    ↓
caps.screencast.window == false
    ↓
隐藏“录制窗口”

不要假设某个 compositor 必然实现所有能力。

---

5. Portal Request 生命周期

公共层统一处理：

创建 Request
    ↓
等待 Response
    ↓
SUCCESS / CANCELLED / FAILED
    ↓
释放 D-Bus object

并负责：

- cancellation
- D-Bus service 消失
- malformed result
- timeout policy
- GCancellable
- session cleanup

产品层不应重复实现这些代码。

统一错误：

JUST_CAPTURE_ERROR_CANCELLED
JUST_CAPTURE_ERROR_NOT_SUPPORTED
JUST_CAPTURE_ERROR_PERMISSION_DENIED
JUST_CAPTURE_ERROR_PORTAL_UNAVAILABLE
JUST_CAPTURE_ERROR_PROTOCOL
JUST_CAPTURE_ERROR_IO

“用户取消”必须与真正错误区分。

---

6. Output 公共层

公共层只负责生成合理的目标位置。

例如：

Pictures/Screenshots/
Videos/Screen Recordings/

以及：

Screenshot_2026-08-24_02-30-14.png
ScreenRecording_2026-08-24_02-30-14.mp4

提供：

make_output_path()
ensure_output_directory()
make_unique_filename()

不负责

JustShot 自己负责：

Portal URI
→ decode/copy/edit/export
→ final image

JustRecord 自己负责：

temporary recording
→ mux finalize
→ atomic finalization
→ final video

因为两种文件生命周期完全不同。

---

7. 不共享持久化设置

不要创建一个巨大的：

org.just.capture

保存所有设置。

应该：

org.just.JustShot
org.just.JustRecord

各自管理配置。

公共库只提供类型和 helper，不拥有用户 preference。

---

8. 不共享 UI

第一版不要创建：

libjustcapture-ui

JustShot 和 JustRecord 虽然视觉风格相同，但交互不同。

真正出现大量重复以后再抽：

libjustui

不要因为“看起来类似”提前形成复杂公共组件。

---

9. Phosh 集成也不属于 Common

结构必须是：

JustShot Phosh Plugin
        ↓
JustShot API

JustRecord Phosh Plugin
        ↓
JustRecord Service

而不是：

Phosh Plugin
    ↓
libjustcapture
    ↓
直接录制

Quick Setting 是产品的客户端，不拥有 capture pipeline。

Phosh 当前 "PhoshQuickSetting" 原生支持 "status-page"，可以用它提供额外操作页面。

---

10. 建议代码结构

libjustcapture/
├── portal/
│   ├── portal-client.c
│   ├── portal-request.c
│   ├── screenshot-portal.c
│   └── screencast-portal.c
│
├── output/
│   ├── output-path.c
│   └── filename.c
│
├── environment/
│   └── capabilities.c
│
├── common/
│   ├── errors.c
│   └── types.c
│
└── include/
    └── justcapture/

依赖尽量限制在：

- GLib
- GIO
- GObject

不要让公共库依赖 GTK、PipeWire 或 GStreamer。

---

11. API 稳定策略

早期不要急着承诺永久 ABI。

建议：

JustShot MVP
       +
JustRecord MVP
       ↓
验证公共接口
       ↓
libjustcapture 0.1
       ↓
稳定后再发布 ABI

可以先作为两个项目共同使用的内部 library/subproject。

---

12. 最重要的边界

JustCapture owns

«“怎样安全、统一地与桌面捕获基础设施通信？”»

JustShot owns

«“怎样完成一张截图？”»

JustRecord owns

«“怎样完成一次录屏？”»

任何功能不知道放在哪里时，用这三个问题判断。

---

13. MVP 验收

公共层第一阶段只要求：

1. 能检测 Screenshot Portal。
2. 能读取截图 targets。
3. 能完成 Screenshot Request。
4. 能建立 ScreenCast Portal Session。
5. 能返回 PipeWire FD 和 stream metadata。
6. 正确处理用户取消。
7. 正确清理 Portal Request/Session。
8. 能生成安全、不冲突的输出路径。
9. 不依赖 GTK。
10. 不依赖 PipeWire/GStreamer。

JustCapture 到 PipeWire FD 为止；从 PipeWire FD 开始，就是 JustRecord 的责任。