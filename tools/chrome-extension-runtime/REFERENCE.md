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
Offscreen document messaging/lifetimes. It separately records each browser’s
actual background context. The initial development-artifact comparison used a
Floorp event page; rebuilt native candidates must retain and execute the signed
manifest’s Service Worker. Historical event-page results remain visible and do
not establish native Worker compatibility or a real-extension compatibility
rate.

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

The signed content corpus can run an authored primary operation using unchanged
package scripts:

```sh
node tools/chrome-extension-runtime/test-corpus-content.mjs --ids eimadpbcbfnmbkopoojfekhnkhdbieeh,clngdbkpkpeebahjckkjfobafhncgmne,jinjaccalgkegednnccohejagnlnfdag
python3 tools/chrome-extension-runtime/test-corpus-content-floorp.py --browser <compiled-binary> --ids eimadpbcbfnmbkopoojfekhnkhdbieeh,clngdbkpkpeebahjckkjfobafhncgmne,jinjaccalgkegednnccohejagnlnfdag
```

Dark Reader must visibly transform the authored document. Stylus must save
matching CSS through its real editor using native keyboard and pointer input.
The userscript recipe must save an authored script through the real extension
editor, execute it in the main document and an actual matching frame, and retain
the effect across navigation. The reference enables Allow User Scripts through
the actual private-profile extension details toggle. The authored script uses an
exact fixture URL include pattern, including the server port. Every recipe
requires an actual effect, navigation persistence, and restoration after
uninstall. A saved editor or registered bootstrap alone does not pass.

The Floorp wrapper always runs headed. On Linux it creates a fresh `xvfb-run`
display and a private `xfwm4`/D-Bus session through the owned window-manager
wrapper, including when another `DISPLAY` is inherited. Install `xvfb`, `xauth`,
`xfwm4`, `dbus-x11` and `x11-utils` first. macOS and Windows use their real
desktop session. The private consent actor is copied with `--actor-module` and
remains bound to the complete executed-source inventory.

For an authenticated original MV3 manifest declaring `userScripts`, the Floorp
recipe additionally checks the installed Chrome compatibility ID, opens that
addon's real Add-ons Manager permissions panel, and clicks the visible **Allow
user scripts** switch with trusted native input. Its report retains the original
required/optional declaration, selected addon ID, native click events and saved
before/after states. This switch changes only the independent browser setting;
an ungranted optional public permission still blocks execution. Dark Reader and
Stylus declare no `userScripts` permission and receive no consent UI step. No
extension script, preference or permission store is rewritten by the recipe.

Use `FLOORP_TEST_CANDIDATE_MANIFEST=/absolute/path/candidate-manifest.json` when
running the command against a production candidate. The shared installer still
requires the exact authenticated package/build, required native background mode
and complete unchanged original script inventory before a primary can qualify.
Running a raw developer binary remains explicitly unqualified; this readiness
change adds no native qualification or corpus success.

The Floorp run verifies the frozen CRX digest, invokes the production signed
converter, and compares every packaged script digest before installing the
result. Its test actors are explicitly copied into the private profile's
`chrome/runtime-actors` directory before launch using `--actor-module`. Each
actor query has a deadline. Missing fixture actors are reported as a harness
failure; production origin permissions and sandbox policies remain unchanged.
Recent packages that require native service workers must be tested with the
compiled candidate. An older event-page artifact cannot qualify those packages.
The reports preserve per-package results and leave the overall 50-package pass
rate unset.

For a production candidate, set `FLOORP_TEST_CANDIDATE_MANIFEST` to the absolute
path of its `candidate-manifest.json` before invoking a Floorp corpus wrapper.
The generic browser runner also accepts `--candidate-manifest` directly. The
manifest and its original package must remain together. This mode verifies the
locked source, current Floorp commit, maintained patches, package checksum and
installed files, then checks the running build ID, operating system, ABI and
compiled bindings before executing a suite. Corpus background checks require the
actual native Worker when the signed manifest selected one. Reports retain the
authenticated candidate identity; runs without this option remain historical or
development evidence and cannot qualify a production release.

