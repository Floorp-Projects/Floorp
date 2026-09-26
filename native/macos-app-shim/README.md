# macOS App Shim native core

This target is the native application half of Floorp's experimental App Shim.
It owns a real `NSApplication`, `NSWindow`, `NSView`, menu bar, and Core Animation
layer tree. It does not create a browser profile, execute web content, use
`WKWebView`, or copy screenshots from another window. Gecko remains responsible
for content processes, site isolation, rendering, storage, and permissions.

The native tests establish the SDK-only protocol and presentation foundation.
They are not evidence of a complete Gecko integration, end-to-end login sharing,
VoiceOver, native dialogs, drag-and-drop, GPU recovery, or release readiness.

## Build and native validation

Run from the repository root on macOS with the Xcode SDK and CMake installed:

```sh
cmake -S native/macos-app-shim -B _dist/app-shim-native -DCMAKE_BUILD_TYPE=Debug
cmake --build _dist/app-shim-native -j 4
ctest --test-dir _dist/app-shim-native --output-on-failure
```

`FloorpAppShimCore` is a static library built with ARC, C++17, and C++ exceptions
disabled. `floorp-app-shim` is the production executable. The separate
`floorp-app-shim-tests` executable contains its own compile-time-only test peer
policy. No production command-line option disables authentication.
The SDK build explicitly ad-hoc signs local developer executables after linking,
including on Intel. This is not distribution signing or notarization. The
installer must still sign and bind each personalized application bundle.

Tests register an ephemeral Mach service, spawn a separate test process,
authenticate the kernel-provided audit token, transfer and read an IOSurface
Mach right, reject a wrong peer and replay, and create a short-lived native test
window. WindowServer ownership is checked without capturing the screen. Tests
also exercise atomic layer transactions, raw compositor geometry, and the
Japanese text-input cache. They do not access a user browser profile.

For a compiled, signed Runtime, `tools/app-shim/test-runtime.py --browser /path/to/Nightly.app`
creates an isolated profile and signed temporary Shim bundle. It requires an
authenticated second process, WindowServer ownership, real Gecko compositor
frames, shared HttpOnly cookies and local storage, AppKit input delivered with
`CGEventPostToPid`, and a cookie written by that input returning to the normal
browser. Native input requires existing Accessibility permission for the test
runner. `--skip-native-input` records partial evidence and exits 2. Reports and
browser logs remain under `_dist/app-shim-runtime-smoke`; the harness stops only
its own browser. Passing the standalone native tests does not satisfy this gate.
`--launch-services` exercises macOS application startup and separately verifies
the browser PID and exit status. `--capture-window` captures only the verified
fixture window with existing Screen Recording permission. Frontend mode
(`--frontend-modules bridge/loader-modules/_dist`) enables a real WebDriver BiDi
session so beforeunload prompts can be cancelled explicitly; classic Marionette
automatically accepts those prompts. Add `--quit-all` to test the alternate
browser-quit setting.

## Bundle identity and startup

The installer must put these values in the signed bundle's `Info.plist`:

| Key | Value |
| --- | --- |
| `CFBundleIdentifier` | Unique installed application identifier |
| `CFBundleDisplayName` | Application name |
| `FloorpAppShimAppId` | Existing Floorp Web App ID, including UUID braces |
| `FloorpAppShimProfileId` | Stable parent profile identity |
| `FloorpAppShimHostBundleIdentifier` | Pinned browser signing identifier |
| `FloorpAppShimHostCodeRequirement` | Browser's designated code requirement |
| `FloorpAppShimHostPath` | Absolute path to the verified browser `.app` |
| `FloorpAppShimProfilePath` | Absolute path to the existing browser profile |

