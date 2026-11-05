# KAN-90: Thunderbird-origin portal route established

## Result

**One real Thunderbird mail-link → portal OpenURI → existing signed Flatpak Floorp route passed.** This is a new Thunderbird-origin test, not the earlier independent gdbus OpenURI fixture. The native Debian140 control also explains why the earlier configuration did not demonstrate portal dispatch.

Source reference for Floorp remains `984509d2053be7bae9027133bda7259cd706b208`, Floorp12.21.1/Gecko159.0.1 signed Flathub app commit `ab3c23d5f3a32863918263c197b483e37d9ed78a6bb9d00a1657e31dc5e04621`. No product code changes, forced identity, protection disabling, merge/release or Done transition.

## Exact native140 settings and dispatch reason

The previously tested Debian package is Thunderbird `1:140.12.0esr-1~deb13u1`, application140.12.0 BuildID20260612205621, Gecko source [`7df86525c2c876c7c92320e49c3e0771f7a605c0`](https://hg.mozilla.org/releases/mozilla-esr140/file/7df86525c2c876c7c92320e49c3e0771f7a605c0/). Its own platform.ini identifies this source; sources were fetched from the pinned official repository, not inferred from a newer Firefox checkout.

- `StaticPrefList.yaml` documents `widget.use-xdg-desktop-portal.mime-handler`: **0 never / 1 always / 2 auto**, default2. `WidgetUtilsGtk.cpp::ShouldUsePortal(MimeHandler)` overrides generic GTK_USE_PORTAL auto behavior with `IsRunningUnderFlatpakOrSnap()`. Thus GTK_USE_PORTAL=1 alone does not select MIME portal handling for this native package.
- The separate `widget.use-xdg-desktop-portal.open-uri` is also 0/1/2 with default2, but its inspected call site is file/directory reveal. Setting open-uri=2 did not force HTTP protocol launch through a portal.
- A fresh native140 profile was explicitly tested with the documented **mime-handler=1**, open-uri=2 and GTK_USE_PORTAL=1. Runtime readback confirmed those values and content sandbox5. Handler inspection reported `preferredAction=4`, equal to `Ci.nsIHandlerInfo.useSystemDefault`, no preferred application, and default description `System Handler`.
- The pinned `nsMIMEInfoBase::LaunchWithURI` useSystemDefault branch invokes `LoadUriInternal`; pinned `nsMIMEInfoUnix`/`nsGNOMERegistry` lead to `nsGIOService::ShowURI`, whose implementation calls `g_app_info_launch_default_for_uri`. GLib2.84.4's official synchronous implementation tries the registered native handler first and calls the portal only after **failure**, under `if (!res && glib_should_use_portal())`. The same isolated library lookup resolves GLib2.84.4. The native desktop handler succeeded: GET arrived in the existing Floorp, forkServer96 remained96, but no OpenURI occurred.
- Sender/PID mapping for this control distinguished Thunderbird PID41835 on sender`:1.211` (SchemeSupported only) from Floorp's proxy PID40311 on sender`:1.15`. A SchemeSupported entry without that mapping is not evidence that Thunderbird dispatched a URI. Earlier traces lacked this sender attribution; they should not be promoted to a proven Thunderbird portal path.

These source gates plus effective handler action and real bus observations explain the tested native route. They do not claim that every possible native Thunderbird handler configuration must bypass portals or that the native140 behavior is a Floorp bug. No fake handler object, hidden default-app registry, forced native failure, or replacement OpenURI implementation was used.

## Genuine package/app-id distinction

The installed native desktop entry is `thunderbird.desktop`, RemotingName/StartupWMClass `thunderbird-default`; Debian's packaging patch sets the program name from the lowercased remoting name. This is distinct from Mozilla's application GUID and from a portal's caller identity. Pinned140/158 upstream `RegisterHostApp` contains the literal `org.mozilla.firefox` for its unsandboxed registration path; that string must not be treated as proof that the caller is Firefox or manually replaced to make the test pass. The inspected registration skips Flatpak/Snap. No registration or app-id spoof was performed.

The old uppercase Flathub ID `org.mozilla.Thunderbird` reports end-of-life/rebase. The actual current signed app is **`org.mozilla.thunderbird`**, commit `e3be32008d26a83a5da2e77783688b79a1306bfd71f5c2aa8873922659fd35ec`, Freedesktop runtime25.08. Its product was **Thunderbird159.0**, not assumed158; application/platform BuildID20261026161902 and Gecko source [`5add844c26613d7e4d84c799d6e23cf987b1240d`](https://hg.mozilla.org/releases/mozilla-release/file/5add844c26613d7e4d84c799d6e23cf987b1240d/) were extracted and its official source inspected before the trial. The package wrapper normally executes `/app/lib/thunderbird/thunderbird --name org.mozilla.thunderbird`; it was not modified. Official158 source/availability was inspected as a fallback only; no158 product execution is claimed.

## Actual mail-link test: shipped Flatpak defaults

`run-flatpak-159.py` starts a fresh, QA-only Thunderbird profile in its own normal Flatpak app-data area, opens a local .eml with a localhost link, and clicks the visible link with synthetic X11 input. No personal account, credentials, remote mail or form submission is involved. The harness **does not call portal OpenURI directly**. Its gdbus calls only query sender process IDs for attribution.

Thunderbird runtime readback: version159.0, **mime-handler=2, open-uri=2** (both shipped defaults), GTK_USE_PORTAL empty, MOZ_APP_REMOTINGNAME override empty, genuine `FLATPAK_ID=org.mozilla.thunderbird`, content sandbox5 (unchanged product default). Flatpak/sandbox detection supplies the automatic portal behavior. Floorp content sandbox was measured6 before the link; no sandbox preference or disabling environment variable was set.

The successful bounded sequence is recorded in small raw JSON and the real bus trace:

1. The local message link is clicked; Thunderbird's GIO log records ShowURI for `http://127.0.0.1:19094/from-thunderbird-origin`.
2. **Sender`:1.182` issues real OpenURI** for that exact marker. The live DBus GetConnectionUnixProcessID returns host PID45204; its process is `xdg-dbus-proxy`, and its actual `/proc/45204/root/.flatpak-info` identifies **org.mozilla.thunderbird**. This is observed sandbox metadata, not a test-assigned identity.
3. Portal sender`:1.2` (PID43912, xdg-desktop-portal) returns the matching Request.Response **uint32 0** to`:1.182`.
4. The local HTTP server receives **GET /from-thunderbird-origin**. The marker tab appears in the existing live Floorp session, parent **2→2** in the Flatpak namespace. Host Flatpak instance PID **43914→43914** is unchanged. Thunderbird's host instance PID45195 is separately listed.
5. Read-only Floorp process snapshots show **forkServer92→92**. The same live Marionette session responds after navigation. No SIGSEGV/segmentation-fault marker is observed in the owned Floorp trial log. This is a bounded successful path, not a universal crash-freedom claim.

The comparison simultaneously identifies the other SchemeSupported sender`:1.17` as Floorp's proxy PID43967, app-id `one.ablaze.floorp`. The two namespace parent PIDs happen to be2; they are not conflated because host instances, bus proxy PIDs and observed app-ids are recorded separately.

## Scope and remaining condition

Actual environment remains Debian13.6/Xvfb/Openbox, real private session DBus and GTK portal backend, with the private address also used for a fixture system bus. It is not native Fedora44 or its desktop/login/policy services. The existing protected Fedora container namespace EPERM evidence is retained and **was not repeated**. The original reported Floorp12.17.2/Fedora44 forkserver crash remains unvalidated, and package/runtime/source matching plus symbolized crash evidence is still required if it recurs. This successful159 Flatpak Thunderbird path is not a claim about the original reporter's Thunderbird package.

The narrower `needs-thunderbird-portal` capability gap is now actually closed for one regular protected package path. `needs-fedora-44`, original reproduction/crash conditions and `env-blocked` remain. No speculative Floorp or TST patch was made; other issues were not retested.

One initial setup attempt lacked a newly created Thunderbird fixture directory and ended before Thunderbird launch. That is a harness setup error, not a product failure, and is excluded from the successful trial count. Full profiles, screenshots and logs stay local. Exported artifacts contain selected original result JSON, reviewed small bus traces, source/version/hash excerpts, reproducing scripts and a verification script; no cookies, private keys, personal URLs or whole process environment.
