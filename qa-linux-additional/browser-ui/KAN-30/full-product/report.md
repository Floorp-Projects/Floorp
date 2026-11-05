# KAN-30: full current-production package with existing PR2934

**Standard strict packaging passed, and the parent verified the packaged payload in actual Linux UI tests.** No native C++/Rust build, release, signing bypass, new product-fix PR, or shared artifact modification was performed.

## Source and product scope

- Existing PR2934 latest head: `b7866aaa4905be4d86fb3dff1237d6a70c483f10`, changing the sidebar window listener from blur to deactivate.
- Its original full source is Floorp12.20.2 with an older Runtime158-generation lock. That old source was **not** mixed with Runtime159 and called an exact-head full build.
- Candidate is current main `29a3eb23145da90706d8a94441373278521cc737` plus precisely that existing one-line change. Independent clean source commit: `1b8f3503df9e8cae6b08118ecbe2d85e2d5158e7`.
- Retrievable source: [qa/kan30-full-pr2934-current](https://github.com/Floorp-Projects/Floorp/tree/qa/kan30-full-pr2934-current). `candidate.patch` preserves the complete main-to-candidate diff. Source head was held constant throughout build and testing.
- Matching Runtime source `8965ffc346d426ccdf919bd18585e4d4a0c21654`, verified native artifact SHA256 `cb9b01283d5ce3167f4ba046ee7b39ae9d98a692e1beba73bd9f85f460157772`, Gecko159.0.1/build20261103153232. Product is Floorp12.21.1@159.0.1.

## Complete production and package checks

Frozen dependency installation and all before-mach production targets passed. Standard configure succeeded in4m24 (2267 moz.build files,15809 descriptors); prebuilt-artifact `mach build` succeeded in1m04; after-mach XHTML injection passed. All **1115 production assets across11 mounts** match their independently built source output byte-for-byte. There was no compiled-controller replacement, CSS injection or DOM rendering substitute.

Canonical packager.mk was compared byte-for-byte to Runtime gitHEAD before packaging, retaining `--error-if-empty`. Standard `mach package` exited0 in1m37. The former missing-XPT problem was resolved with the existing genuine complete generated set:155 modules,801 selected IDLs and903 source/generator dependencies all match this candidate's unchanged artifact configuration. All155 companion ZIP entries match generated material; ZIP SHA256 `e6b1118254c2066921c207741090aa59a165591c5ac564ab239c221e88412889`. Neither dummy XPTs nor a relaxed empty-archive check was used. The first QA comparison attempted to read manifests from the wrong directory; its traceback is preserved, the path was corrected, and the same assertions then passed.

Native floorp SHA256 remains `4f05c1330b38e50040440be6a3b6e0e7a5b52bc79edb5c5478d978c81f5a603d`; libxul remains `760ff8a689f52c9b1f3c89a45ea8813e59f187990caed747e4a7680020c8dbc7`. Buildid2 is `01a1ad0a-6135-7717-8ec5-d54d679fd122`.

## Local deliverables and actual UI result

- Archive: `/workspace/kan30-full/obj-artifact-build-output/dist/floorp-12.21.1@159.0.1.en-US.linux-x86_64.tar.xz`.
- Archive SHA256: **`388f2543413cc44783e8fcd114a4149081394793693ecb79924a3137604f5bb3`**.
- Runnable: `/workspace/kan30-full/obj-artifact-build-output/dist/floorp/floorp`.
- This is the **standard packager's staged product**, not a separately tar-extracted copy. All57 archive members matches its staged content/type. This avoids another large extraction while proving the runnable payload is what the archive contains.

Parent actual test29950 used that GRE directory and current source SHA. Inputs were synthetic W3C/OS events on the Linux virtual desktop, not physical hardware. Normal signed Sidebery installation (signed state2), toolbar opening and OS row click QAA→QAB kept sidebar opacity1. Both Ctrl+Tab focus conditions kept opacity1. Pointer leave closed it to0. Actual trusted window deactivation changed document focus TRUE→FALSE and closed it to0. Parent evidence is `../probe-29950.json` plus its actual screenshots; this child did not independently repeat those UI checks. Product signature requirement was TRUE with no user value, sandbox6; automation focus test mode was explicitly false. The official baseline's existing false signature default is a different defaults configuration and is not concealed.

Archive, executable and libxul hashes remain unchanged after UI testing; the full57-member archive/stage audit was repeated. No binary/archive was published. Source branch contains only the one-line integration; parent handles the existing test Draft3043, Jira and evidence publication.

## Isolation and practical limits

Initial disk space was274MB. Private overlay upper and reflink cloning were unsupported. Automatic review rejected hardlink cloning and deletion of old canonical-stage output; neither rejected operation executed. The successful alternative used a **private user+mount namespace with a genuinely read-only existing Runtime bind mount**, separate frontend source, external mozconfig and object output. Its relative noraneko symlink resolves to this candidate's own source. A write probe failed as required. Existing byte-identical Node tools were reused through another read-only mount after deleting only the new candidate's own disposable duplicate tools. No canonical package, XPT set, source or tool was deleted. Symlinks were dereferenced one at a time to achieve the workflow's rsync-aL payload while keeping disk peaks bounded.

The high shared memory use included source-cache pressure; OOM/OOM-kill stayed0. This is not evidence of a product crash. The65MB raw Runtime size listing stays outside publishable QA; its checksum and484172 tracked entries/3657015593 bytes are in capacity.json.

This is not an official release/default-byte/signature identity claim. The original CI-produced XPT companion is unavailable, so identity with XPT bytes embedded during native compilation is not proven; genuine regeneration from matching inputs is proven. Full native recompilation, original hardware/distribution conditions and exhaustive UI coverage are not claimed. No security-pref override stage was applied. Reproduction scripts, configuration, manifests, failure logs and hash checks are adjacent; they require the stated existing native inputs and sufficient workspace capacity.
