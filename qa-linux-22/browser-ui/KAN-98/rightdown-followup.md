# KAN-98 right-down follow-up: input sampling sensitivity

No further product code changes. Draft #3031 remains a limited fix for returning gestures.

## Fresh actual-product comparison

Official unmodified Floorp 12.21.1 / Gecko159.0.1 / BuildID20261103153232; Debian13.6, Xvfb :92, DBus, new profile. W3C pointer input, not physical input. The same product protections stay enabled. Separate enabled preference is true, and the captured configuration explicitly contains `["right","down"] -> gecko-open-new-tab` plus single-up control.

| Input | Repeats | Action label / New Tab |
|---|---:|---|
| Two 200px right/down moves, 100ms per move | 3 | 3/3 success |
| Two 200px right/down moves, 800ms per move | 3 | 3/3 success |
| Explicit 10px steps along both legs, 40ms per step | 3 | 3/3 success |
| Single-up control, 100ms | 3 | 3/3 success |

`rightdown-29321.json` contains config, exact requested paths, received events, labels, and before/after tab counts. Unique screenshots `rightdown-29321-*-held.png` show the first repeat for each case. Unlike the earlier single failed trial, the new nominally sparse 100ms requests delivered intermediate samples within the initial 10–50px direction-sampling band. W3C move endpoints alone do not determine the resulting mousemove sample sequence.

## Replay of captured traces in Gecko

`rightdown-replay-29322.json` replays both earlier failures and all nine fresh right-down successes through the pinned main recognizer source, inside actual Floorp chrome. The replay removes duplicate events and applies the controller's 1.5px point filter. It uses actual config thresholds: minimum score 0.73, activation/sequence distance 5px.

- Earlier unmodified product failure (`gesture-29301.json`): initial direction becomes `downRight`, extracted sequence `[downRight]`, recognition null.
- Earlier returning-distance-patched product failure (`gesture-29314.json`): initial direction becomes `downRight`, extracted sequence `[right]`, recognition null.
- All nine fresh successes: initial direction `right`, sequence `[right,down]`, recognition `right-down`, score1.

The earlier failed trails jump beyond 50px before the first captured held movement. `calculateMeanDirection` finds no 10–50px sample, then falls back from the end of the whole trail, yielding the overall diagonal displacement instead of its initial rightward leg. Sparse samples also fail to establish the complete direction sequence. The $1 fallback checks the computed first direction against the configured variant and rejects it. These source-level behaviors explain why the recorded sparse trajectories fail while denser ones succeed.

**Classification:** sample-sensitive recognizer limitation reproduced with captured W3C trajectories; the previous right-down non-firing was not a missing configuration, and it is not an unconditional inability to recognize multipart gestures. The experiment does not establish how frequently real mouse hardware on Arch/Sway/Wayland produces such gaps. This finding is outside the returning-distance change in #3031. Keep KAN-98 open with physical input, original compositor/OS, and sparse-input robustness remaining.

## Reproduction

- `../kan98-rightdown.py` and `../kan98-rightdown-run.py`: actual product density/speed comparison. `PYTHONPATH=../python python3 ../kan98-rightdown-run.py floorp 29321 rightdown` from this directory.
- `build-replay.cjs`: bundle the pinned recognizer with diagnostic-only export of `calculateMeanDirection`, without changing repository source.
- `../kan98-rightdown-replay.py` and `../kan98-rightdown-replay-run.py`: load the bundle in real browser chrome and replay the saved traces.
- Raw original input files `../gesture-29301.json`, `../gesture-29314.json` are required by the replay.

## CI #3031 snapshot

PR head `dbafd082f1f315a49575446e7094177450b1882b`, Draft/open. Run `42045200076`: smoke passed, Ubuntu ARM/Windows/macOS Runtime/helper jobs passed; these OS jobs skip the browser suite. Ubuntu24.04 x64 is still running the combined browser suite against the locked Runtime at this snapshot. Its upload-evidence step is pending, and the artifact API currently returns an empty list. No full browser-suite success or artifact verification is claimed. `ci3031-snapshot.json` preserves the observed jobs and steps.
