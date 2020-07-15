"use strict";

module.exports = {
  plugins: ["react"],
  globals: {
    exports: true,
    isWorker: true,
    loader: true,
    module: true,
    reportError: true,
    require: true,
  },
  rules: {
    "no-unused-vars": ["error", { args: "none", vars: "all" }],
    // These are the rules that have been configured so far to match the
    // devtools coding style.

    // Rules from the mozilla plugin
    "mozilla/no-aArgs": "error",
    // See bug 1224289.
    "mozilla/reject-importGlobalProperties": ["error", "everything"],
    "mozilla/var-only-at-top-level": "error",
    "mozilla/use-chromeutils-import": ["error", { allowCu: true }],
  },
};
