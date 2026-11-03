# Normal networking in a private test profile

The generic native browser runner defaults to the browser's normal profile settings. External-service corpus recipes can explicitly opt in to the existing process environment:

```sh
python3 tools/chrome-extension-runtime/run-browser-tests.py \
  --browser PATH_TO_BROWSER --environment-proxy --test PATH_TO_SUITE
```

This sets native manual HTTP/HTTPS proxy preferences from `http_proxy`/`HTTP_PROXY` and `https_proxy`/`HTTPS_PROXY`, with `ALL_PROXY` as a fallback. `NO_PROXY` is preserved, and localhost,127.0.0.0/8 and::1 always bypass the proxy for authored fixtures. The current helper accepts HTTP proxy transport; unsupported transports and credential-bearing URLs fail explicitly without printing credentials. Direct/proxy-specific suites should retain the default behavior and control their own private preferences.

The runner imports the ordinary public PEM CA bundle into that disposable profile's native certificate database before loading the test suite. Bundle selection is `SSL_CERT_FILE`, `REQUESTS_CA_BUNDLE`, `CURL_CA_BUNDLE`, then Python's normal system bundle. Host trust, certificate validation and browser security preferences are unchanged. Reports include bundle hash/count and nonsecret proxy transport/port metadata.

An external service error remains a service error. A successful native HTTPS response carrying an upstream503 proves transport setup, not the extension's primary operation. Existing private authored-CA fixtures continue to import their own CA and use their recorded loopback routing.

For the frozen Linux Chrome reference, `private-browser-trust.py --output RUN_DIRECTORY --browser CHROME_BINARY` creates a process wrapper retaining normal public NSS roots. `--ca AUTHORED_CA` is optional. Pass the same run directory, or one of its descendants, to `ChromePipe`; the wrapper keeps the rest of the filesystem read-only. It never copies private keys or changes host trust.

`test-corpus-identity-prerequisites.mjs` uses that wrapper, explicit environment proxy/bypass arguments and a private headed display to inspect1Password, LastPass, Proton Pass and Checker Plus through their genuine native actions. It verifies every vendor script against the frozen CRX and stops before authentication. Its results are prerequisite observations with primary operations not attempted; they never count as corpus success. `--id FROZEN_ID` selects one of those packages.
