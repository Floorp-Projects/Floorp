# Scoped Linux product observation

`validate-final-package-qa.yml` retains its normal Linux/macOS matrix and
source, native, signature and pre-publication checks. Its explicit
`linux_l2_blacksmith=true` selection runs only Linux x86_64's existing shipped
Web Panel UA and Fluent/Flasco probes. `validation_only=false` and original
producer/consumer attempt 1 are required. A hosted preflight rejects
contradictory or repeated selection before requesting Blacksmith. No PR event
starts this paid mode. This increment does not dispatch it or change publishing
gates.

The requested label is `blacksmith-8vcpu-ubuntu-2404`, observed in the original
[Runtime run 38807044655](https://github.com/Floorp-Projects/Floorp-Runtime/actions/runs/38807044655),
job 116734453851
(`Linux x86_64 / Linux x86_64 Debug / Cold Linux x64 Debug harness`). That job
was **skipped**, with no assigned runner. It establishes a configured label, not
successful execution, availability, GUI capability or this repository's
Blacksmith installation. Runtime builds remain on their existing Actions path;
the normal Floorp package/native records are inputs to the later product QA. No
alternative Runtime-manifest download route is introduced.

The probe processes receive a fixed environment with separate fresh HOME/TMP,
DISPLAY, PATH, locale and Python user-site disabled. Build/signing/publisher
secrets are not passed to them. This is environment minimization, not proof that
a browser cannot read same-user runner control tokens/files or reach the
network. Agent/model execution and complete secret/IPC qualification remain out
of scope.

The fixed probes retain their existing process ownership and timeout behavior.
Each step has a ten-minute cap inside the thirty-minute job cap. Provider loss
or a job timeout can prevent final upload; no successful observation then
exists. The observer additionally rejects raw PASS with SIGKILL or unconfirmed
direct host exit. Existing helpers do not certify all descendants/ports are
gone; the observation explicitly leaves those properties UNVERIFIED. It never
uses broad process-name killing. The disposable provider job is the lifecycle
boundary.

A separate hosted observer waits for the native job's terminal outcome and reads
original-attempt job/step metadata. It verifies the exact raw artifact ID, name,
SHA256, run and source, then reads the bounded ZIP without extracting it. Both
reports, their native record, shipped identities and nonempty browser logs must
match the exact package/record IDs and digests from the original producer.
Original native verification job ID/attempt and consumer job ID/attempt/label
are retained. Missing, ambiguous, skipped, foreign or incomplete evidence
blocks; an original failure survives clean report data and a later retry.
Archive bytes and per-member hashes bind the original reports/logs.
Snapshot/receipt JSON alone does not authenticate an arbitrary caller: a
consumer must authenticate the original Actions workflow revision and terminal
jobs through the provider.

Artifacts include run/attempt in their names, use no overwrite and retain the
original files for 14 days. The observation artifact's own ID/digest is provider
metadata created after upload, not a self-issued field in its contents. Expired
or deleted evidence cannot qualify later reuse. `concurrency` limits overlap; it
is not a historical claim ledger. A different consumer run may repeat the
package, so global first evaluation remains UNVERIFIED. Host-loss durability,
cross-controller exactly-once and indefinite retention are not claimed.

The only positive status is `PASS_SCOPE_ONLY`, for these two observed probes.
Every status retains `publicationAuthorized:false`; compile/profile, all
targets, Agent and release qualification are not granted. The stricter
root/shared-claim pilot remains unchanged and is not a prerequisite of this
separate observation. The existing
`bundle -> final QA -> manual approval -> fresh revalidation ->
publish` policy
is unchanged; this observer is not connected to a publisher.

Host contracts use synthetic provider/product records and an owned Python CLI:

```sh
python3 -B -m unittest discover -s tools/linux-l2 -p 'test_*.py' -v
```

They do not execute a product browser or qualify Blacksmith. Actual execution
requires an authorized fixed product input and the existing billing limit; no
new credential, shared filesystem or dedicated VM is required by this mode.
