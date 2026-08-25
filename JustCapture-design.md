# JustCapture 技术方案
> 基于 JustCapture/JustCapture.md 的具体实现设计

---

## 1. 构建系统

### 1.1 项目元信息

| 项目 | 值 |
|------|-----|
| 包名 | `libjustcapture` |
| 版本 | `0.1.0`（MVP 阶段，不承诺 ABI） |
| 许可证 | LGPL-2.1+ |
| 最低依赖 | GLib ≥ 2.80, GIO ≥ 2.80, GObject |

### 1.2 Meson 顶层

```meson
project('libjustcapture', 'c',
  version: '0.1.0',
  meson_version: '>= 1.3.0',
  license: 'LGPL-2.1+',
  default_options: ['warning_level=2', 'c_std=c17'])

# 符号可见性：默认 hidden，导出宏
add_project_arguments('-DGLIB_VERSION_MIN_REQUIRED=GLIB_VERSION_2_80',
                      '-DGLIB_VERSION_MAX_ALLOWED=GLIB_VERSION_2_80',
                      '-DG_LOG_DOMAIN="JustCapture"',
                      language: 'c')

justcapture_inc = include_directories('include/')

subdir('src/')
subdir('tests/')
```

### 1.3 库目标

```meson
# src/meson.build
libjustcapture_sources = files(
  'portal/portal-client.c',
  'portal/portal-request.c',
  'portal/screenshot-portal.c',
  'portal/screencast-portal.c',
  'output/output-path.c',
  'output/filename.c',
  'environment/capabilities.c',
  'common/errors.c',
  'common/types.c',
  'common/async-helpers.c',
)

libjustcapture_headers = files(
  '../include/justcapture/justcapture.h',
  '../include/justcapture/errors.h',
  '../include/justcapture/types.h',
  '../include/justcapture/portal-client.h',
  '../include/justcapture/portal-request.h',
  '../include/justcapture/screenshot-portal.h',
  '../include/justcapture/screencast-portal.h',
  '../include/justcapture/capabilities.h',
  '../include/justcapture/output-path.h',
  '../include/justcapture/filename.h',
  '../include/justcapture/async-helpers.h',
  '../include/justcapture/version.h',
)

libjustcapture = library('justcapture',
  libjustcapture_sources,
  include_directories: [justcapture_inc],
  dependencies: [glib_dep, gio_dep, gobject_dep],
  c_args: ['-DJUSTCAPTURE_COMPILATION'],
  install: true,
  version: meson.project_version(),
  soversion: '0',
  gnu_symbol_visibility: 'hidden',
)

install_headers(libjustcapture_headers, subdir: 'justcapture/')

# pkg-config
pkg = import('pkgconfig')
pkg.generate(libjustcapture,
  description: 'Common infrastructure for JustShot and JustRecord',
  subdirs: 'justcapture')
```

### 1.4 子项目消费

JustShot 和 JustRecord 通过 `subproject('libjustcapture')` 或系统 `dependency('justcapture')` 引用，MVP 阶段优先使用 subproject。

---

## 2. 目录结构

```
libjustcapture/
├── meson.build
├── include/
│   └── justcapture/
│       ├── justcapture.h          # 伞状头文件
│       ├── version.h              # 版本宏
│       ├── errors.h               # 错误域 + 错误码
│       ├── types.h                # 公共枚举/结构体
│       ├── portal-client.h        # Portal 连接管理
│       ├── portal-request.h       # 通用 Request 生命周期
│       ├── screenshot-portal.h    # Screenshot 封装
│       ├── screencast-portal.h    # ScreenCast 封装
│       ├── capabilities.h         # 能力探测
│       ├── output-path.h          # 输出目录
│       ├── filename.h             # 文件命名 + 防冲突
│       └── async-helpers.h        # 异步辅助
├── src/
│   ├── meson.build
│   ├── portal/
│   │   ├── portal-client.c
│   │   ├── portal-request.c
│   │   ├── screenshot-portal.c
│   │   └── screencast-portal.c
│   ├── output/
│   │   ├── output-path.c
│   │   └── filename.c
│   ├── environment/
│   │   └── capabilities.c
│   └── common/
│       ├── errors.c
│       ├── types.c
│       └── async-helpers.c
├── tests/
│   ├── meson.build
│   ├── test-errors.c
│   ├── test-filename.c
│   ├── test-output-path.c
│   └── test-types.c
└── docs/ (可选)
```