The candidate CI executes separate core, media, headed permission-consent and
headed screen/window recording groups on each target operating system. The
desktop group must record and decode actual source pixels; merely opening its
chooser does not pass. Local production checks must preserve the browser's
content sandbox, including unsetting any inherited `MOZ_DISABLE_CONTENT_SANDBOX`
development override.

Use `--headed` for the reference when an extension selects a different code path
for HeadlessChrome. It uses the actual browser user agent and requires a
caller-owned display; it does not override browser identity.

The automation corpus can select the three local primary operations without
running Browserflow’s separate account-boundary observation:

```sh
python3 tools/chrome-extension-runtime/test-corpus-automation-floorp.py --browser <compiled-binary> \
  --ids gcbalfbdmfieckjlnblleoemohcganoc,infppggnoaenmfagbfknfkancpbljcca,jnhgnonknehpejjnehehllkliplmbmhn \
  --output /path/to/evidence --timeout 360
```

The wrapper creates a fresh evidence directory under `--output` and always runs
headed. Linux uses the same owned Xvfb/window-manager session described above;
macOS and Windows use their native desktop. UI.Vision and Automa must execute
their real form macros and produce exactly one click. Web Scraper must extract
the exact authored product rows through its real DevTools panel. Only selected
IDs run or contribute operation reports; omitted IDs remain explicitly
unattempted. Omitting `--ids` retains all four operations, including
Browserflow’s account-boundary observation. No account observation counts as a
primary pass.

The controlled media/metadata corpus uses a native decoded WebM, ordinary TLS
verification, private profile certificate trust, and exact private DNS aliases:

```sh
python3 tools/chrome-extension-runtime/test-corpus-media-content.py --ids gebbhagfogifgggkldgodflihgfeippi
python3 tools/chrome-extension-runtime/test-corpus-media-content.py --browser <binary> --ids gebbhagfogifgggkldgodflihgfeippi
```

This Linux harness requires the existing local NSS/bwrap tools and temporarily
binds only loopback port 443. The Chrome child mounts a private NSS database;
the Floorp run trusts the authored CA only in its temporary profile. It does not
change host trust, browser identity, origin permissions, or certificate
validation. The fixtures identify their metadata as authored service data.
Return YouTube Dislike must make an actual native HTTP/TLS request, render the
known count in real document controls, retain it in a new document, and restore
the original controls after uninstall. Both package paths compare all scripts
with the frozen signed package before execution. Store-generated `_metadata` is
excluded only from the reference's private loadUnpacked copy because Chrome
reserves that directory name; extension scripts remain byte-identical.

The DevTools corpus uses actual framework applications and the native registered
extension panel:

```sh
DISPLAY=<private-display> node tools/chrome-extension-runtime/test-corpus-devtools.mjs
python3 tools/chrome-extension-runtime/test-corpus-devtools-floorp.py --browser <binary>
```

The reference opens the genuine headed Chrome frontend and uses native keyboard
input to dock it, then native pointer input to select the extension's panel. The
Floorp counterpart opens Gecko's actual toolbox and selects the registered
extension tool. React Developer Tools must expose the real component tree,
inspect the authored component's `count` prop, and observe `0` becoming `1`
after a native click in the actual React application. Both require actual panel
removal after uninstall. Redux DevTools must expose the actual authored store in
its native State view, observe `count` changing from `0` to `1`, and record the
real `AUTHORED_INCREMENT` action after native app input. Both recipes verify
panel removal after uninstall. The React package's thirteen scripts and Redux
package's seven scripts are verified against its frozen signed CRX; opening the
frontend alone cannot pass.

`build-devtools-fixture.mjs` uses the project's existing React 19.2.6, ReactDOM
19.2.6, Redux 5.0.1, and scheduler 0.27.0 packages. It records their existing
`deno.lock` integrity entries and cached file hashes without changing dependency
files. These cache hashes are provenance for the authored fixture; they do not
claim an independent vendor archive signature verification. The optional
`freeze-frameworks.py` fetches exact official registry archives and verifies
SHA-512 integrity when normal vendor network access is available. Vue 3.5.44
remains a prerequisite until those genuine framework bytes are available; a
replacement hook or a fabricated Vue object is never used.
