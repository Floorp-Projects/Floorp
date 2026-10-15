# Single Linux package intake

This is a separate QA3 boundary for one final production-mode Linux x86_64
package. It does not relax the existing exact 16-file release bundle or the
five-record native verification set. Linux `unsigned: false` describes the
production packaging path; it does not assert a Windows/macOS code-signing
certificate.

`qa3-linux-package-producer.yml` runs common source QA once, validates an exact
Runtime manifest, calls the existing Linux package implementation with
`skip_signing: false`, and uses the existing native `--full-version` verifier.
It then uploads a closed index binding their original identities. It has no
release, tag, PPA, updater or Portable job. A completed successful producer P
must precede independent consumer C. This avoids making a job wait for its own
parent workflow to complete. Runtime builds remain on GitHub Actions; any new
downstream native consumer must use the separately qualified Blacksmith route.

The `signed_validation` input is exposed to `workflow_call` as well as the
existing dispatch contract, defaulting to false. No existing production
default-branch restriction is removed. The nonpublishing wrapper's validation
mode may inspect an exact development Runtime cohort; this does not qualify
that cohort, change `floorp-runtime.lock.json`, or transfer historical 157 L1
evidence to a 158 product.

`producer.py` validates the closed index. `intake.py` additionally requires an
external caller's authenticated original P/C/Runtime snapshots, complete job
pages, exact provider artifact metadata and original archives. Candidate JSON
cannot supply that authentication. The original Runtime manifest is mandatory;
an API listing, a public release repack, a prior log or a provider digest alone
cannot replace its bytes. All original attempts must be 1. The initial intake
requires the controller and Floorp product source to be the same full SHA.

The archive directory contains `index.zip`, `source.zip`, `provenance.zip`,
`package.zip`, `nativeRecord.zip` and `manifest.zip`, each with its original
provider SHA-256. ZIP layout, directory size, file count/type, expansion sizes,
duplicates, links and traversal are checked before extraction. Original JSON
is parsed, hashed and saved from one bounded read; archives are copied once
into private creation-only files before any digest/extraction checks. The source
archive retains the raw contract exit code and all fixed source reports. The
same closed Runtime/native parsers validate manifest, fresh API snapshot,
producer normalization and the exact package cohort. The final tar member is
additionally identified by size/SHA-512, safely expanded and checked for native
ELF architecture, application identity and the packaged overlay BuildID.

```sh
QA3_DENO_FOR_CONTRACTS=/trusted/deno python3 -B -m unittest discover -s tools/linux-package-qa -p 'test_*.py' -v
python3 -B tools/linux-package-qa/intake.py \
  --request /trusted/request.json --provider /trusted/provider.json \
  --archives /trusted/original-archives --deno /trusted/deno \
  --output /new/intake-output
```

Successful intake returns `INPUTS_MATCH_SNAPSHOTS_NOT_EXECUTED` and
`providerAuthentication: EXTERNAL_CALLER_REQUIRED`, with both native
product and compile/profile qualification `UNVERIFIED` and
`publicationAuthorized: false`. It does not launch the product.

The separate `capture.py` and `qa3-linux-package-intake.yml` now provide a
metadata-only controller entrypoint using the existing PAT's REST read access.
The fixed API origin/paths exclude archive endpoints and redirects; requests,
pages, bytes and elapsed time are bounded, and original API bytes are retained.
Complete P job/artifact inventories and the active original C are checked.
HTTP framing must be complete before JSON is accepted. A later connection-close
failure cannot replace the original HTTP/read failure or produce a successful
capture. Partial received bodies and secondary cleanup failures stay separate.
The CLI checks a 120-second monotonic deadline between reads and uses socket
timeouts; DNS resolution also needs the workflow's external GNU `timeout`
(TERM at 150 seconds, KILL after another 5 seconds). An external termination can
leave the final receipt absent, which remains incomplete evidence and blocks
qualification; retained files are not promoted to a completed capture.
Its declared index is used only to select metadata: original index/member bytes
remain unauthenticated until the separate archive intake. Successful capture is
`AUTHENTICATED_METADATA_CAPTURED_NOT_EXECUTED`, with no archive acquisition,
browser launch or product qualification. The workflow has not been dispatched.
It runs only metadata on standard GitHub Actions; a future native consumer must
still use the qualified Blacksmith path. No new credential or API write scope
is added. The root-issued sealed worker adapter, original archive acquisition
and qualified first-evaluation backend are still missing. Do not rename an
intake result to the old prepared schema or invent a bundle/manifestSetId to pass
the old worker.

The old root supervisor, DynamicUser/cgroup limits, token-free worker and shared
NFS/qualified-filesystem claim requirements remain unchanged. A Git ref or
Actions concurrency group alone does not establish durable first-FAIL history.
No native run may start without that boundary. Product evidence must later bind
the real six-case result, original failure and raw logs, complete owned cleanup,
all required targets, Runtime qualification, exact final bundle and sidecar.
The release path still needs `bundle -> final QA -> manual approval -> fresh
evidence revalidation -> publish`; these modules do not activate that path.

Current execution blockers are recorded separately from source implementation:
Runtime run 38551492598's manifest artifact 11916387386 returned Forbidden with
zero received bytes. Do not retry that family through CI, another route,
principal or repack. The existing earlier forbidden artifact families also stay
blocked. Floorp Blacksmith app scope and billing were unavailable through the
current read connection, and no qualified shared claim store was established.
This change dispatches no producer/native job and creates no new credentials,
rights, service, mount or publication.