---

## 3. 公共类型与错误模型

### 3.1 错误域

```c
/* include/justcapture/errors.h */

#define JUST_CAPTURE_ERROR (just_capture_error_quark ())
GQuark just_capture_error_quark (void);

typedef enum {
  JUST_CAPTURE_ERROR_CANCELLED,          /* 用户取消 Portal 对话框 */
  JUST_CAPTURE_ERROR_NOT_SUPPORTED,      /* 当前桌面环境不支持 */
  JUST_CAPTURE_ERROR_PERMISSION_DENIED,  /* 权限被拒绝 */
  JUST_CAPTURE_ERROR_PORTAL_UNAVAILABLE, /* Portal 服务不可用 */
  JUST_CAPTURE_ERROR_PROTOCOL,           /* D-Bus 协议异常 */
  JUST_CAPTURE_ERROR_IO,                 /* 文件 I/O 错误 */
  JUST_CAPTURE_ERROR_FAILED,             /* 通用失败 */
} JustCaptureError;
```

**CANCELLED 必须与真正错误区分**（md §5）。产品层收到 `JUST_CAPTURE_ERROR_CANCELLED` 时不视为错误，不提示用户。

### 3.2 枚举

```c
/* include/justcapture/types.h */

/* Screenshot Portal 目标，对应 AvailableTargets 位掩码 */
typedef enum {
  JUST_CAPTURE_SCREENSHOT_TARGET_NONE          = 0,
  JUST_CAPTURE_SCREENSHOT_TARGET_SCREEN        = 1 << 0,  /* 1 */
  JUST_CAPTURE_SCREENSHOT_TARGET_WINDOW        = 1 << 1,  /* 2 */
  JUST_CAPTURE_SCREENSHOT_TARGET_AREA          = 1 << 2,  /* 4 */
  JUST_CAPTURE_SCREENSHOT_TARGET_ACTIVE_WINDOW = 1 << 3,  /* 8 */
} JustCaptureScreenshotTarget;

/* ScreenCast 源类型，对应 AvailableSourceTypes 位掩码 */
typedef enum {
  JUST_CAPTURE_SOURCETYPE_MONITOR = 1 << 0,   /* 1 */
  JUST_CAPTURE_SOURCETYPE_WINDOW = 1 << 1,    /* 2 */
  JUST_CAPTURE_SOURCETYPE_VIRTUAL = 1 << 2,   /* 4 */
} JustCaptureSourceType;

/* ScreenCast 光标模式，对应 AvailableCursorModes 位掩码 */
typedef enum {
  JUST_CAPTURE_CURSOR_MODE_HIDDEN    = 1 << 0,  /* 1 */
  JUST_CAPTURE_CURSOR_MODE_EMBEDDED  = 1 << 1,  /* 2 */
  JUST_CAPTURE_CURSOR_MODE_METADATA  = 1 << 2,  /* 4 */
} JustCaptureCursorMode;

/* 输出类型 */
typedef enum {
  JUST_CAPTURE_OUTPUT_KIND_SCREENSHOT,
  JUST_CAPTURE_OUTPUT_KIND_SCREENCAST,
} JustCaptureOutputKind;
```

### 3.3 结构体

