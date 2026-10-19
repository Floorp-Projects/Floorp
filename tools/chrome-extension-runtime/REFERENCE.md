# Chrome comparison reference

The fixed reference is official Google Chrome **155.0.8059.84**, Linux x86-64.
`chrome-reference.json` records the vendor package URL, package/binary hashes,
and the signed apt metadata verified when choosing this reference. Google’s
Release signature, the signed index hash, and the package hash were verified.

Download and extract locally, then run the authored fixture against both
browsers:

```sh
python3 tools/chrome-extension-runtime/fetch-chrome-reference.py
node tools/chrome-extension-runtime/compare-reference.mjs \
  --floorp _dist/bin/floorp/floorp
```

The downloader uses Python’s standard library and `dpkg-deb`. It does not
install packages or change dependency declarations. The comparison requires the
project’s existing Node/esbuild dependencies and a patched Floorp artifact.
Chrome runs headless with its sandbox, a separate temporary profile, and CDP
pipe debugging. Only the checked-in unpacked fixture is loaded; no account or
Store extension is used. The fixture has version `1.0` and a fixed public key,
giving Chrome ID `deeefbhaclcgmcjdgbnfdmjmmdjdgfjk`.

The Floorp fixture uses the production manifest transformer. The report retains
raw results and normalizes only nondeterministic context IDs and native URL
schemes when comparing runtime IDs, self messages, Ports, context filters, and
Offscreen document messaging/lifetimes. It separately records Chrome’s real
Service Worker and Floorp’s current event-page background. This difference is
required to remain visible; a matching API fixture does not establish Worker
compatibility or a real-extension compatibility rate.

Reports and temporary profiles go under
`_dist/chrome-runtime-tests/comparison-*`. The initial reference download
metadata and headless smoke result are under `_dist/chrome-reference/`. This
harness currently covers Linux x86-64 only.

The debugger adapter has a separate authored public API differential fixture:

```sh
node tools/chrome-extension-runtime/compare-debugger.mjs --floorp /path/to/floorp
# A compiled candidate is required for these additional native commands:
node tools/chrome-extension-runtime/compare-debugger.mjs --floorp /path/to/floorp --native-overrides
```

Both runs use the same frozen Chrome package and binary hashes. The second run
checks focus/visibility emulation, real CSP-blocked and permitted inline
scripts, and CSP restoration after debugger detach.

`node tools/chrome-extension-runtime/probe-desktop-reference.mjs` freezes the
Worker DesktopCapture initiation contract. Chrome 155 returns a synchronous
integer request ID; a Worker must supply a target tab. With a target, both a
request without an immediate gesture and a request from an actual click can open
the chooser. Programmatic cancellation suppresses the callback. The headless
probe includes the tab source because screen/window sources are unavailable in
this environment; it does not select a source or claim an acquired stream.

The authored auxiliary API comparison is reproducible with:

```sh
node tools/chrome-extension-runtime/compare-auxiliary.mjs --floorp <binary>
python3 tools/chrome-extension-runtime/run-browser-tests.py --browser <binary> \
  --test browser-features/modules/modules/chrome-web-store/test/ChromeDeclarativeContentNative.test.mts
python3 tools/chrome-extension-runtime/run-browser-tests.py --browser <compiled-binary> \
  --test browser-features/modules/modules/chrome-web-store/test/ChromeAuxiliaryNative.test.mts
```

The comparison exercises storage authorization grant/revoke, notification
create/update/readback/clear, native action settings/badges/popup, real
declarative constructors and ImageData, rule storage, and actual content-script
injection. The lifecycle suite additionally checks background suspension,
extension reload, permission revocation/restoration, native action effects, and
uninstall cleanup. The alert suite requires the compiled native notification
patch and checks actual browser alert controls and event wake-up. An older
artifact reports its missing native argument instead of silently accepting
button support. Frozen auxiliary package references can be regenerated with
`inventory-auxiliary.py` after the signed corpus has been downloaded; they are
source evidence, not execution passes.