Finder startup (`argc == 1`) validates the Shim's code signature, the host's
signature/requirement/identifier, and the existing profile directory. It uses
`posix_spawn` to invoke the regular executable inside the verified browser with
`--profile <existing directory> --start-ssb <app ID>`, then exits. Arguments are
passed literally, without a shell. `MOZ_NO_REMOTE` and `MOZ_NEW_INSTANCE` are
removed from the inherited environment so standard browser profile remoting can
forward the request to an existing host. `XRE_PROFILE_PATH`,
`XRE_PROFILE_LOCAL_PATH`, `SELECTABLE_PROFILE_RESET_PATH` and
`SELECTABLE_PROFILE_RESET_STORE_ID` are also removed because Gecko consults them
before the explicit profile argument. This bootstrap
does not own a content window. The browser subsequently launches the real Shim
instance with:

```text
--host-service SERVICE --host-pid PID --launch-token 64_LOWERCASE_HEX_DIGITS
```

The browser must launch that authenticated instance with
`createsNewApplicationInstance = YES`, so it cannot accidentally reuse the
short-lived Finder bootstrap process. Its own app registry should reuse an
already connected app. The cold invocation uses normal profile remoting and
must not create a second owner of the existing browser profile.

The warm Shim verifies the first received message using the Mach audit token,
expected PID, current effective user, pinned browser signing identifier, and
sealed designated requirement. Subsequent messages must have the exact verified
kernel audit token, including the process generation. It checks its own signature as well. The host
must independently authenticate the registered Shim, including its sealed
app/profile identity, expected launched PID, one-time launch token, and registered
code identity (for personalized ad-hoc bundles, the exact registered CDHash).
A service name, a JSON PID, or possession of a Mach send right is not sufficient
authentication.

## Wire protocol

`Protocol.h` is authoritative. Version 1 uses local native-endian Darwin Mach
messages, a fixed 32-byte `WireHeader`, and a UTF-8 JSON dictionary of at most
64 KiB. Each message has a Mach complex body with zero or one send-right
descriptor. Zero padding aligns the message to four bytes. Reserved fields must
be zero; major/minor versions, message types, sizes, descriptor kinds, and audit
trailers are checked. Each direction uses a strictly increasing nonzero 64-bit
sequence. App, profile, layer, and window identity cannot be changed mid-session.

Normal host and Shim messages use an ordered asynchronous sender. A successful
enqueue means admission, not delivery. Each connection permits at most 512
queued/in-flight messages and 4 MiB of encoded payload, independently retaining
destination and IOSurface send rights. A private worker retries a full kernel
queue for at most five seconds per message, so AppKit creation and activation do
not block either peer's main queue. Delivery failure is reported on the main
queue. The message bound accommodates a maximal 258-message layer replacement
transaction (begin, 128 additions, 128 removals, commit). Receive ports request
the largest kernel queue limit, so a briefly busy main thread is absorbed by the
queue rather than overflowing it. Queue overflow is reported as backpressure:
the Shim drops coalescible `mouseMove` and `scroll` input instead of quitting,
every other refused message disconnects its connection, and host presentation
and control sends disconnect on refusal. This is bounded backpressure, not
unbounded presentation retry. `Stop` drops pending rights, suppresses stale
failure callbacks, and returns without waiting for the worker; an already
delivered kernel message cannot be recalled. The initial `Hello` bootstrap
remains a single synchronous send before windows exist.

Text forwarded from AppKit (`insertText`, `setMarkedText`) is bounded by its
encoded size, not by UTF-16 units, so multi-byte IME and dictation input cannot
exceed the 64 KiB wire payload once the JSON wrapper is added.

The Shim looks up the browser's registered service with `bootstrap_look_up` and
sends `Hello` with a MAKE_SEND descriptor for its private receive port:

```json
{
  "appId": "{existing-uuid}",
  "profileId": "stable-profile-id",
  "bundleIdentifier": "installed.bundle.identifier",
  "launchToken": "host-generated-64-hex-token",
  "nonce": "shim-generated-64-hex-challenge",
  "capabilities": ["iosurface-bgra-layers-v1", "text-input-v1", "native-window-v1"]
}
```

The verified host replies with `HelloAccepted`, echoing `appId`, `profileId`, and
`nonce`, and setting `accepted` to `true`. No windows are created before this
response. An unverified message, invalid command, failed send, host exit, or
five-second handshake timeout closes the connection and terminates the Shim.

