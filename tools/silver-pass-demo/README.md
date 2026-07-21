# Silver Guide demo

This directory contains the static municipal application fixture used by the
Silver Guide browser-chrome demo. The feature intentionally accepts only the
exact loopback URL below (URL fragments are allowed).

Start the fixture server from the repository root:

```bash
python3 -m http.server 4173 \
  --bind 127.0.0.1 \
  --directory tools/silver-pass-demo
```

In another terminal, start the Floorp development runtime:

```bash
deno task feles-build dev
```

Open the fixture in that Floorp instance:

```bash
deno task dev-tool navigate \
  http://127.0.0.1:4173/municipal-application.html \
  --context content
```

Select the Silver Guide icon in the URL bar to open the guidance panel. The
fixture is fictional and self-contained: it loads no external assets, sends no
form data, and stores nothing. Stop both development commands with `Ctrl+C` when
finished.
