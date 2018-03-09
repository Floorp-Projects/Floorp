"use strict";

module.exports = {
  "plugins": [
    "react"
  ],
  "globals": {
    "exports": true,
    "isWorker": true,
    "loader": true,
    "module": true,
    "reportError": true,
    "require": true,
  },
  "rules": {
    "no-unused-vars": ["error", {"args": "none", "vars": "all"}],
    // These are the rules that have been configured so far to match the
    // devtools coding style.

    // Rules from the mozilla plugin
    "mozilla/no-aArgs": "error",
    "mozilla/no-cpows-in-tests": "error",
    "mozilla/no-single-arg-cu-import": "error",
    // See bug 1224289.
    "mozilla/reject-importGlobalProperties": "error",
    // devtools/shared/platform is special; see the README.md in that
    // directory for details.  We reject requires using explicit
    // subdirectories of this directory.
    "mozilla/reject-some-requires": ["error", "^devtools/shared/platform/(chome|content)/"],
    "mozilla/var-only-at-top-level": "error",
    "mozilla/use-chromeutils-import": ["error", {allowCu: true}]
  }
};
