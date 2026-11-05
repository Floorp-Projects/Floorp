# Unpublished candidate build route

Existing source route: `.github/workflows/package.yml` at984509d: production `feles-build build --phase before-mach`; matching Runtime configure/artifact build with MOZ_ARTIFACT_FILE; `feles-build build --phase after-mach`; normal local `mach package`. No Actions dispatch, artifact upload, signing changes or release publication is requested.

Runtime daily-1179 source8965ffc346d426ccdf919bd18585e4d4a0c21654 and public x86_64 artifact707330202 are pinned in shared/runtime-input.json. GitHub asset SHA256 verification succeeds; matching application metadata confirms159.0.1/build20261103153232. This uses the existing artifact build design, not a native Gecko rebuild. The source-test lock157 is a separate input and is not represented as159.

3033 and3031+3034 build in separate detached Floorp/Runtime worktrees and objectdirs. Shared Floorp/Runtime checkout content and branch refs are not changed. Production assets are rebuilt as a whole, rather than manually replacing a calculation or CSS fragment. Final candidates and tarballs stay local. Curated source/scripts/logs may be published on the previously authorized QA branch.

PR2877 comment receipt: https://github.com/Floorp-Projects/Floorp/pull/2877#issuecomment-6663548952 . Head and existing comment were rechecked before posting; there was no duplicate.

## Observed outcomes

Both candidates completed before-mach all production assets, matched Runtime artifact build and after-mach. 3033assembledX11 and separately labelled optional-aux packageWayland pass adoption without CSS replacement. 3031+3034assembled product29721 passes exact returning gesture once with no wrong action/menu. These are full production-asset assemblies using prebuilt native code, not native Gecko compilation.

Initial strict `mach package` failed for both because genuine matching XPT auxiliary input was absent; this was resolved by canonical source generation as recorded below. The same-source Runtime CI41664957250 artifact13272530069 was downloaded through the connected service, digest verified, and its ZIP manifest inspected; it includes exactly the same native tar as the release, host tools, application.ini and provenance, with no XPT companion. All13release asset names are also recorded in shared/ci-input-manifest.json. No fabricated XPT or false strict-package pass. 3033's separate optional-aux archive uses existing workflow handling and is explicitly not strict success; strict checks were restored. 3031+3034never changes the empty-input check.

The preference override phase that would disable addon signature checking is omitted. Actual product tests record signature checking true (no user override), and combined gesture product sandbox6 without user override. Official distribution/default identity is not claimed. Candidate products and local tar stay unpublished.

## Current result: strict package succeeds for both

Same patched Runtime source plus unchanged artifact configuration, evaluated by BuildReader, selects155modules/801IDLs. The existing unmodified xpidl-process.py generates all genuine XPTs without C++/Rust compilation. The same903dependency hashes and selected modules/inputs match both candidates, so outputs are reused once. Both machpackage commands now exit0 with canonical packager.mk byte-identical to gitHEAD, including every --error-if-empty check. Full evidence and reproduction: xpt-generation/report.md, strict-results.json. Native-compiled XPT byte identity is not independently proven.

The final3033strict product matches all56entries of the earlier tested extracted product. The combinedstrict product matches all1115production assets and26ELF native files of the earlier tested assembly. Redundant same-payload UI runs are avoided. Signed-release/default equivalence remains unqualified; no binary is published.
