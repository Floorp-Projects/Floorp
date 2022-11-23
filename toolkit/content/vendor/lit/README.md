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

Using `mach`s `npm` you can update the bundle with:

```
cd toolkit/content/vendor/lit
../../../../mach npm run vendor
```

Then commit the changes.