### Windows and lifecycle

All IDs are nonzero unsigned 32-bit integers. Window sizes and input coordinates
use Cocoa points. View coordinates start at the content area's top left.
Desktop coordinates use the primary screen's top edge as `y = 0`.

| Host command | Payload |
| --- | --- |
| `CreateWindow` | `windowId`, `title` (up to 512 UTF-16 units), `width`, `height`; optional `kind` (`window`, `dialog`, `popup`), `parentWindowId`, `geometryRequestId` |
| `CloseWindow` | `windowId`; host has already completed close/beforeunload policy |
| `SetWindowTitle` | `windowId`, `title` |
| `ActivateWindow` | `windowId` |
| `ConfigureWindow` | `windowId`; optional `x`, `y`, `width`, `height`, `visible`, `enabled`, `fullscreen`, `minimized`, `maximized`, `geometryRequestId` |
| `SetCursor` (31) | `windowId`, `cursor` from the explicit native cursor whitelist |
| `Terminate` | Empty object; ends only this app's Shim |
| `CancelQuit` | Empty object; clears a pending app-quit request |

Sizes are bounded to 64–16384 points and at most 16 windows are allowed.
Popups use a borderless nonactivating `NSPanel`, require a live same-app parent,
and permit sizes down to one point. Dialogs use a titled native window. Native
child windows belong to their Shim parent. Closing a parent detaches its children
so each Gecko widget can complete its own close policy; detached popups are
hidden, and independent dialogs remain alive. Queued child frames remain valid
until that child's own `CloseWindow`. Repeated `CloseWindow` is idempotent.
`WindowChanged` reports `windowId`, `kind`, `geometryRequestId`, `x`, `y`, `width`, `height`, `scale`,
`key`, `visible`, `occluded`, `fullscreen`, `minimized`, and `maximized`. Kinds include `created`, `resized`,
`moved`, `backingChanged`, `focusChanged`, `occlusionChanged`,
`fullscreenChanged`, `minimizedChanged`, `configured`, and `closeRequested`.
The separate `closed` acknowledgement contains only `windowId` and `kind`; it
is emitted after the window is hidden and layer removal has committed.
The geometry request ID defaults to zero at creation. If supplied on a later
configuration, it must strictly increase. Native notifications echo the most
recent request so the host can ignore stale geometry while updating requested
bounds synchronously. A geometry configuration explicitly reports `configured`
even when AppKit does not change the frame. On macOS 14 and later, the host
yields activation to its authenticated Shim before an activation command, and
the Shim requests cooperative activation using the public AppKit API.

Native close and Cmd+Q do not destroy browser content directly. Close emits
`closeRequested`; Cmd+Q emits `QuitRequested {appId}` and cancels AppKit's local
termination until the host sends `Terminate`. Dock reopening emits
`ReopenRequested {appId}`. Closing the last window leaves the application
running. Menu actions emit `MenuCommand {windowId, command}`. The host owns
beforeunload, app-scoped quit, browser keepalive, global shutdown, and restart.

An unexpected Shim disconnection suspends the Gecko widget's presentation and
preserves its docshell. A new authenticated connection recreates the existing
native window identities in parent-first order, restores window state and editor
cache, and submits existing compositor layers through a fresh presentation sink.
The browser session policy owns the bounded retry and force-destruction after
failed recovery; a dead native owner does not trigger a vetoable close request.

Cursor names are `default`, `text`, `vertical-text`, `pointer`, `crosshair`,
`grab`, `grabbing`, `move`, `copy`, `alias`, `context-menu`, `not-allowed`,
`ew-resize`, `ns-resize`, `nwse-resize`, `nesw-resize`, `zoom-in`, `zoom-out`,
and `none`. AppKit cursor rectangles and immediate changes are confined to the
owning view/window under the pointer. Unknown names are rejected. Custom Gecko
image cursors use their declared standard fallback; no image path is accepted.
Zoom and diagonal cursors use public macOS 15 APIs with arrow/crosshair fallbacks
on older systems. `none` uses a locally generated transparent cursor, avoiding
global hide/unhide state.

