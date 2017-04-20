/**
 * @fileoverview Remove macros from SpiderMonkey's self-hosted JS.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

"use strict";

var path = require("path");

const selfHostedRegex = /js\/src\/(?:builtin|shell)\/.*?\.js$/;
const macroRegex = /\s*\#(if|ifdef|else|elif|endif|include|define|undef).*/;

module.exports = {
  preprocess(text, filename) {
    if (path.win32) {
      filename = filename.split(path.sep).join("/");
    }
    if (!selfHostedRegex.test(filename)) {
      return [text];
    }

    let lines = text.split(/\n/);
    for (let i = 0; i < lines.length; i++) {
      if (!macroRegex.test(lines[i])) {
        // No macro here, nothing to do.
        continue;
      }

      for (; i < lines.length; i++) {
        lines[i] = "// " + lines[i];

        // If the line ends with a backslash (\), the next line
        // is also part of part of the macro.
        if (!lines[i].endsWith("\\")) {
          break;
        }
      }
    }

    return [lines.join("\n")];
  },

  postprocess(messages, filename) {
    return Array.prototype.concat.apply([], messages);
  }
};
