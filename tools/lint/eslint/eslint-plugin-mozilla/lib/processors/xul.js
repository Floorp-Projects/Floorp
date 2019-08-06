/**
 * @fileoverview Converts inline attributes from XUL into JS
 * functions
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

"use strict";

let path = require("path");
let fs = require("fs");

let XMLParser = require("./processor-helpers").XMLParser;

// Stores any XML parse error
let xmlParseError = null;

// Stores the lines of JS code generated from the XBL
let scriptLines = [];
// Stores a map from the synthetic line number to the real line number
// and column offset.
let lineMap = [];

let includedRanges = [];

// Deal with ifdefs. This is the state we pretend to have:
const kIfdefStateForLinting = {
  MOZ_UPDATER: true,
  XP_WIN: true,
  MOZ_BUILD_APP_IS_BROWSER: true,
  MOZ_SERVICES_SYNC: true,
  MOZ_DATA_REPORTING: true,
  MOZ_TELEMETRY_REPORTING: true,
  MOZ_CRASHREPORTER: true,
  MOZ_NORMANDY: true,
  MOZ_MAINTENANCE_SERVICE: true,
  HAVE_SHELL_SERVICE: true,
  MENUBAR_CAN_AUTOHIDE: true,
  MOZILLA_OFFICIAL: true,
};

// Anything not in the above list is assumed false.
function dealWithIfdefs(text, filename) {
  function stripIfdefsFromLines(input, innerFile) {
    let outputLines = [];
    let inSkippingIfdef = [false];
    for (let i = 0; i < input.length; i++) {
      let line = input[i];
      let shouldSkip = inSkippingIfdef.some(x => x);
      if (!line.startsWith("#")) {
        outputLines.push(shouldSkip ? "" : line);
      } else {
        if (
          line.startsWith("# ") ||
          line.startsWith("#filter") ||
          line == "#" ||
          line.startsWith("#define")
        ) {
          outputLines.push("");
          continue;
        }
        // if this isn't just a comment (which we skip), figure out what to do:
        let term = "";
        let negate = false;
        if (line.startsWith("#ifdef")) {
          term = line.match(/^#ifdef *([A-Z_]+)/);
        } else if (line.startsWith("#ifndef")) {
          term = line.match(/^#ifndef *([A-Z_]+)/);
          negate = true;
        } else if (line.startsWith("#if ")) {
          term = line.match(/^defined\(([A-Z_]+)\)/);
        } else if (line.startsWith("#elifdef")) {
          // Replace the old one:
          inSkippingIfdef.pop();
          term = line.match(/^#elifdef *([A-Z_]+)/);
        } else if (line.startsWith("#else")) {
          // Switch the last one around:
          let old = inSkippingIfdef.pop();
          inSkippingIfdef.push(!old);
          outputLines.push("");
        } else if (line.startsWith("#endif")) {
          inSkippingIfdef.pop();
          outputLines.push("");
        } else if (line.startsWith("#expand")) {
          // Just strip expansion instructions
          outputLines.push(line.substring("#expand ".length));
        } else if (line.startsWith("#include")) {
          // Uh oh.
          if (!shouldSkip) {
            let fileToInclude = line.substr("#include ".length).trim();
            let subpath = path.join(path.dirname(innerFile), fileToInclude);
            let contents = fs.readFileSync(subpath, { encoding: "utf-8" });
            contents = contents.split(/\n/);
            // Recurse:
            contents = stripIfdefsFromLines(contents, subpath);
            if (innerFile == filename) {
              includedRanges.push({
                start: i,
                end: i + contents.length,
                filename: subpath,
              });
            }
            // And insert the resulting lines:
            input = input.slice(0, i).concat(contents, input.slice(i + 1));
            // Re-process this line now that we've spliced things in.
            i--;
          } else {
            outputLines.push("");
          }
        } else {
          throw new Error("Unknown preprocessor directive: " + line);
        }

        if (term) {
          // We always want the first capturing subgroup:
          term = term && term[1];
          if (!negate) {
            inSkippingIfdef.push(!kIfdefStateForLinting[term]);
          } else {
            inSkippingIfdef.push(kIfdefStateForLinting[term]);
          }
          outputLines.push("");
          // Now just continue; we'll include lines depending on the state of `inSkippingIfdef`.
        }
      }
    }
    return outputLines;
  }
  let lines = text.split(/\n/);
  return stripIfdefsFromLines(lines, filename).join("\n");
}

function addSyntheticLine(line, linePos, addDisableLine) {
  lineMap[scriptLines.length] = { line: linePos };
  scriptLines.push(line + (addDisableLine ? "" : " // eslint-disable-line"));
}

function recursiveExpand(node) {
  for (let [attr, value] of Object.entries(node.attributes)) {
    if (attr.startsWith("on")) {
      if (attr == "oncommand" && value == ";") {
        // Ignore these, see bug 371900 for why people might do this.
        continue;
      }
      // Ignore dashes in the tag name
      let nodeDesc = node.local.replace(/-/g, "");
      if (node.attributes.id) {
        nodeDesc += "_" + node.attributes.id.replace(/[^a-z]/gi, "_");
      }
      if (node.attributes.class) {
        nodeDesc += "_" + node.attributes.class.replace(/[^a-z]/gi, "_");
      }
      addSyntheticLine("function " + nodeDesc + "(event) {", node.textLine);
      let processedLines = value.split(/\r?\n/);
      let addlLine = 0;
      for (let line of processedLines) {
        line = line.replace(/^\s*/, "");
        lineMap[scriptLines.length] = {
          // Unfortunately, we only get a line number for the <tag> finishing,
          // not for individual attributes.
          line: node.textLine + addlLine,
        };
        scriptLines.push(line);
        addlLine++;
      }
      addSyntheticLine("}", node.textLine + processedLines.length - 1);
    }
  }
  for (let kid of node.children) {
    recursiveExpand(kid);
  }
}