```c
/* Screenshot 结果 */
typedef struct {
  gchar *uri;        /* Portal 返回的图片 URI（带 document: 或 file: 前缀） */
  gint  actual_target; /* 实际使用的 target 值 */
} JustCaptureScreenshotResult;

void just_capture_screenshot_result_free (JustCaptureScreenshotResult *result);
#define just_capture_screenshot_result_cleanup (&just_capture_screenshot_result_free)

/* ScreenCast 流描述符 */
typedef struct {
  gchar *id;               /* 流唯一标识（v4+，local to session） */
  gint   node_id;          /* PipeWire node ID（v6 起 deprecated for targeting） */
  guint64 pipewire_serial; /* object.serial，用于 PW_KEY_TARGET_OBJECT（v6+） */
  gint   source_type;      /* MONITOR / WINDOW / VIRTUAL */
  gint   position_x, position_y;  /* 坐标空间位置 */
  gint   width, height;          /* 显示尺寸 */
} JustCaptureStreamDescriptor;

void just_capture_stream_descriptor_free (JustCaptureStreamDescriptor *desc);
#define just_capture_stream_descriptor_cleanup (&just_capture_stream_descriptor_free)

/* ScreenCast Session */
typedef struct {
  gchar    *session_handle;  /* Portal session handle（对象路径） */
  gint      pipewire_fd;     /* OpenPipeWireRemote 返回的 fd */
  GList    *streams;         /* GList<JustCaptureStreamDescriptor*> */
  gchar    *restore_token;   /* 下次恢复用的 token（NULL 如果不支持持久化） */
  gboolean  has_serial;      /* v6+ 是否提供了 pipewire-serial */
} JustCaptureScreenCastSession;

void just_capture_screencast_session_free (JustCaptureScreenCastSession *session);
#define just_capture_screencast_session_cleanup (&just_capture_screencast_session_free)

/* 能力探测结果 */
typedef struct {
  /* Screenshot 可用目标（位掩码） */
  JustCaptureScreenshotTarget screenshot_targets;
  /* ScreenCast 可用源类型（位掩码） */
  guint screencast_source_types;
  /* ScreenCast 可用光标模式（位掩码） */
  guint screencast_cursor_modes;
  /* Portal 版本号 */
  guint screenshot_portal_version;
  guint screencast_portal_version;
  /* 整体可用性 */
  gboolean portal_available;
} JustCaptureCapabilities;
```

### 3.4 内存管理

所有 `JustCapture*` 结构体使用 `g_autoptr()` 兼容（通过 `G_DEFINE_AUTOPTR_CLEANUP_FUNC` 或 `_cleanup_` 宏）。

---

## 4. Portal 层

### 4.1 portal-client（连接管理）

```c
/* include/justcapture/portal-client.h */
typedef struct _JustCapturePortal JustCapturePortal;

/* 获取/创建单例，延迟初始化 */
JustCapturePortal *just_capture_portal_get_default (void);

/* 异步探测 Portal 是否可用（检查 D-Bus name owner 和版本） */
void just_capture_portal_check_available_async (
    JustCapturePortal *portal,
    GCancellable      *cancellable,
    GAsyncReadyCallback callback,
    gpointer           user_data);
gboolean just_capture_portal_check_available_finish (
    JustCapturePortal *portal,
    GAsyncResult      *result,
    GError           **error);

/* 获取内部 GDBusConnection（session bus，lazy init） */
GDBusConnection *just_capture_portal_get_connection (JustCapturePortal *portal);

/* 获取 D-Bus proxy（缓存，按 interface name 复用） */
GDBusProxy *just_capture_portal_get_proxy (
    JustCapturePortal *portal,
    const gchar       *interface_name);
```

**实现细节**：
- 内部使用 `g_bus_get(G_BUS_TYPE_SESSION, ...)` 获取 session bus 连接
- 通过 `g_bus_watch_name_on_connection` 监听 `org.freedesktop.portal.Desktop` 的 name owner 变化
- proxy 缓存：`GHashTable<gchar*, GDBusProxy*>` 按 interface 名缓存
- 线程安全：所有操作通过 GMainContext 主循环调度
- 若 D-Bus 不可用或 Portal 服务消失，所有后续操作返回 `JUST_CAPTURE_ERROR_PORTAL_UNAVAILABLE`

### 4.2 portal-request（通用 Request 生命周期）

这是 md §5 的核心实现。封装一个通用模式：

```c
/* include/justcapture/portal-request.h */

/* 发送 Portal 请求并等待 Response 信号 */
void just_capture_portal_request_call (
    JustCapturePortal *portal,
    const gchar       *interface_name,   /* e.g. "org.freedesktop.portal.Screenshot" */
    const gchar       *method_name,       /* e.g. "Screenshot" */
    GVariant          *parameters,        /* 传给方法的参数 */
    gint               timeout_msec,      /* -1 用默认 */
    GCancellable      *cancellable,
    GAsyncReadyCallback callback,
    gpointer           user_data);

/* 完成回调：提取结果（成功则为 Response 信号返回的 results dict） */
GVariant *just_capture_portal_request_call_finish (
    GAsyncResult *result,
    GError      **error);
```

