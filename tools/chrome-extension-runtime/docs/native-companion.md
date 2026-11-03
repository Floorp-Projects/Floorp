# Official native companion qualification

The frozen Video DownloadHelper package is Chrome ID
`lmjnegcaeklhafolokijcfjliaokphfk`, version **10.6.1.2**. Its original
manifest has no `nativeMessaging` permission. The vendor explicitly states that
[v10 does not need the companion](https://github.com/aclap-dev/vdhcoapp/blob/f3736cee7013d4f9003884d9f8077c4c16d2ee4f/README.md).
It contains its own LibAV WASM download engine.

The frozen corpus selection originally asked this package to save media through
the companion. Reports preserve that selection in `primaryOperationAsSelected`,
describe the actual v10 operation in `actualAdvertisedOperation`, and record
`nativeCompanionApplicability: "not-applicable-v10"`. The denominator and frozen
package are unchanged. No v10 result establishes native-host coverage.

`test-corpus-downloadhelper.mjs` and
`test-corpus-downloadhelper-floorp.py --browser <candidate>` drive the unchanged
vendor UI: play an authored local MP4, open the native extension action, accept
the normal vendor lawful-use terms, and select Download. A pass requires a real
saved file containing video and audio of the expected duration, plus identical
decoded first video frames. The Floorp run verifies that every signed script
survives production CRX conversion unchanged and requires a native extension
Service Worker. No account or premium entitlement is supplied.

## Separate companion probe

The supplemental probes contribute **zero** to frozen-corpus scoring. They use
the official legacy Linux companion **2.0.19**, published at
[the vendor release](https://github.com/aclap-dev/vdhcoapp/releases/tag/v2.0.19).

| Artifact | SHA256 |
| --- | --- |
| `vdhcoapp-linux-x86_64.tar.bz2` | `97c43b20fa36dcfbf468c1b652071bc200bd676339b8375065b2d7f93fc958bd` |
| Extracted `vdhcoapp` executable | `069d434b094a30fad059bde6b44dc50310ff7c4771ce7d5aaf29fed3b3c7e0b1` |

The archive matches the vendor's HTTPS-published
[`checksums_v2.0.19.json`](https://github.com/aclap-dev/vdhcoapp/releases/download/v2.0.19/checksums_v2.0.19.json).
The vendor provides no detached Linux signature. The executable reports
`net.downloadhelper.coapp`, version `2.0.19`, Linux x86_64 and the bundled ffmpeg
version. The GPLv2 license is included in that archive and its source release.
These vendor files are installed only in the ephemeral test environment and
are excluded from Floorp packages, artifacts and installation flows.

Before an official `vdhcoapp install --user`, confirm that none of its target
registrations already exists. Preserve the installer's JSON bytes and record
its output paths; never overwrite a pre-existing host. Its real Google Chrome
manifest authorizes the original Chrome ID, whereas its Firefox manifest
authorizes the legacy Firefox IDs. Both registrations remain present during
the test. Use the official `vdhcoapp uninstall --user` after testing to remove
only those test-created registrations.

The Chrome command is:

```sh
node tools/chrome-extension-runtime/test-vdhcoapp-reference.mjs --media /absolute/authored-video.mp4
```

It creates an explicitly authored unpacked developer extension using the
original public manifest key, calls the real `chrome.runtime.connectNative`,
and invokes the vendor's actual `info`, `fs.open`, `fs.write` and `fs.close`
protocol. It copies the exact official registration JSON into the private
Chrome user-data directory, where that browser looks for user hosts. A pass
requires the real saved MP4 to match the authored bytes and remain playable.
This fixture is not represented as a signed vendor extension.

For Floorp, run `test-vdhcoapp-floorp.mjs` with `run-browser-tests.py`, setting
`FLOORP_COAPP_OUTPUT` to a private evidence directory and `FLOORP_COAPP_MEDIA`
to the authored MP4. This probe uses an explicit authored parent context with
the production `NativeManifests` and `NativeApp` modules. It validates native
discovery, exact Chrome-origin authorization, real process launch, 39 framed
RPC exchanges for the authored fixture, and byte-for-byte file persistence.
It does not claim extension API binding or permission-prompt coverage.

`test-vdhcoapp-api-floorp.py --browser <candidate> --media /absolute/authored-video.mp4`
runs a separate public API qualification and validates the saved media with
`ffprobe`. The browser suite uses the same environment variables. It installs explicitly authored temporary
MV2 document and MV3 native module Service Worker extensions and calls their
actual `chrome.runtime.connectNative`. Its negative cases require denial without
`nativeMessaging` and for an unauthorized Chrome ID in both contexts, plus denial
for an ordinary Gecko caller. That control is unauthorized in both official
registrations; the separate five-group native-host suite establishes namespace
selection independently. Each authorized original-ID context must save the
authored MP4 through the official process and preserve its bytes. The Worker
qualification requires a compiled native Service Worker background; a document
fallback cannot satisfy it.
This suite tests developer-install permission declaration and the real API
binding; it does not test a persistent-install permission prompt. A generated
suite or a passing parent-backend report does not establish a pass for this
separate qualification; retain its actual browser report.

The dual registration exposed a genuine lookup defect: the Firefox allowlist
previously blocked an independently authorized Chrome caller. Chrome mode now
uses only Chrome/Chromium registration; ordinary Gecko callers use only Firefox
registration. Denied or malformed higher-precedence Chrome hosts still fail
closed, and authorization never falls through to another browser namespace.
