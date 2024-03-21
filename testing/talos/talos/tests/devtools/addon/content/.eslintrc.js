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
    dampWindow: true,
  },
  rules: {
    "no-unused-vars": ["error", { argsIgnorePattern: "^_", vars: "all" }],
    // These are the rules that have been configured so far to match the
    // devtools coding style.

    // Rules from the mozilla plugin
    "mozilla/no-aArgs": "error",
    "mozilla/no-define-cc-etc": "off",
    // See bug 1224289.
    "mozilla/reject-importGlobalProperties": ["error", "everything"],
    "mozilla/var-only-at-top-level": "error",
  },
};
