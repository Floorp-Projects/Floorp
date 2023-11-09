# Contributing

The following content goes more in-depth than the [Overview](./01_overview.md) section
to provide helpful information regarding contributing to Firefox Translations.

- [Source Code](#source-code)
- [Architecture](#architecture)
    - [JSActors](#jsactors)
- [Remote Settings](#remote-settings)
    - [Admin Dashboards](#admin-dashboards)
    - [Prod Admin Access](#prod-admin-access)
    - [Pulling From Different Sources](#pulling-from-different-sources)
    - [Versioning](#versioning)
      - [Non-Breaking Changes](#non-breaking-changes)
      - [Breaking Changes](#breaking-changes)
- [Building fastText](#building-fasttext)
    - [Downloading The Models](#downloading-the-models)
    - [Building the WASM Binary](#building-the-wasm-binary)
        - [Dependencies](#dependencies)
        - [Modifying the EMCXXFLAGS](#modifying-the-emcxxflags)
- [Building Bergamot](#building-bergamot)

---
## Source Code

The primary source code for Firefox Translations lives in the following directory:

> **[toolkit/components/translations]**

---
## Architecture

### JSActors

Translations functionality is divided into different classes based on which access privileges are needed.
Generally, this split is between [Parent] and [Child] versions of [JSWindowActors].

The [Parent] actors have access to privileged content and is responsible for things like downloading models
from [Remote Settings](#remote-settings), modifying privileged UI components etc.

The [Child] actors are responsible for interacting with content on the page itself, requesting content from
the [Parent] actors, and creating [Workers] to carry out tasks.

---
## Remote Settings

The machine-learning models and [WASM] binaries are all hosted in Remote Settings and are downloaded/cached when needed.

### Admin Dashboards

In order to get access to Firefox Translations content in the Remote Settings admin dashboards, you will need to request
access in the Remote Settings component on [Bugzilla].

Once you have access to Firefox Translations content in Remote Settings, you will be able to view it in the admin dashboards:

**Dev**<br>
> [https://settings.dev.mozaws.net/v0/admin](https://settings.dev.mozaws.net/v1/admin)

**Stage**<br>
> [https://remote-settings.allizom.org/v0/admin](https://settings-writer.stage.mozaws.net/v1/admin)

### Prod Admin Access

In order to access the prod admin dashboard, you must also have access to a VPN that is authorized to view the dashboard.
To gain access to the VPN, follow [Step 3] on this page in the Remote Settings documentation.

**Prod**<br>
> [https://remote-settings.mozilla.org/v1/admin](https://settings-writer.prod.mozaws.net/v1/admin)


### Pulling From Different Sources

When you are running Firefox, you can choose to pull data from **Dev**, **Stage**, or **Prod** by downloading and installing
the latest [remote-settings-devtools] Firefox extension.

### Versioning

Firefox Translations uses semantic versioning for all of its records via the **`version`** property.

#### Non-breaking Changes

Firefox Translations code will always retrieve the maximum compatible version of each record from Remote Settings.
If two records exist with different versions, (e.g. **`1.0`** and **`1.1`**) then only the version **`1.1`** record
will be considered.

This allows us to update and ship new versions of models that are compatible with the current source code and wasm runtimes
in both backward-compatible and forward-compatible ways. These can be released through remote settings independent of the
[Firefox Release Schedule].

#### Breaking Changes

Breaking changes for Firefox Translations are a bit more tricky. These are changes that make older-version records
incompatible with the current Firefox source code and/or [WASM] runtimes.

While a breaking change will result in a change of the semver number (e.g. **`1.1 ⟶ 2.0`**), this alone is not
sufficient. Since Firefox Translations always attempts to use the maximum compatible version, only bumping this number
would result in older versions of Firefox attempting to use a newer-version record that is no longer compatible with the
Firefox source code or [WASM] runtimes.

To handle these changes, Firefox Translations utilizes Remote Settings [Filter Expressions] to make certain records
available to only particular releases of Firefox. This will allow Firefox Translations to make different sets of Remote Settings records available to different versions
of Firefox.

```{admonition} Example

Let's say that Firefox 108 through Firefox 120 is compatible with translations model records in the **`1.*`** major-version range, however Firefox 121 and onward is compatible with only model records in the **`2.*`** major-version range.

This will allow us to mark the **`1.*`** major-version records with the following filter expression:

**`
"filter_expression": "env.version|versionCompare('108.a0') >= 0 && env.version|versionCompare('121.a0') < 0"
`**

This means that these records will only be available in Firefox versions greater than or equal to 108, and less than 121.

Similarly, we will be able to mark all of the **`2.*`** major-version records with this filter expression:

**`
"filter_expression": "env.version|versionCompare('121.a0') >= 0"
`**

This means that these records will only be available in Firefox versions greater than or equal to Firefox 121.

```

Tying breaking changes to releases in this way frees up Firefox Translations to make changes as large as entirely
switching one third-party library for another in the compiled source code, while allowing older versions of Firefox to continue utilizing the old library and allowing newer versions of Firefox to utilize the new library.

---
## Building fastText

### Downloading the Models

The fastText model that we use can be downloaded directly from the fastText website:<br>
> [https://fasttext.cc/docs/en/language-identification.html](https://fasttext.cc/docs/en/language-identification.html)

Firefox Translations uses the compressed, **`lid.176.ftz`** model.

### Building the WASM Binary

To build the fastText [WASM] binary, we can follow the steps in the [Requirements] section of the fastText website.

#### Dependencies

**C++ Compiler**<br>
Any of the C++ compilers from [Getting Set Up To Work On The Firefox Codebase] will be sufficient for this.

**emskd**<br>
Follow the [Download and Install] instructions for setting up the emscripten sdk.

#### Modifying the EMCXXFLAGS

At the time of writing, the a latest commit on the fastText repo ([3697152e0fd772d9185697fdbd4a1d340ca5571d])
is not compatible by default with the latest version of [emscripten (3.1.35)].

A few changes need to be made to the Makefile in order to generate the fastText [WASM] for use in Firefox.

**1) Disable DYNAMIC_EXECUTION**<br>
In the `Makefile` for the fastText repo, there is a variable called **`EMCXXFLAGS`**.<br>
We need to add the following flag to this variable:

```
-s "DYNAMIC_EXECUTION=0"
```

If this flag is not set to **`0`**, then emscripten will [generate functions] that use the [eval()] function.
[eval()] is not allowed in the context that fastText runs in FireFox due to security reasons.

**2) Rename EXTRA_EXPORTED_RUNTIME_METHODS**<br>
In [emscripten (2.0.18)], **`EXTRA_EXPORTED_RUNTIME_METHODS`** was deprecated in favor of **`EXPORTED_RUNTIME_METHODS`**.
The fastText Makefile still has the old flag, so we need to update the name.

**3) Use the -r Flag When Appropriate**<br>
In [emscripten (2.0.3)] the following change was made:

> "The default output format is now executable JavaScript. Previously we would default to output objecting files unless, for example, the output name ended in **`.js`**. This is contrary to behavior of clang and gcc. Now emscripten will always produce and executable unless the **`-c`**, **`-r`** or **`-shared`** flags are given. This is true even when the name of the output file ends in **`.o`**. e.g, **`emcc foo.c -o foo.o`** will produce a JavaScript file called **`foo.o`**. This might surprise some users (although it matches the behavior of existing toolchains) so we now produce a warning in this case."

The Makefile needs to be modified to use the **`-r`** flag when appropriate. These changes are modeled after comments on this [GitHub Issue].

**Cumulative Changes**<br>
Here is a diff of the full changes needed for the Makefile at the time of writing:

```diff
diff --git a/Makefile b/Makefile
index e246f79..396ae0b 100644
--- a/Makefile
+++ b/Makefile
@@ -73,7 +73,9 @@ clean:

 EMCXX = em++
-EMCXXFLAGS = --bind --std=c++11 -s WASM=1 -s ALLOW_MEMORY_GROWTH=1 -s "EXTRA_EXPORTED_RUNTIME_METHODS=['addOnPostRun', 'FS']" -s "DISABLE_EXCEPTION_CATCHING=0" -s "EXCEPTION_DEBUG=1" -s "FORCE_FILESYSTEM=1" -s "MODULARIZE=1" -s "EXPORT_ES6=1" -s 'EXPORT_NAME="FastTextModule"' -Isrc/
+EMCXXFLAGS_BASE = --bind --std=c++11 -s WASM=1 -s ALLOW_MEMORY_GROWTH=1 -s "EXPORTED_RUNTIME_METHODS=['addOnPostRun', 'FS']" -s "DISABLE_EXCEPTION_CATCHING=0" -s "EXCEPTION_DEBUG=0" -s "DYNAMIC_EXECUTION=0" -s "FORCE_FILESYSTEM=1" -s "MODULARIZE=1" -s "EXPORT_ES6=1" -s 'EXPORT_NAME="FastTextModule"' -Isrc/
+EMCXXFLAGS = $(EMCXXFLAGS_BASE) -r
+EMCXXFLAGS_JS = $(EMCXXFLAGS_BASE)
 EMOBJS = args.bc autotune.bc matrix.bc dictionary.bc loss.bc productquantizer.bc densematrix.bc quantmatrix.bc vector.bc model.bc utils.bc meter.bc fasttext.bc main.bc


@@ -120,6 +122,6 @@ fasttext.bc: src/fasttext.cc src/*.h
 	$(EMCXX) $(EMCXXFLAGS)  src/fasttext.cc -o fasttext.bc

 webassembly/fasttext_wasm.js: $(EMOBJS) webassembly/fasttext_wasm.cc Makefile
-	$(EMCXX) $(EMCXXFLAGS) $(EMOBJS) -o webassembly/fasttext_wasm.js
+	$(EMCXX) $(EMCXXFLAGS_JS) $(EMOBJS) -o webassembly/fasttext_wasm.js
```

After modifying the Makefile in the previous section, running **`make wasm`** in the fastText repo should run without warnings or errors and the following files will be generated in the **`webassembly`** directory:

```
webassembly
├── fasttext.js
├── fasttext_wasm.js
└── fasttext_wasm.wasm
```

#### Modifying fasttext_wasm.js

There are a few changes we need to make to the **`fasttext_wasm.js`** file to make it compatible with use in Firefox.

**1) Define a function, not a module**<br>
The generated code exports a module, but this needs to be modified into a function for use in [importScripts()] in a worker.

At the top of the file we need to make the following changes:

```diff
diff --git a/toolkit/components/translations/fasttext/fasttext_wasm.js b/toolkit/components/translations/fasttext/fasttext_wasm.js
index 64c6184a85851..4802343da2a03 100644
--- a/toolkit/components/translations/fasttext/fasttext_wasm.js
+++ b/toolkit/components/translations/fasttext/fasttext_wasm.js
@@ -1,9 +1,6 @@

-var FastTextModule = (() => {
-  var _scriptDir = import.meta.url;
-
-  return (
-async function(FastTextModule = {})  {
+async function loadFastTextModule(FastTextModule = {})  {
+  const _scriptDir = null;

 // include: shell.js
 // The Module object: Our interface to the outside world. We import
```

Here we are defining a function rather than a variable, and we are setting **`_scriptDir`** to null
because **`import.meta.url`** is only available for use within modules.

Next we need to modify the bottom of the file to match these changes:

```diff
diff --git a/toolkit/components/translations/fasttext/fasttext_wasm.js b/toolkit/components/translations/fasttext/fasttext_wasm.js
index 64c6184a85851..0a6fca3f524e4 100644
--- a/toolkit/components/translations/fasttext/fasttext_wasm.js
+++ b/toolkit/components/translations/fasttext/fasttext_wasm.js
@@ -8287,7 +8287,3 @@ run();

   return FastTextModule.ready
 }
-
-);
-})();
-export default FastTextModule;
```

**2) Remove unneeded environment checks**<br>
Next we need to remove unneeded checks for different environments:

```JavaScript
if (ENVIRONMENT_IS_NODE) {
    // ...
} else
if (ENVIRONMENT_IS_SHELL) {
    // ...
} else
if (ENVIRONMENT_IS_WEB || ENVIRONMENT_IS_WORKER) {
    // ...
} else
{
  throw new Error('environment detection error');
}
```

Since this code will only be run inside of a worker, we want to delete the blocks that deal with **`ENVIRONMENT_IS_NODE`** and **`ENVIRONMENT_IS_SHELL`**. In fact, this code will fail to be imported by [importScripts()] if we don't do this.

**3) Remove the use of `import.meta.url`**<br>
Finally, there is a use of **`import.meta.url`** that we need to remove.

```diff
diff --git a/toolkit/components/translations/fasttext/fasttext_wasm.js b/toolkit/components/translations/fasttext/fasttext_wasm.js
index 64c6184a85851..746cbae2ec952 100644
--- a/toolkit/components/translations/fasttext/fasttext_wasm.js
+++ b/toolkit/components/translations/fasttext/fasttext_wasm.js
@@ -746,7 +746,7 @@ if (Module['locateFile']) {
   }
 } else {
   // Use bundler-friendly `new URL(..., import.meta.url)` pattern; works in browsers too.
-  wasmBinaryFile = new URL('fasttext_wasm.wasm', import.meta.url).href;
+  wasmBinaryFile = null;
 }

 function getBinary(file) {
```

As mentioned before, **`import.meta.url`** is not allowed outside of modules and cannot be used with [importScripts()]
in the worker code that we are creating.

It is okay to set this to null here, because we will be providing the **`wasmBinaryFile`** via [Remote Settings].

**4) Minifying the file**<br>
The generated **`fasttext_wasm.js`** file is very large. To minimize the impact on the size of the code in the Firefox source tree, we want to minify the file using the [minify] tool.

```
Size Name
291k ├── fasttext_wasm.js (original)
109k └── fasttext_wasm.js (minified)
```

**5) Adding the license**<br>
Finally, we should add a copy of the current fastText MIT license to the top of the minified **`fasttext_wasm.js`** file.
You should be able to paste this from the generated **`fasttext.js`** file.

#### Modifying fasttext.js

```{note}
It is likely that the source file in tree already has these changes and is already sufficient,
even if **`fasttext_wasm.js`** has been recently updated. Try running it first as-is before replacing
and re-modifying.
```

Next we need to modify **`fasttext.js`** to utilize the changes that we made to **`fasttext_wasm.js`** and also to
not be a module so that we can import it using [importScripts()].

These changes do the following:

1) Define a variable called **`fastTextModule`** for use in the worker scripts.
2) Utilize the **`loadFastTextModule()`** function that we defined in **`fasttext_wasm.js`**
3) Add a function **`loadModelBinary()`** that takes the wasm binary directly, which we will provide through [Remote Settings].
4) Remove any module exports.

```diff
diff --git a/toolkit/components/translations/fasttext/fasttext.js b/toolkit/components/translations/fasttext/fasttext.js
index 86600b9ac9e28..2c49b3faaeedc 100644
--- a/toolkit/components/translations/fasttext/fasttext.js
+++ b/toolkit/components/translations/fasttext/fasttext.js
@@ -6,20 +6,30 @@
  * LICENSE file in the root directory of this source tree.
  */

-import fastTextModularized from './fasttext_wasm.js';
-const fastTextModule = fastTextModularized();
+let fastTextModule;
+
+const _initFastTextModule = async function (wasmModule) {
+  try {
+    fastTextModule = await loadFastTextModule(wasmModule);
+  } catch(e) {
+    console.error(e);
+  }
+  return true
+}

 let postRunFunc = null;
 const addOnPostRun = function(func) {
   postRunFunc = func;
 };

-fastTextModule.addOnPostRun(() => {
-  if (postRunFunc) {
-    postRunFunc();
-  }
-});

+const loadFastText = (wasmModule) => {
+  _initFastTextModule(wasmModule).then((res) => {
+    if (postRunFunc) {
+      postRunFunc();
+    }
+  })
+}
 const thisModule = this;
 const trainFileInWasmFs = 'train.txt';
 const testFileInWasmFs = 'test.txt';
@@ -41,7 +51,7 @@ const getFloat32ArrayFromHeap = (len) => {
 const heapToFloat32 = (r) => new Float32Array(r.buffer, r.ptr, r.size);

 class FastText {
-  constructor() {
+  constructor(fastTextModule) {
     this.f = new fastTextModule.FastText();
   }

@@ -77,6 +87,15 @@ class FastText {
     });
   }

+  loadModelBinary(buffer) {
+    const fastTextNative = this.f;
+    const byteArray = new Uint8Array(buffer);
+    const FS = fastTextModule.FS;
+    FS.writeFile(modelFileInWasmFs, byteArray);
+    fastTextNative.loadModel(modelFileInWasmFs);
+    return new FastTextModel(fastTextNative);
+  }
+
   _train(url, modelName, kwargs = {}, callback = null) {
     const fetchFunc = (thisModule && thisModule.fetch) || fetch;
     const fastTextNative = this.f;
@@ -515,6 +534,3 @@ class FastTextModel {
     });
   }
 }
-
-
-export {FastText, addOnPostRun};
```

---
## Building Bergamot

TODO


<!-- Hyperlinks -->
[3697152e0fd772d9185697fdbd4a1d340ca5571d]: https://github.com/facebookresearch/fastText/tree/3697152e0fd772d9185697fdbd4a1d340ca5571d
[Bugzilla]: https://bugzilla.mozilla.org/enter_bug.cgi?product=Cloud%20Services&component=Server%3A%20Remote%20Settings
[Child]: https://searchfox.org/mozilla-central/search?q=TranslationsChild
[Download and Install]: https://emscripten.org/docs/getting_started/downloads.html#download-and-install
[emscripten (2.0.3)]: https://github.com/emscripten-core/emscripten/blob/main/ChangeLog.md#203-09102020
[emscripten (2.0.18)]: https://github.com/emscripten-core/emscripten/blob/main/ChangeLog.md#2018-04232021
[emscripten (3.1.35)]: https://github.com/emscripten-core/emscripten/blob/main/ChangeLog.md#3135---040323
[Environments]: https://remote-settings.readthedocs.io/en/latest/getting-started.html#environments
[eval()]: https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/eval
[Filter Expressions]: https://remote-settings.readthedocs.io/en/latest/target-filters.html#filter-expressions
[Firefox Release Schedule]: https://wiki.mozilla.org/Release_Management/Calendar
[generate functions]: https://emscripten.org/docs/api_reference/emscripten.h.html?highlight=dynamic_execution#functions
[Getting Set Up To Work On The Firefox Codebase]: https://firefox-source-docs.mozilla.org/setup/index.html
[GitHub Issue]: https://github.com/facebookresearch/fastText/pull/1227#issuecomment-1353830003
[importScripts()]: https://developer.mozilla.org/en-US/docs/Web/API/WorkerGlobalScope/importScripts
[JSWindowActors]: https://firefox-source-docs.mozilla.org/dom/ipc/jsactors.html#jswindowactor
[minify]: https://github.com/tdewolff/minify
[Parent]: https://searchfox.org/mozilla-central/search?q=TranslationsParent
[Step 3]: https://remote-settings.readthedocs.io/en/latest/getting-started.html#create-a-new-official-type-of-remote-settings
[remote-settings-devtools]: https://github.com/mozilla-extensions/remote-settings-devtools/releases
[Remote Settings]: https://remote-settings.readthedocs.io/en/latest/
[Requirements]: https://fasttext.cc/docs/en/webassembly-module.html#requirements
[toolkit/components/translations]: https://searchfox.org/mozilla-central/search?q=toolkit%2Fcomponents%2Ftranslations
[WASM]: https://webassembly.org/
[Workers]: https://searchfox.org/mozilla-central/search?q=%2Ftranslations.*worker&path=&case=false&regexp=true
