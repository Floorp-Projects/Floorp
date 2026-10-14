# Same-profile restart development QA

This is a separate ordinary development contract with exactly two ordered cases:

1. Normal application quit and restart restores the active workspace, selected
   tab and visibility of the original four synthetic tabs.
2. Closing the last visible tab exits the application, then restart restores the
   original hidden user tab once, without restoring the explicitly closed tab or
   creating duplicate startup tabs.

Each case creates one fresh private profile at `_dist/profile/test`, then boots
the locked native browser twice using that same directory. It does not copy,
reseed, or write a session snapshot between boots. Native restore promises must
finish before observation. The original clean `sessionstore.jsonlz4` is read and
preserved; the second boot must consume those exact bytes into
`sessionstore-backups/previous.jsonlz4`. URL and workspace membership are
checked against the first boot's preparation record.

The listener must belong to the owned native executable and profile. `/proc`
start ticks distinguish PID generations. The original browser and observed
descendants, including zombies, and its loopback Marionette port must disappear
before the next boot. Every boot requires a successful normal exit and clean
session file. Forced cleanup is reserved for owned failed processes and the
development servers; it cannot qualify a browser quit. Deadlines, framing, reply
IDs, expanded session bytes, and evidence logs are bounded. There is one
original attempt, no case retries, and a missing or skipped case fails closed.

The first boot uses `feles-build test --own-native-session` with Linux `setsid`.
Every native root must own its session, including reparented children; servers
have a separate scope. Its development servers remain running for the later
three native launches, retaining the compiled overlay BuildID2. Each of the four
boots receives a fresh existing one-shot test control selecting one explicit
nonexistent target, with matching legacy prefs. Native bootstrap must consume
that boot's exact run ID and complete with zero discovered or executed colocated
suites, no abort or error. This is an isolation guard only; zero bootstrap
suites are never counted as passing restart cases. Without it, test-mode startup
defaults to all colocated suites and could contaminate state. The native
children receive an environment allowlist without GitHub tokens. All state is
synthetic; no real user profile is read. The two case profiles are removed after
normal shutdown, and all owned servers are cleaned up.

Run the limited workflow manually with `workspace_restart_only=true`; selecting
more than one limited mode is rejected. The existing six, preservation four, and
flow three contracts and default native matrix retain their own behavior. The
controller requires GitHub attempt 1 and exact checked-out source identity.
Local protocol, ownership and receipt unit tests are:

```sh
python3 -B -m unittest discover -s tools/workspace-restart-development -p 'test_*.py' -v
node --test tools/workspace-restart-development/test_restore_oracle.mjs
```

The unchanged pure `restore-oracle.js.txt` factory is injected by
`browser_script()` into the native phase and evaluated directly by Node tests.
Direct positive/negative cases cover combined duplicate startup URLs, unknown
URLs, duplicate/missing/misattributed retained tabs, the closed tab, changed
native configuration and post-selection behavior. Host tests also execute the
actual assembled browser phase with synthetic API fixtures, so removing its
shared predicate call cannot silently bypass the unknown-URI regression.

The host rechecks the creation-only profile marker before a phase. Native
profile/PID ownership is checked before tab reads or prepare mutations. A failed
phase captures only that owned synthetic window's URI/current URI, workspace,
visibility, selection, native new-tab/home configuration and bootstrap summary.
The host gates a bounded dedicated `*-failure-diagnostic.json` before preserving
the phase trace. Foreign or malformed observations are discarded, and a
diagnostic type/size/write failure is recorded separately without replacing the
original failure. Phase error strings contain the error message, not snapshots.
No user profile, environment, cookies or complete preference dump is collected.

The third original failed run `38513152121` did not record its exact unexpected
URI; its cause remains unconfirmed. A later run's URI observation cannot
identify it retrospectively. The separate production duplicate-collapse
configuration candidate is unchanged and is not treated as that run's cause.

The receipt, browser phases, bounded original logs and original clean session
files form a separate one-day artifact. `PASS_DEVELOPMENT_ONLY` does not qualify
the production artifact, Debug Runtime, compile profile, signing, or
publication. Downloading and inspecting the raw archive is a separate evidence
check; an access refusal must remain an explicit limitation, without retrying
another route, principal, or artifact family.
