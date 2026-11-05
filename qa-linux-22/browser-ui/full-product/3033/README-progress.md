# PR3033 local product candidate (in progress)
Head e7dda31f660bb865c2ea8a505ece44875796970f. Independent checkout /workspace/product3033/noraneko; matching Runtime source 8965ffc346d426ccdf919bd18585e4d4a0c21654 in sibling runtime. No shared checkout edits, CI dispatch, release, upload or native compilation.

Complete `feles-build build --phase before-mach` passed. Runtime upstream/common/packaging patches applied per existing workflow. Version prerequisite `feles-build misc writeVersion` creates 12.21.1@159.0.1. Configure passed. Existing artifact build in progress using verified daily-1179 floorp-linux-x86_64-moz-artifact.tar.xz sha256 cb9b01283d5ce3167f4ba046ee7b39ae9d98a692e1beba73bd9f85f460157772.

The repository preference override step includes xpinstall.signatures.required=false and is intentionally omitted under the user's explicit requirement to retain verification. Record candidate signature default/user value without mutation. This build cannot claim byte identity with an official package.

Candidate regression uses candidate-probe.py without source CSS or style injection, only native gBrowser split/group/window APIs. Packaged assets and product archive hashes will be retained.
