# vendored dependencies

- onnxruntime-web 1.14.0 (UMD modules)
- Transformers.js 2.16.1 (patched for gecko)

To build the vendored dependencies, run `./build.sh`.

The Transformers.js patch is required for building Transformers.js
without including the `onnxruntime-web` NPM bundle. It uses
webpack's `externals` feature to include the onnxruntime-web UMD
module and changes the import statement in `backends/onnx.js` to
point to it.
