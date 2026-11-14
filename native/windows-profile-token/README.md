# Windows private profile token storage

This is the native backend for explicit dev/test profile token advertisement. It is not a new authentication authority: it stores only the browser-generated OS API credential, never a Google grant, and does not authorize a runner or MCP client. Product-default opt-out behavior stays compatible.

## Security contract

The backend accepts only a native ProfD match on a local fixed drive. It opens each component through pinned directory handles, rejects all reparse points, rejects multiply-linked leaves, and requires the current process-user owner. Before secret bytes are written it installs and independently checks a protected owner+SYSTEM full-access DACL on the exact opened objects. It never propagates ACL inheritance into existing children.

A live 128-bit file identity and volume anchor binds subsequent writes and deletion. Foreign replacements are not overwritten or removed; ordinary profile IO and CRT readers remain usable. Missing leaves make cleanup idempotent. Normal object destruction retains the compatible private advertisement; explicit cleanup withdraws it.

The claim is protection against other users/ACEs, not isolation from processes with the same SID or an administrator. Writes are in-place and are **not content-atomic**; a concurrent same-user reader can see partial bytes and must not treat them as authentication success.

## Native host tests

On Windows with CMake and MSVC available:

```powershell
cmake -S native/windows-profile-token -B _dist/fdh/windows-profile-native-build -A x64
cmake --build _dist/fdh/windows-profile-native-build --config Debug
ctest --test-dir _dist/fdh/windows-profile-native-build -C Debug -V --output-on-failure
cmake --build _dist/fdh/windows-profile-native-build --config Release
ctest --test-dir _dist/fdh/windows-profile-native-build -C Release -V --output-on-failure
```

The fixtures operate only on synthetic files in a newly created temporary tree. They use real kernel junction, hardlink, ACL, file-ID and sharing operations; they are not Gecko API mocks. The separate built-module tests verify only capability routing and lifecycle behavior.

## Runtime integration and qualification

The privileged main-process XPCOM service exposes ABI version 1. Native policy independently requires the explicit opt-in and established dev/test mode before directory lookup; the expected root must equal native ProfD. Cleanup uses captured handles, not current preferences or a fresh directory-service lookup.

The adapter checks native capability and exact ABI **before dirsvc or generic IO**. Missing, unknown or incomplete native code fails closed without falling back to Home. Old prebuilt Runtime artifacts therefore remain unsupported safely.

Regenerate the common Runtime patch from the canonical source against an isolated locked Runtime checkout or the verified source fixture:

```sh
deno run --allow-read --allow-write native/windows-profile-token/generate-runtime-patch.ts --source-root /absolute/pinned-runtime
deno test --allow-read tools/src/fdh_windows_profile_token_patch.test.ts
```

The generator checks the pinned upstream build-file blob before writing the fixed common-patch output. It never edits the Runtime input. Apply the patch **before mach build** in the Runtime producer. Applying it after prebuilt artifact download, successful source patch application, standalone CTest, and fake-Gecko tests do **not** prove that xul contains the component. A fresh compiled Runtime with bound patch/provenance and an actual owned dev/test browser publication/ACL/cleanup run remain mandatory before native browser qualification or shipping claims.