### Compositor transactions

Every frame is `BeginFrame`, zero or more `SetLayer`/`RemoveLayer`, and
`CommitFrame`, with matching `windowId` and strictly increasing `frameId`.
Only one frame may be staged per window. Changes become visible together inside
one Core Animation transaction. A committed tree has at most 128 layers.

Runtime `SetLayer` uses these fields:

- `layerId`, `positionX`, `positionY`, `sizeWidth`, `sizeHeight`: Gecko device pixels.
- `scale`: backing scale (0.25–8), `opacity` (0–1), `zOrder`.
- `transform16`: 16 finite values ordered `_11, _12, ... _44`, with Gecko's
  pretranslated position. The Shim performs the position/scale conversion;
  the host supplies the original matrix, not a preconverted matrix.
- `displayRect: {x,y,width,height}`: crop in local device pixels.
- Optional `clip: {x,y,width,height}`: clip in root device pixels.
- Optional `roundedClip: {rect, radii}`: root device pixels; four `{width,height}`
  radii in top-left, top-right, bottom-right, bottom-left order.
- `flipped`: surface orientation, `sampling`: `nearest` or `linear`.
- Either `surfaceId` plus exactly one IOSurface Mach send right, or
  `color: {r,g,b,a}` with no right and no `surfaceId`.

The raw geometry is represented with separate root clipping, transform,
display-rectangle crop, and content layers. Color-only layers may provide zero
`sizeWidth`/`sizeHeight`; their extent is then derived from `displayRect`.
The test-oriented point geometry (`x/y/width/height` instead of raw geometry)
remains available, with a local point-space `clip`.

Only single-plane 32-bit BGRA IOSurfaces are accepted. Surface dimensions must
match raw geometry, be at most 16384 pixels per dimension, and occupy at most
256 MiB. Current, staged, and retiring surfaces together are capped at 512 MiB
per window. HDR, DRM, and other pixel formats are rejected; the host must not
silently flatten or reinterpret them. Runtime support for such surfaces is a
separate integration task.

The selected remote Gecko widget opts out of native overlay composition. The
existing `RenderCompositorNative` full-window path composites page and video
into BGRA IOSurfaces through WebRender/OpenGL, with the existing SWGL fallback.
This avoids exporting YUV overlay planes. It is GPU rendering, with no screenshot
or bitmap readback in the transport. This does not establish HDR/DRM support or
tone mapping; unsupported export remains an explicit failure.

`FramePresented {windowId, frameId}` acknowledges the native Core Animation
transaction completion, not a display-vsync fence. `SurfaceReleased
{windowId, surfaceId}` is sent only after a replaced/removed layer has retired.
The host must pin surfaces until this acknowledgement, never reuse a live
`surfaceId`, enforce backpressure over all unreleased surfaces, and retain only
the latest unsent complete snapshot when coalescing. On widget destruction,
remaining producer leases move to the host service. They are released only
after authenticated `WindowChanged {windowId, kind: "closed"}` following native
window hiding and layer-transaction completion, or verified peer-process exit.
A missing close acknowledgement terminates that exact launch after five seconds;
its leases remain pinned until exit. Service shutdown retains process monitors
for the same reason. The Shim never looks up global IOSurface IDs or performs a
bitmap readback.

### Input and IME

`Input` always includes `windowId` and `kind`. Mouse and wheel messages carry
point coordinates, native modifier flags, timestamp, button/click count or
scroll delta/phase. Keyboard messages carry native key code, characters,
characters ignoring modifiers, modifier flags, repeat flag, and timestamp.
`keyDown` precedes AppKit's text interpretation callbacks. The host must not
insert characters twice from the raw key event and the text callback.

`EditorState` is a monotonic host-authoritative cache:

```text
windowId, revision, text, textOffset (default 0), editable, optional password,
selectionStart, selectionLength,
optional markedStart, markedLength,
caretX, caretY, caretWidth, caretHeight,
optional discardMarkedText
```

