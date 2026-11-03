# Bitwarden primary operation with an owned local vault

This harness uses the frozen signed Bitwarden CRX, genuine Vaultwarden 1.37.4
pinned by its official container digest, and an authored HTTPS login form. It
creates an ephemeral local account through the official Web Vault signup UI.
The extension then logs in, saves an authored login, and fills initially empty
username/password fields through its actual UI. No extension JavaScript or
browser API is replaced.

On the Linux Chrome reference environment, start a private Xvfb display and run:

```sh
python3 tools/chrome-extension-runtime/prepare-bitwarden-vault.py
DISPLAY=:104 node tools/chrome-extension-runtime/prepare-bitwarden-account.mjs --server /absolute/output/server.json
DISPLAY=:104 node tools/chrome-extension-runtime/test-corpus-bitwarden.mjs --server /absolute/output/server.json --recipe tools/chrome-extension-runtime/corpus-bitwarden-create-recipe.json
DISPLAY=:104 python3 tools/chrome-extension-runtime/test-corpus-bitwarden-floorp.py --server /absolute/output/server.json --browser /absolute/path/to/floorp
```

The Chrome recipe expects a fresh Chrome profile and an empty vault. Its native
UI clicks and text-input commands are also accepted as JSON lines on stdin when
`--recipe` is omitted. The Floorp runner uses the same HTTPS URL and genuine
vault, and accepts commands by appending objects to
`floorp-primary/commands.json`; its current native UI is written to `ui.json`.
Use `{"op":"verify-fill"}` to compare the authored page's actual field values
with the private test credential, then `{"op":"close"}` to finish.

The private CA is trusted only inside each test browser: Chrome uses a child
mount namespace with existing public trusted certificates retained, and Floorp
imports it into its disposable profile. Certificate validation remains enabled.
The genuine Chrome UI must run headed: this Bitwarden release detects
`HeadlessChrome` as Safari. The harness preserves the actual browser user agent.

Reports contain package/browser digests, canonical unchanged-script hashes,
the authored URL/source hash, native UI steps, and comparison booleans. The
private credential file has mode 0600. Stop the owned container afterward:

```sh
python3 tools/chrome-extension-runtime/prepare-bitwarden-vault.py --stop /absolute/output/server.json
```
