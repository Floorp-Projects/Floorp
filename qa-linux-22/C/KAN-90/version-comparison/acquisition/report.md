# Public12.17.2 artifact acquisition

**Exact public archive acquired, checksum verified and extracted without execution.** Existing Flatpak installations/profiles/products were not updated, downgraded or removed. No artifact was published, no Jira write or remote branch push occurred.

## Public release and actual bytes

[Floorp v12.17.2](https://github.com/Floorp-Projects/Floorp/releases/tag/v12.17.2), release API `draft=false`, `prerelease=false`, published2026-09-03T11:58:08Z. Linuxx86_64 assetID542589720, [public archive](https://github.com/Floorp-Projects/Floorp/releases/download/v12.17.2/floorp-linux-x86_64.tar.xz), 99,499,888bytes. Actual downloaded SHA256 **2f59e332766d8ca5e966cc655450a0f1d3cf6e286f9f52172f8bd3caa7858ffc**, matching GitHub's asset digest and historical Flathub manifest. This is the published historical product, not a source reconstruction or old frontend on a newer runtime.

Local executable, **not run by this acquisition task**:
`/workspace/qa-linux-22/C/KAN-90/version-comparison/products/12.17.2/floorp/floorp`.

Actual extracted application.ini: Version12.17.2@155.0, BuildID20260903040334. platform.ini: Milestone155.0, BuildID20260903040334. The file comment says application.ini itself is not the runtime's authoritative embedded application data; root's actual-product comparison should measure the effective build ID/version.

## Public metadata discrepancy retained

Public `hashes.txt`, `linux-meta.json`, `release-manifest-set-v2.json` were also fetched; their SHA256s match GitHub's declared asset digests (`github-verified-metadata.json`). linux-meta says version12.17.2@155.0, but BuildID**20260902101625**, inconsistent with extracted application/platform.ini **20260903040334**. Do not silently substitute the release JSON build ID for the actual archive's files or eventual process metadata.

linux-meta provenance declares Floorp source `38bde3075f197b9a88490875d1534bb79d6159a5`, Runtime `41fb7de3adb7f33223ae5b24b969a93efa29a9aa`; these are declarations from the separately verified metadata, not independently established by running the archive.

## Bounded Flathub historical investigation

[Public Flathub manifest commit46747f209ee88905036ecc9f8933583e975ceaa2](https://github.com/flathub/one.ablaze.floorp/commit/46747f209ee88905036ecc9f8933583e975ceaa2) updates both architectures to12.17.2 and pins the exact same x86_64 archive SHA256. Manifest uses Freedesktop Platform25.08 and Mozilla Firefox BaseApp25.08.

The [public x86_64 build run](https://github.com/flathub-infra/vorarbeiter/actions/runs/33776994508) has success/Build ready status. Its decoded job log confirms exact12.17.2 download, Platform25.08 commit `bd44a6230581917d04f89812a4c21090c304d390edb73995af1c2f9fd8abf4e8`, BaseApp25.08 commit `7d5a63c899050c37535753909520a2f54022d134a176616293dbdce64362a2d5`, and **test** app commit `63ecd176ac8086e8921f0441ba4ebc7cb8639138eed670d91732e4aff4fe096d`. A test-build commit is not proof of a signed stable published deployment.

A new isolated cache (under acquisition/flatpak-home) used normal Flathub GPG/TLS configuration. `remote-info --log` returned only6 current-history commits, oldest2026-09-30; no12.17.2 stable commit. A single explicit retrieval attempt for the known test-build commit returned **404**. No signature bypass, no repackaging, no downgrade of existing installation. A signed stable12.17.2 OSTree deployment commit was **not identified or acquired** by this bounded search; the404 for a test commit does not prove every historical stable object is absent.

Useful next comparison: run acquired12.17.2 archive and current12.21.1 archive in separate profiles through the same actual Thunderbird portal-origin fixture. Explicitly label archive packaging and OS differences from the original Fedora44/Flatpak report. Do not call it an exact historical Flathub reproduction.

Small evidence: `summary.json`, `archive-acquired.json`, `github-verified-metadata.json`, release metadata JSONs, manifest YAML, relevant build-log excerpts and historical-cache retrieval result. Full public API snapshots/logs remain local for audit; the export list excludes author/creator fields, caches, binaries, profiles and all credentials.