**内部时序**：

```
调用方                         portal-request                  D-Bus
  │                                │                             │
  │  ── request_call ──→          │                             │
  │                                │  ── CreateProxy(interface) ─→│
  │                                │  ←── proxy ready ──────────│
  │                                │  ── CallMethod(interface) ─→│
  │                                │  ←── (request_path, ...) ──│
  │                                │  ── Subscribe Response ────→│
  │                                │    on request object path   │
  │                                │                             │
  │                                │   ┌─── 等待 ────────────┐   │
  │                                │   │                      │   │
  │                                │  ←── Response(0, results) ─│ SUCCESS
  │                                │  ←── Response(1, {}) ──────│ CANCELLED
  │                                │  ←── Response(2, {error}) ─│ FAILED
  │                                │                             │
  │                                │  Unsubscribe               │
  │                                │  Drop proxy ref            │
  │  ←── callback ──────────────│                             │
```

**生命周期规则**（对应 md §5 的 bullet list）：
1. **Cancellation**：GCancellable 触发 → 调用 `org.freedesktop.portal.Request.Close` 方法 + 返回 `JUST_CAPTURE_ERROR_CANCELLED`
2. **D-Bus service 消失**：NameOwnerChanged 回调 → 返回 `JUST_CAPTURE_ERROR_PORTAL_UNAVAILABLE` + 取消所有进行中的请求
3. **Malformed result**：Response 信号缺少必要字段（如 Screenshot 缺 `uri`）→ 返回 `JUST_CAPTURE_ERROR_PROTOCOL`
4. **Timeout policy**：可配置 timeout（默认 120s 交互式，60s 非交互式）→ 超时返回 `JUST_CAPTURE_ERROR_FAILED`
5. **GCancellable**：传递自产品层
6. **Session cleanup**：ScreenCast 需要单独处理 Session 的关闭（见 4.4）

**内部数据结构**：

```c
typedef struct {
  GDBusProxy        *request_proxy;  /* 订阅 Request 接口的 proxy */
  gchar             *request_path;   /* 用于取消的路径 */
  GCancellable      *cancellable;    /* 用户 cancellable */
  gulong             signal_id;      /* Response 信号 handler id */
  gulong             name_watch_id;  /* NameOwner 监测 */
  GTask             *task;           /* 持有 GAsyncResult 回调 */
  gint               timeout_id;     /* 超时 source id */
  /* ... */
} PortalRequest;
```

### 4.3 screenshot-portal

```c
/* include/justcapture/screenshot-portal.h */

/* 查询 Screenshot 可用目标（异步） */
void just_capture_screenshot_query_targets_async (
    JustCapturePortal *portal,
    GCancellable      *cancellable,
    GAsyncReadyCallback callback,
    gpointer           user_data);
JustCaptureScreenshotTarget
  just_capture_screenshot_query_targets_finish (
    JustCapturePortal *portal,
    GAsyncResult      *result,
    GError           **error);

/* 发起截图请求 */
void just_capture_screenshot_request_async (
    JustCapturePortal           *portal,
    JustCaptureScreenshotTarget  target,   /* 0 表示不指定，由 portal 决定 */
    gboolean                     interactive,
    const gchar                 *parent_window,  /* "" 或无 */
    GCancellable                *cancellable,
    GAsyncReadyCallback          callback,
    gpointer                     user_data);
JustCaptureScreenshotResult *
  just_capture_screenshot_request_finish (
    JustCapturePortal *portal,
    GAsyncResult      *result,
    GError           **error);
```

**实现细节**（对应 Screenshot Portal v3 规范，§3.1.1 已验证）：

- **query_targets**：读取 `org.freedesktop.portal.Screenshot:AvailableTargets` property（`u` 位掩码）→ 映射到 `JustCaptureScreenshotTarget`
- **request**：调用 `Screenshot(parent_window, options)`，options 包含：
  - `handle_token`：自动生成
  - `target`（`u`）：如果 target != 0，传递对应值；如果 target 不在 AvailableTargets 中，fallback 到 interactive 模式
  - `interactive`（`b`）：根据参数设置
  - `modal`（`b`）：默认 TRUE
