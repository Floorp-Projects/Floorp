/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

/**
 * Build: <objdir>/dist/@types/lib.gecko.nsresult.d.ts,
 *
 * from:  <srcdir>/js/xpconnect/src/xpc.msg and
 *        <srcdir>/tools/ts/error_list.json,
 *        generated manually for now.
 */

const fs = require("fs");
const peggy = require("peggy");

const HEADER = `/**
 * NOTE: Do not modify this file by hand.
 * Content was generated from xpc.msg and error_list.json.
 */
`;

const XPC_MSG_GRAMMAR = `
  File = Header @( Comment / Definition / NL )*
  Header = '/*' (!'*/' .)* '*/' NL
  Comment = '/*' @$(!'*/' !NL .)* '*/' NL
  Definition = 'XPC_MSG_DEF(' @$[A-Z0-9_]+ [ ]* ', "' @$[^"]+ '")' NL
  NL = @$[ ]* '\\n'
`;

function main(lib_dts, xpc_msg, errors_json) {
  let parser = peggy.generate(XPC_MSG_GRAMMAR);

  let messages = parser.parse(fs.readFileSync(xpc_msg, "utf8"));
  let errors = JSON.parse(fs.readFileSync(errors_json, "utf8"));

  let lines = messages.map(line => {
    if (!Array.isArray(line)) {
      return line ? `  // ${line.trim()}` : "";
    }
    let [name, msg] = line;
    return `\n  /** ${msg} */\n  ${name}: 0x${errors[name].toString(16)};`;
  });

  let dts = [
    HEADER,
    "interface nsIXPCComponents_Results {",
    lines.join("\n").replaceAll("\n\n\n", "\n\n"),
    "}\n",
  ].join("\n");

  console.log(`[INFO] ${lib_dts} (${dts.length.toLocaleString()} bytes)`);
  fs.writeFileSync(lib_dts, dts);
}

main(...process.argv.slice(2));
