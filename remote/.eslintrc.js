/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

module.exports = {
  rules: {
    "no-undef-init": "error",
    // This is usually only applied to jsm or jsm.js files.
    // But marionette modules are JS files without a jsm extension and use
    // the EXPORT_SYMBOLS syntax.
    "mozilla/mark-exported-symbols-as-used": "error",
  },
  overrides: [
    {
      // The mozilla recommended configuration is not strict enough against
      // regular javascript files, compared to JSM. Enforce no-unused-vars on
      // "all" variables, not only "local" ones.
      files: ["*"],
      // test head files and html files typically define global variables
      // intended to be used by other files and should not be affected by
      // this override.
      excludedFiles: ["**/test/**/head.js", "**/test/**/*.html"],
      rules: {
        "no-unused-vars": [
          "error",
          {
            args: "none",
            vars: "all",
          },
        ],
      },
    },
  ],
};