- **Response 结果**：提取 `uri`（`s`），构造 `JustCaptureScreenshotResult`
- 结果 URI 处理：若为 `document:` 协议，产品层通过 `org.freedesktop.portal.Documents` 解码；若为 `file:` 直接使用
- 本层**不负责**将 URI 解码为图片——这是 JustShot 的责任

### 4.4 screencast-portal

```c
/* include/justcapture/screencast-portal.h */

/* 建立完整的 ScreenCast Session */
void just_capture_screencast_session_create_async (
    JustCapturePortal    *portal,
    guint                 source_types,      /* 位掩码：MONITOR|WINDOW|VIRTUAL */
    JustCaptureCursorMode cursor_mode,       /* 光标模式 */
    gboolean              allow_multiple,    /* 是否允许多源 */
    const gchar          *restore_token,     /* NULL 或之前保存的 token */
    GCancellable         *cancellable,
    GAsyncReadyCallback   callback,
    gpointer              user_data);
JustCaptureScreenCastSession *
  just_capture_screencast_session_create_finish (
    JustCapturePortal *portal,
    GAsyncResult      *result,
    GError           **error);

/* 关闭并清理 Session */
void just_capture_screencast_session_close_async (
    JustCapturePortal      *portal,
    JustCaptureScreenCastSession *session,
    GCancellable           *cancellable,
    GAsyncReadyCallback     callback,
    gpointer                user_data);
gboolean just_capture_screencast_session_close_finish (
    JustCapturePortal *portal,
    GAsyncResult      *result,
    GError           **error);
```

**内部实现**（对应 ScreenCast v6 规范，§3.1.2 已验证）：

```
                  screencast-portal                     D-Bus
                        │                                 │
  1. CreateSession()    │  ── CreateSession(options) ────→│
                        │  ←── {session_handle} ──────────│
                        │                                 │
  2. SelectSources()    │  ── SelectSources(session,      │
                        │       options: types,multiple,  │
                        │         cursor_mode,restore) ──→│
                        │  ←── Response ─────────────────│
                        │                                 │
  3. Start()            │  ── Start(session, parent,      │
                        │          options) ────────────→│
                        │  ←── {streams, restore_token} ─│
                        │                                 │
  4. OpenPipeWireRemote │  ── OpenPipeWireRemote(session)─→│
                        │  ←── fd (h) ───────────────────│
                        │                                 │
                        │  组装 JustCaptureScreenCastSession
                        │  {session_handle, fd, streams, restore_token}
```

**关键实现要点**：

- **v6 stream targeting**：从 Start 返回的 stream properties 中优先读取 `pipewire-serial`（`t`），设置 `has_serial=TRUE`；若 v6 以下则使用 node ID。JustRecord 消费时用 `PW_KEY_TARGET_OBJECT = pipewire-serial` 连接
- **Session 管理**：使用 `org.freedesktop.portal.Session` 接口管理生命周期；监听 `Session::Closed` 信号处理 compositor 主动关闭
- **Restore 流程**：SelectSources 时传递 `restore_token`（如果非 NULL）；Start 返回新的 `restore_token` 供下次使用
- **Cleanup**：关闭 Session（`Session.Close`）+ 关闭 fd（`close()`）+ 释放所有资源
- **权限**：SelectSources 不做权限决定——Start 弹出对话框让用户确认

### 4.5 关于 portal 失败的处理

- 所有 async 函数都接受 `GCancellable`，产品层可随时取消
- 用户取消 Portal 对话框 → `JUST_CAPTURE_ERROR_CANCELLED`（不严重）
- Portal 服务不可用（未安装、未运行）→ `JUST_CAPTURE_ERROR_PORTAL_UNAVAILABLE`
- Portal 返回的 response 码 2 + error 信息 → `JUST_CAPTURE_ERROR_PERMISSION_DENIED` 或 `JUST_CAPTURE_ERROR_PROTOCOL`
- Compositor 不支持 ScreenCast → AvailableSourceTypes 为空

---

## 5. Capability 模型

