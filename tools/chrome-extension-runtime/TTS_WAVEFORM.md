# Browser TTS waveform verification

`test-browser-tts-waveform.py` records a real operating-system loopback while
running **only** `AuxiliaryNative.test.mts` in an authenticated native
candidate. It never invokes a command-line synthesizer, generates a tone, or
replaces browser speech events. Run it after Worker and media suites have exited
so their audio cannot contaminate the recording.

## Evidence available so far

The helper preflight ran on actual Linux, macOS, and Windows. Linux captured a
quiet real Pulse monitor; macOS compiled the Objective-C helper with its native
SDK and enumerated actual devices; Windows compiled the C# helper with Add-Type
and queried an active native endpoint after the existing signed audio bootstrap.
Those checks establish helper readiness. **They do not establish browser TTS
waveform success or macOS/Windows capture success.**

The exact source hashes, run/commit IDs, artifact hashes, and promotion changes
are recorded in [tts-recorder-provenance.json](tts-recorder-provenance.json).
The original native helper bodies match the versions compiled in that preflight;
their leading comments now describe the verified scope. The Python wrapper also
uses the checkout location instead of the original scratch directory. The
analyzer adds bounded metadata parsing and invalid numeric/interval guards.

The first Windows checkout converted LF to CRLF and failed its source-byte guard
before compilation. The Windows follow-up restored exact Git blob bytes, then
compiled and probed successfully. Source hashes in the provenance file refer to
LF Git source bytes. No compilation success is attributed to the failed
byte-guard attempt.

## Scoped Linux speech regression

`auxiliary/SpeechFragmentsNative.test.mts` tests the Linux SpeechDispatcher
backend through real `chrome.tts` calls. It checks CJK punctuation without
whitespace, original UTF-16 boundary positions and lengths across bounded native
fragments, surrogate safety, one start and final event per task, and queued
cancellation. Resume must retain the paused position and produce new native
speech boundaries **after** its resume event before cancellation is tested.

Fragmentation is enabled only on the privileged Chrome TTS host's own native
utterance through a default-false, Chrome-only opt-in. The regression also
speaks a genuine ordinary native utterance with that opt-in false and verifies
its start/end events. Ordinary Web Speech retains its legacy task path; no
content IPC opt-in is added.

Candidate CI includes this backend-specific fixture only in Linux's `media`
group. `AuxiliaryNative.test.mts` and the separate authenticated `tts` waveform
group remain required on Linux, macOS, and Windows. The waveform group runs only
Auxiliary so other media or fragment tests cannot contaminate its baseline.

Raw developer runs can retain real native events, PCM packets, and diagnostic
waveforms. They remain explicitly unqualified (`nativeTtsQualified: false`) and
do not replace the strict wrapper's matching production package, source, and
candidate-manifest authentication on each operating system.

## Prerequisites

- A completed native production package, extracted executable, and matching
  `candidate-manifest.json`. The wrapper and browser runner both authenticate
  the candidate; an older browser with a matching filename is insufficient.
- Python 3.12, project dependencies for the existing browser runner, and a real
  installed operating-system speech voice.
- Linux: the existing Pulse tools and a selected dedicated `floorp_ci` output
  (`floorp_test` is also accepted for local tests), plus the configured Speech
  Dispatcher backend. The recorder uses that exact output's monitor.
- macOS: BlackHole 2ch selected as actual output, its matching input device, the
  locally compiled helper, and genuine OS audio authorization. The helper checks
  input/output identities and observed PCM format. Consent errors fail the run;
  neither the wrapper nor helper edits TCC.
- Windows: an active default render endpoint and PowerShell. The helper records
  shared-mode WASAPI loopback on the actual `eRender/eConsole` endpoint; it does
  not assume a DirectShow input named CABLE Output. Native mix samples are
  converted to mono PCM16 at their observed sample rate.

The wrapper removes `MOZ_DISABLE_CONTENT_SANDBOX` and `MOZ_HEADLESS` from its
child environment and forces `FLOORP_REQUIRE_NATIVE_CHROME_AUXILIARY=1`. It does
not modify global preferences, default devices, OS consent databases, or the
supplied candidate.

## Invocation

Use a new output directory for each run. On Linux, preserve the existing
`PULSE_SERVER`, `SPEECHD_ADDRESS`, and runtime library environment:

```sh
python3 tools/chrome-extension-runtime/test-browser-tts-waveform.py \
  --browser /absolute/path/to/floorp \
  --candidate-manifest /absolute/path/to/candidate-manifest.json \
  --output _dist/chrome-runtime-tests/browser-tts
```

On Windows, add:

```text
--native-recorder tools/chrome-extension-runtime/tts-windows-recorder.ps1
```

On macOS, compile the test helper first and pass its absolute executable path
using `--native-recorder`:

```sh
xcrun clang -fobjc-arc -O2 \
  tools/chrome-extension-runtime/tts-macos-recorder.m \
  -o /absolute/private/path/tts-macos-recorder \
  -framework Foundation -framework AVFoundation -framework CoreMedia \
  -framework AudioToolbox -framework CoreAudio
```

`tts-windows-recorder.ps1 -CompileOnly` and `-Probe`, and the macOS helper's
`--probe`, are bounded compilation/device checks. Probe modes do not initialize
capture, read audio packets, or speak. They cannot qualify native browser TTS.

## Measurements and artifacts

The recorder starts before the browser. Native packet timestamps are aligned
with the first utterance's actual `speechEvidence.events[].receivedAt` values.
The analyzer compares a preceding 750 ms baseline with the speech interval,
requires nonzero speech RMS/peak and multiple voiced bins, and rejects missing
compiled events, extra test suites, unbounded/invalid packets, loss, or device
changes. Linux reports its bounded receive-clock timing uncertainty; macOS and
Windows retain native clock calibration and packet timestamps.

Artifacts include `capture.pcm`, `packets.ndjson`, native ready/done metadata,
browser logs and candidate report, and `browser-tts.wav` with SHA-256 and
measured baseline/speech statistics in `waveform-report.json`. Packet PCM
contains genuine recorded packets. The derived WAV marks no-render gaps as
silence; positive signal statistics require actual non-silent native packets. A
failed run retains its partial PCM/metadata and `failure.json` and must not be
reported as a pass.

Recording is bounded to 300 seconds and 32 MiB of PCM. Only processes started by
the wrapper are stopped during cleanup.

Host guard checks do not launch a browser, recorder, voice, or tone:

```sh
python3 tools/chrome-extension-runtime/test-tts-waveform-tools.py
```

`tts-helper-linux-preflight.py` additionally performs a short genuine Pulse
baseline capture when explicitly invoked. Its report always labels the scope as
recorder preflight and sets `nativeTtsQualified: false`.
