# Publication contract model

This pure, nonpublishing model checks five separate inputs: fixed controller
policy, candidate subject/evidence references, pre-approval snapshot, bound
manual approval and fresh post-approval snapshot. Every input is an offline
fixture/declaration. A snapshot here does not authenticate itself. Unknown
fields such as `authenticated: true` are rejected.

```sh
python3 -B -m unittest discover -s tools/qa3-publish-contract -p 'test_*.py' -v
```

The positive decision is only `READY_FOR_AUTHORITATIVE_REVALIDATION` with
`stage: contract-model`, `externalAuthentication: REQUIRED`, both product and
compile/profile qualification UNVERIFIED and `publicationAuthorized: false`.
There is no publisher import, callback, API client, browser or command runner.
All positive/negative fixtures exercise a fake mutation surface for draft/tag,
assets, updater, Portable and PPA and require zero calls.
This observes only the fixture caller; it does not measure the unchanged real
publish workflows end to end.

The model requires all five native targets and every artifact member/case in
the fixed policy, source-once, a separate Debug applicability qualification,
PGO/profile lineage and Linux ARM production-opt, complete original first
evaluations, owned cleanup and exact final bundle/evidence digests. The
first-evaluation key uses repository, member bytes, target and a fixed suite;
new C/controller/producer/bundle ID/filename/store namespace cannot reset it.
Missing, FAIL, skip, retry, unknown, incomplete, expired or revoked evidence is
blocked. Approval binds the subject, policy, gate run/environment and exact pre
snapshot; the post snapshot must follow that approval and retain every evidence
and history binding. Expiry at now and stale/equal-time snapshots are blocked.

The sixteen-member digest inventory and verification flags are model
preconditions, not substitutes for running the unchanged production v2 byte
verifier, authenticating signed/notarized native records or reading a qualified
durable history service. Fixture case IDs are synthetic and are not a complete
Floorp product catalog. This model cannot be promoted to production by changing
a flag. It does not wire or change any existing publisher or approval gate.
Real producer/sidecar qualification, issuer/history authentication, fresh API
acquisition, qualified backend and all-target final product execution remain
required before implementing an authoritative publisher adapter.