```c
/* include/justcapture/capabilities.h */

/* 异步探测能力 */
void just_capture_capabilities_query_async (
    JustCapturePortal *portal,
    GCancellable      *cancellable,
    GAsyncReadyCallback callback,
    gpointer           user_data);
JustCaptureCapabilities *
  just_capture_capabilities_query_finish (
    JustCapturePortal *portal,
    GAsyncResult      *result,
    GError           **error);
void just_capture_capabilities_free (JustCaptureCapabilities *caps);
```

**能力来源**：

| 字段 | 来源 |
|------|------|
| `screenshot_targets` | `org.freedesktop.portal.Screenshot:AvailableTargets` (property `u`) |
| `screencast_source_types` | `org.freedesktop.portal.ScreenCast:AvailableSourceTypes` (property `u`) |
| `screencast_cursor_modes` | `org.freedesktop.portal.ScreenCast:AvailableCursorModes` (property `u`, v2+) |
| `screenshot_portal_version` | `org.freedesktop.portal.Screenshot:version` (property `u`) |
| `screencast_portal_version` | `org.freedesktop.portal.ScreenCast:version` (property `u`) |
| `portal_available` | `g_bus_watch_name` 检查 `org.freedesktop.portal.Desktop` 是否存在 |

**实现**：先检查 portal_available，若不可用则所有能力返回 0；然后并行读取 4 个 property（通过 4 个 D-Bus property get 调用，在 `GTask` 内部用 `g_task_run_in_thread` 或异步 `g_dbus_proxy_call` 并行）。结果缓存 30 秒。

**产品层使用示例**：

```c
/* JustShot 中 */
JustCaptureCapabilities *caps = ...;
if (caps->screenshot_targets & JUST_CAPTURE_SCREENSHOT_TARGET_AREA)
    show_area_button ();    /* 显示"区域截图"按钮（md §4） */
```

---

## 6. Output 层

### 6.1 输出目录

```c
/* include/justcapture/output-path.h */

/* 获取标准截图目录 Pictures/Screenshots/ */
gchar *just_capture_output_path_get_screenshots_dir (void);

/* 获取标准录屏目录 Videos/Screen Recordings/ */
gchar *just_capture_output_path_get_recordings_dir (void);

/* 确保目录存在（g_mkdir_with_parents 0755） */
gboolean just_capture_output_path_ensure_dir (
    const gchar *dir,
    GError     **error);

/* 根据输出类型获取完整路径（含文件名） */
gchar *just_capture_output_path_make (
    JustCaptureOutputKind  kind,
    const gchar           *filename);
```

**实现**：
- 使用 `g_get_user_special_dir(G_USER_DIRECTORY_PICTURES)` 和 `G_USER_DIRECTORY_VIDEOS`
- 回退：`g_get_home_dir() / "Pictures"` 或 `"Videos"`
- 子目录名硬编码为 `"Screenshots"` 和 `"Screen Recordings"`
- 异步变体（GTask）可提供，但 I/O 轻量，同步即可

### 6.2 文件名生成

```c
/* include/justcapture/filename.h */

/* 生成标准文件名（不含目录） */
gchar *just_capture_filename_make_screenshot (GDateTime *timestamp);
gchar *just_capture_filename_make_recording (GDateTime *timestamp);

/* 防冲突：在目录中生成不存在的文件名 */
gchar *just_capture_filename_make_unique (
    const gchar *directory,
    const gchar *basename,     /* 如 "Screenshot_2026-08-24_02-30-14" */
    const gchar *extension);   /* 如 ".png" */
```

**格式**（md §6）：

```
Screenshot_2026-08-24_02-30-14.png
ScreenRecording_2026-08-24_02-30-14.mp4
```

- 时间戳：`g_date_time_format(timestamp, "%Y-%m-%d_%H-%M-%S")`，本地时区
- 防冲突：若文件已存在，追加 `_1`、`_2` 后缀，循环直到找到空位（最多 1000 次，超过返回错误）
- 扩展名由产品层传入（JustShot 传 `".png"`，JustRecord 传 `".mp4"`）

---

## 7. Async Helpers

