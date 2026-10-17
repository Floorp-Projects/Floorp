# Source improvements independent of native qualification

The shared `tools/src/browser_connector.ts` now makes one bounded dial/session
attempt, completes partial UTF-8 writes, validates bounded canonical framing and
exact response IDs, rejects overlapping commands and retires failed connections.
Deadlines include writes, reads and a monotonic completion check. Pending I/O is
reaped after Deno TCP close; a kernel-blocked runtime still needs external job
teardown. Frame/command limits are 16 MiB/1 MiB. Three existing gesture QA
callers and `dev-tool` use this client. The gesture callers retain the original
error, collect cleanup errors and print PASS only after cleanup. Host faults
validate this path; they do not establish real native input, memory safety or a
secret-free Agent worker. General `dev-tool eval` is not an Agent action API.

The [development inventory](../../libs/test-inventory/README.md) is consumed by
the current runner and browser loader. It makes the current origin/migration
denominator visible without claiming packaged eligibility or full Mozilla
coverage. A missing/unknown required browser source, ordinary empty execution,
duplicate or incorrectly classified result fails. Required scoped files come
from the closed catalog, independently of source discovery. Unselected missing
files remain observations. Explicit leased restart bootstrap isolation keeps its
separate `NOT_EXECUTED` purpose and can run no automatic suites beside restart.

The existing final-package Web Panel UA probe adds
`web-panel-ua.redirect.mobile-on` and `web-panel-ua.redirect.mobile-off`. Each
uses a unique run/case path and two fixed redirects on an exact loopback origin.
The server records three ordered real HTTP UAs; the oracle checks every hop,
destination, status, Location and inner browser identity. Normal-tab reloads and
other-panel navigation independently check desktop UA after each case. The
original direct-navigation/reload/UI-toggle and final native identity checks
remain. Server records, handler concurrency, socket I/O and request counts are
bounded. Final revalidation catches duplicates or wrong UAs arriving after an
early observation; errors retain their first cause.

Host tests drive the actual HTTP server, oracle and fixed navigation JavaScript.
These new redirect cases remain **NOT_EXECUTED on native products** until an
authorized run uses the new probe revision and exact final artifact. Historical
12.20.2 results cover the old probe only. A host PASS does not fill Windows,
Linux ARM, Mac feature, Debug harness, installer/update/MAR/PPA/Portable/stub,
qualified store or publisher gaps. No manual/native dispatch is part of this
change. The existing signing, approval and publication gates are unchanged.
