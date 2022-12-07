# Vendoring for lit

[lit](https://lit.dev/) can be used to help create Web Components.

## The lit.all.mjs bundle

The lit package is imported in a vendoring step and the contents are extracted
into the lit.all.mjs file. This has some differences from using lit in a regular
npm project. Imports that would normally be into a specific file are pulled
directly from the lit.all.mjs file.

eg.

```
// Standard npm package:
import { LitElement } from "lit";
import { classMap } from "lit/directives/class-map.js";

// Using lit.all.mjs (pathing to lit.all.mjs may differ)
import { classMap, LitElement } from "../vendor/lit.all.mjs";

## To update the lit bundle

Vendoring runs off of the latest tag in the https://github.com/lit/lit repo. If
the latest tag is a lit@ tag then running the vendor command will update to that
version. If the latest tag isn't for lit@, you may need to bundle manually. See
the moz.yaml file for instructions.

### Using mach vendor

```
./mach vendor toolkit/content/vendor/lit/moz.yaml
hg ci -m "Update to lit@<version>"
```

### Manually updating the bundle

To manually update, you'll need to checkout a copy of lit/lit, find the tag you
want and manually run our import commands.

  1. Clone https://github.com/lit/lit outside of moz-central
  2. Copy *.patch from this directory into the lit repo
  3. git apply *.patch
  4. npm install && npm run build
  5. Copy packages/lit/lit-all.min.js to toolkit/content/widgets/vendor/lit.all.mjs
  6. hg ci -m "Update to lit@<version>"