```c
/* include/justcapture/async-helpers.h */

/* 在默认 GMainContext 上运行异步任务 */
void just_capture_async_run_in_thread (
    GTask               *task,
    GTaskThreadFunc      func,
    gpointer             source_object,
    GCancellable        *cancellable);

/* 将同步操作包装为异步（用于内部 thread pool） */
void just_capture_async_wrap_sync (
    GTask              *task,
    GTaskThreadFunc     sync_func,
    gpointer            source_object,
    GCancellable       *cancellable);
```

内部使用 `g_task_run_in_thread` 或自定义 `GThreadPool`。MVP 阶段直接使用 `g_task_run_in_thread`。

---

## 8. 依赖树

```
libjustcapture (静态/动态库)
├── GLib       (GMainLoop, GTask, GDateTime, GHashTable, GList, 字符串)
├── GIO        (GDBusConnection, GDBusProxy, GDBusMessage, GFile*)
├── GObject    (GObject 基类, GSignal, GParamSpec)
└── POSIX      (close, dup, 文件 I/O — 仅 fd 操作)
```

**不依赖**：GTK、PipeWire、GStreamer、libadwaita（md §10 约束）。

---

## 9. API 稳定策略（md §11）

| 阶段 | 策略 |
|------|------|
| MVP | 内部 subproject，头文件可能改，不装系统 |
| 0.1 | 安装到系统，但 soversion 不保证 |
| 1.0 | 冻结 ABI，symbol versioning，deprecation 宏 |

MVP 阶段所有符号默认 `hidden`，仅显式导出的公共 API 可见。

---

## 10. 测试策略

### 单元测试（`meson test`）

| 测试 | 内容 |
|------|------|
| `test-errors` | 错误域 quark、错误码映射、GError 构造 |
| `test-filename` | 文件名格式、防冲突、扩展名处理 |
| `test-output-path` | XDG 目录解析、子目录名、mkdir 失败处理 |
| `test-types` | 枚举值、结构体创建/释放、autoptr 兼容 |

### 集成测试（标记为 `manual`，需要 D-Bus session）

| 测试 | 内容 |
|------|------|
| `test-portal-available` | 检查 Portal 是否可用 |
| `test-screenshot` | 实际截图请求（需要交互）|
| `test-screencast` | 实际 ScreenCast 建立（需要交互）|

---

## 11. MVP 验收映射（md §13）

| # | md 要求 | 实现模块 | 验收标准 |
|---|---------|---------|---------|
| 1 | 能检测 Screenshot Portal | portal-client + capabilities | 调用 `query_targets` 返回非零位掩码 |
| 2 | 能读取截图 targets | screenshot-portal + capabilities | 返回 AvailableTargets 位掩码映射 |
| 3 | 能完成 Screenshot Request | screenshot-portal + portal-request | 发起请求 → 得到 URI |
| 4 | 能建立 ScreenCast Portal Session | screencast-portal | 成功返回管程 session |
| 5 | 能返回 PipeWire FD 和 stream metadata | screencast-portal | session 含有非负 fd 和 stream 列表 |
| 6 | 正确处理用户取消 | portal-request | CANCELLED 错误码，不 panic |
| 7 | 正确清理 Portal Request/Session | portal-request + screencast-portal | D-Bus signals 清理 + fd close |
| 8 | 能生成安全、不冲突的输出路径 | output-path + filename | 文件名不覆盖已有文件 |
| 9 | 不依赖 GTK | 构建系统 | `meson build` 不链接 GTK |
| 10 | 不依赖 PipeWire/GStreamer | 构建系统 | `meson build` 不链接 pipewire/gstreamer |

---

## 12. 关键设计决策记录

| 决策 | 选择 | 理由 |
|------|------|------|
| 异步模式 | 全部 GTask 异步 | 产品层可自由选择同步/异步消费 |
| Portal 方法调用 | 高内聚 request 封装 | 避免每个产品重复实现 D-Bus 信号订阅 |
| 能力模型 | 一次性查询 + 缓存 | md §4 要求产品层不自己读 Portal property |
| 输出路径 | 同步 + 防冲突循环 | 轻量操作，不值得异步开销 |
| 结构体生命周期 | g_autoptr 兼容 | 现代 GLib 风格，减少泄露 |
| 错误类别 | 6 种错误码严格区分 | md §5 要求"用户取消必须与真正错误区分" |