module.exports = {
  preprocess(text, filename) {
    if (filename.includes(".inc")) {
      return [];
    }
    xmlParseError = null;
    // The following rules are annoying in XUL.
    // Indent because in multiline attributes it's impossible to understand for the XML parser.
    // Semicolons because those shouldn't be required for inline event handlers.
    // Quotes because we use doublequotes for attributes so using single quotes
    // for strings inside them makes sense.
    // No-undef because it's a bunch of work to teach this code how to read
    // scripts and get globals from them (though ideally we should do that at some point).
    scriptLines = [
      "/* eslint-disable indent */",
      "/* eslint-disable indent-legacy */",
      "/* eslint-disable semi */",
      "/* eslint-disable quotes */",
      "/* eslint-disable no-undef */",
    ];
    lineMap = scriptLines.map(() => ({ line: 0 }));
    includedRanges = [];
    // Do C-style preprocessing first:
    text = dealWithIfdefs(text, filename);

    let xp = new XMLParser(text);
    if (xp.lastError) {
      xmlParseError = xp.lastError;
    }
    let doc = xp.document;
    if (!doc) {
      return [];
    }
    let node = doc;
    for (let kid of node.children) {
      recursiveExpand(kid);
    }

    let scriptText = scriptLines.join("\n") + "\n";
    return [scriptText];
  },

  postprocess(messages, filename) {
    // If there was an XML parse error then just return that
    if (xmlParseError) {
      return [xmlParseError];
    }

    // For every message from every script block update the line to point to the
    // correct place.
    let errors = [];
    for (let i = 0; i < messages.length; i++) {
      for (let message of messages[i]) {
        // ESLint indexes lines starting at 1 but our arrays start at 0
        let mapped = lineMap[message.line - 1];
        // Ensure we don't modify this by making a copy. We might need it for another failure.
        let target = mapped.line;
        let includedRange = includedRanges.find(
          r => target >= r.start && target <= r.end
        );
        // If this came from an #included file, indicate this in the message
        if (includedRange) {
          target = includedRange.start;
          message.message +=
            " (from included file " +
            path.basename(includedRange.filename) +
            ")";
        }
        // Compensate for line numbers shifting as a result of #include:
        let includeBallooning = includedRanges
          .filter(r => target >= r.end)
          .map(r => r.end - r.start)
          .reduce((acc, next) => acc + next, 0);
        target -= includeBallooning;
        // Add back the 1 to go back to 1-indexing.
        message.line = target + 1;

        // We never have column information, unfortunately.
        message.column = NaN;

        errors.push(message);
      }
    }

    return errors;
  },
};