The cache contains at most 32768 UTF-16 units. Selection and marked ranges are
absolute UTF-16 offsets inside this cache; caret geometry is local points.
Password cache contents must be masked by the host. `insertText`,
`setMarkedText`, `unmarkText`, and `textCommand` events include `editorRevision`.
Replacement ranges encode `NSNotFound` as `location: -1`. Native synchronous
text queries use the cache; they do not block on an IPC round trip. Actual
Japanese IME behavior still needs end-to-end testing with Gecko editor state.

## Validation status

This is an opt-in implementation under integration, not a production-ready Web
App runtime. SDK-only tests and successful compilation do not establish Gecko
behavior. The Runtime smoke gate described above must be run against the final
host and Shim binaries; any failure or explicit skip remains a failed or partial
integration result.

The direct Runtime smoke passed on an arm64 Mac on 2026-09-24 with the built,
ad-hoc signed browser and bundled Shim. It verified a distinct authenticated
Shim process owning the native window, the requested 900×700 default geometry,
actual Gecko frame/repaint acknowledgements, shared HttpOnly cookies and local
storage, native AppKit text input reaching Gecko, and reverse cookie sharing.
With LaunchServices startup, the Shim became the key, unoccluded application
window. An exact-window capture was inspected and showed readable Gecko chrome,
the fixture heading and input, and a transformed rounded tile. Window closure
and service shutdown completed without the earlier nonempty-layer-root crash;
the harness then terminated its test host with its requested SIGTERM. Evidence
is in `_dist/app-shim-runtime-smoke/run-uilwql9b/report.json`. This direct test uses
ordinary browser chrome and does not establish the complete Floorp PWA frontend.

The final signed package also passed frontend lifecycle runs `run-5o38ue6c`
(keep apps running) and `run-f602_v7x` (quit all). Both exercised the real signed
installer, native process recovery preserving page and form state, actual
beforeunload cancellation, and app-local Cmd+Q. The first verified browser-only
quit and idle-runtime exit after the last app; the second verified browser and
app shutdown together. Both recorded browser exit code zero. Their captures
were unavailable because the test window did not become key; they do not add
visual evidence beyond the earlier inspected capture.

Final direct run `run-4rvssjdq` also passed authenticated native window-close
acknowledgement and kept the connection alive beyond the five-second surface
retirement watchdog, followed by clean service shutdown and the harness's
requested SIGTERM. Its native input and shared-session assertions passed.

Real browser module suites also passed under `_dist/app-shim-browser-tests`:
`run-vkyttt2p` (MacOS integration), `run-s2km2x71` (installer), and `run-93paf2cc`
(native runtime, including BiDi beforeunload veto). SDK CTest passed both targets,
and the SDK core compiled for arm64 and x86_64; Intel execution was not tested.

The fully bundled Floorp frontend passed Finder-style warm and cold launch in
`_dist/app-shim-cold-tests/run-5cgyw10k/report.json`, using the final Shim with
restart/reset environment filtering. The normal registered command-line handler
reached the existing parent on warm launch and a new parent with the same profile
on cold launch. Both initialized the actual PWA chrome, shared persistent HttpOnly
cookies and local storage, owned native windows, acknowledged fresh frames and
exited with code zero. Conflicting profile environment variables left the
unrelated profile directories untouched. Captures showed only transformed
window thumbnails; the desktop was locked during the attempted foreground
inspection. This run does not establish full-size PWA visual correctness or
foreground activation on an unlocked desktop.

