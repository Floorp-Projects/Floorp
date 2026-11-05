# KAN37 reported Linux12.18 — fresh version-pinned native acceptance

2026-11-06 JST. **Official12.18.0 Linux native initialization and7minimal native paints now qualify.** This supersedes only the original-version test gap from c16f2dcedeb2ac50a4880a8d6052ad376e5c7d56. Earlier auto-updated copy, invalid version attempt, errors and raw evidence remain untouched. Original missing-thumbnail context and Firefoxpolicy causality remain unresolved. No speculative product fix/PR, merge, release or Done.

## Legitimate version fixation and source evidence

User explicitly authorized `app.update.disabledForTesting=true` before startup in the isolated QA profile. New product prefix extracted from the previously hash-verified official archive; SHA256 a4610fa0cf86092c1f0337173d722be874562b482c760bf54cd67917e68ee512. This is not a downgrade/relabel of the already updated copy. Product source1728a7678c34aec1ad502edb06a168fdf2a07be8, native156.0.1/BuildID20260916130640, Floorp display12.18.0@156.0.1.

Before launch, read the actual156package: omni.ja/modules/UpdateServiceStub.sys.mjs `updateDisabledForTesting` returns `(Cu.isInAutomation || Marionette.running || RemoteAgent.running) && pref`. Its `updateDisabled` includes that result. UpdateService.sys.mjs delegates `disabledForTesting`/`disabled` to the stub; `canUsuallyCheckForUpdates` returns false when disabled. RecommendedPreferences documents the same profile-pref as preventing automatic download/application in tests. Source-member hashes and excerpts retained. This is the Gecko update-service gate, not a claim that the standalone MAR binary reads profile prefs or a bypass of signature verification.

Actual native `@mozilla.org/updates/update-service-stub;1` / nsIApplicationUpdateServiceStub returned updateDisabledForTesting=true and updateDisabled=true under the real Marionette session. Initial observational probes used a nonexistent interface/non-exposed property; repaired to the shipped stub interface before image testing, preserving those diagnostic records. The test pref itself was set from the first fresh startup. No update-check method invoked, distribution/ID override, production update routing, signing/sandbox/TLS/TOU change.

## Natural startup and minimal image acceptance

Same Debian13/kernel6.18.57 Xvfb/Openbox/private DBus environment as prior current-version results, proper inherited proxy/CA, fresh owned QA profile. remote.prefs.recommended=false preserves normal feed initialization. Normal start → actual Floorp Start Settings checkbox Disable Floorp Start → normal Quit → same-profile native about:newtab. Native Nova `.content-main` renders with8initial tiles; ActivityStream naturally initialized with normal feed set. No renderer/init call or artificial observer notification.

64px four-color data favicon is registered through existing native NewTabUtils.pinnedLinks/TopSites cache+refresh. It is an API data fixture, not an external favicon retrieval or UI-created shortcut. Supported full design preference preserves schema. No DOM image/CSS/renderer substitution. Native bounding rectangle/screenoffset/DPR locate four quadrant samples in actual X framebuffer PNGs.

| Native156 design | Conditions | Exact4quadrant paints |
|---|---|---:|
| Proton | reload, nativecache clear+reload | 2/2 |
| Fluerial | reload, nativecache clear+reload | 2/2 |
| Lepton | reload, nativecache clear+reload, normalprocess restart | 3/3 |

All7 match RGB red255/green255/blue255/yellow255 exactly. Restart reuses real pinned storage without repinning. Standard default-browser prompt dismissed with ordinary Escape; no Set Default or TOU acceptance. All3sessions (seed/native/restart) normalQuit/exit0. Each startup reads actualServices.appinfo and nativeupdategate, compares the complete46file/symlink list to the fresh archive baseline. Native/restart final live checkpoints also match; next startup plus finalafterquit hash covers all process boundaries. Final46entries exactly unchanged, including IDs/resources/executables. No version drift, manufacturedID or source patch.

Qualified Windows14/currentLinux7 results are separate prior work. This followup adds7reported-version Linux paints; prior FloorpStart70decode/10paint and prior currentLinux7paint were not repeated. Source worktree remains read-only main29a3eb23145da90706d8a94441373278521cc737; no new package/build or Windows trial.

## Classification and remaining conditions

Original Linux12.18 native startup/reload/cache/restart gap is fulfilled under these controlled conditions. Together with prior currentLinux7, there are14qualified Linux native paints, with separate versions/profiles. This does not establish original user's remote-thumbnail/cache/profile/desktop conditions or reproduce the reported blank images. PhysicalGUI/input remains untested. The earlier Firefox fresh-profile ActivityStream-null/policy boundary is unchanged and not retested; no TOU attribution/acceptance spoof. Existing failed auto-update attempt remains historical evidence, not deleted or recounted as a valid156trial. Environment/security protections retain stock settings.

This is additionalKAN37, outside originalLinux22/C5 denominator. Prior KAN50/77/86 and KAN102hold unaffected. `verify-evidence.py` recomputes framebuffer colors, validates version/gate/checkpoint hashes and normal exits. QA export contains no profiles/binaries/cookies/credentials/temporarydownloadURLs.