| Area | Implemented and verified in native tests or direct Runtime smoke | Remaining validation or implementation |
| --- | --- | --- |
| Peer identity | Separate-process Mach audit tokens; signed designated host verification; wrong identifier, changed process generation and replay rejection; real signed Gecko launch | Host upgrade, tampered installed bundle and interrupted launch |
| Window ownership | Real `NSApplication`/`NSWindow`/`NSView`; Gecko window with distinct Shim WindowServer owner and no browser-owned proxy; same-app native popup/dialog parenting, detachment on parent closure and separate child closure | Content `window.open`, XUL menus, focus traversal and multiple concurrent apps |
| Rendering | IOSurface Mach transfer; atomic CALayer transactions; crop, scale, full transform, color layers, rounded clip, flipped sampling and removal; actual Gecko frames, repaint acknowledgements and a visually inspected full-window fixture | Detailed visual comparison, retained-surface stress, GPU restart, sleep/wake and display changes |
| Supported surface formats | Single-plane 32-bit BGRA with bounded dimensions and memory; explicit rejection of unsupported formats | HDR, DRM and other pixel formats are unimplemented |
| Focus and geometry | First-responder hooks, key-window reports, coordinate/backing-scale reports, cooperative activation and hide/show controls; default 900×700 Runtime geometry and foreground activation through LaunchServices; stale geometry request rejection and unchanged-frame acknowledgement | Tab traversal, multi-display coordinates, fullscreen, minimize/maximize and app switching are untested |
| Pointer and wheel | Mouse hooks and wheel deltas/phases are serialized; a native view mouse event is tested | Real Gecko click/scroll behavior and APZ integration are untested; magnify/rotate/swipe, pointer lock and explicit capture are unimplemented |
| Keyboard and IME | Raw key hooks and `NSTextInputClient`; targeted native keyboard events reach Gecko through AppKit and authenticated Mach; Japanese marked/committed callback tests; bounded surrounding-text offsets and caret queries | Actual Japanese input sources, candidate windows, dead keys, emoji, dictation, composition cancellation and rapid asynchronous cache changes are untested |
| Password input | Host sends masked context; secure event input is paired with editable password focus and released on blur/disconnect | OS-level secure-input behavior and password-field transitions need end-to-end validation |
| Cursor | Whitelisted public NSCursor mappings and view-confined cursor rectangles; unknown names/image paths rejected; custom images use standard fallback | Actual Gecko hover, overlapping windows and cursor restoration need integration tests; custom image cursors remain unsupported |
| Drag and drop | Ordinary mouse-drag events are forwarded | OS drag sessions, `NSDraggingDestination`, pasteboard payloads, file promises and cross-app dropping are unimplemented |
| Accessibility | Native AppKit window and menu structures exist | A remote Gecko accessibility tree, hit testing, actions, text ranges and notifications for VoiceOver are unimplemented; visible page content is not accessible through this Shim yet |
| Menus and clipboard | Per-app application/File/Edit/Window menus; hide, quit, close, minimize and edit-command hooks | Gecko command routing and clipboard behavior need integration tests; dynamic enablement, Services, menu localization and full native context-menu behavior are incomplete |
| Dialogs and permissions | Native child dialog windows can be created | File/color pickers, printing, JavaScript dialogs, authentication and permission prompts must be routed to the correct app; native browser services are not yet bridged |
| Cold launch | Signed temporary host launch with literal argument paths and UUID braces; remoting and restart/reset profile overrides removed; wrong identity rejected; bundled Floorp PWA warm and cold Finder launches share the same persistent session and exit cleanly | Another profile active, browser moved/updated, repeated Finder reopening and unlocked foreground activation need scenario testing |
| Lifecycle | App-local quit/close requests await host decisions; real Gecko window closure and service shutdown without a host crash; native process kill/recovery preserves the page and form; actual beforeunload veto; app-only quit, browser keepalive and quit-all setting; last app exits the idle runtime | Background host activation/menu restoration beyond the exercised paths, restart and macOS logout need whole-product tests |
| Shared profile | Shim has no profile or web-content engine; Runtime smoke verifies real HttpOnly cookie/local-storage sharing and reverse cookie writes | Service workers and broader storage behavior remain untested |

Direct smoke mode tests the native component; frontend mode adds selected real
installer and lifecycle paths. These tests do not establish complete migration
coverage, all browser lifecycle policy, OS input methods, or the unimplemented
integration areas in this table.
