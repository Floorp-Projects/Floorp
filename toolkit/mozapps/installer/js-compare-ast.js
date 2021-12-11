/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * This script compares the AST of two JavaScript files passed as arguments.
 * The script exits with a 0 status code if both files parse properly and the
 * ASTs of both files are identical modulo location differences. The script
 * exits with status code 1 if any of these conditions don't hold.
 *
 * This script is used as part of packaging to verify minified JavaScript files
 * are identical to their original files.
 */

// Available to the js shell.
/* global snarf, scriptArgs, quit */

"use strict";

function ast(filename) {
  return JSON.stringify(Reflect.parse(snarf(filename), { loc: 0 }));
}

if (scriptArgs.length !== 2) {
  throw new Error("usage: js js-compare-ast.js FILE1.js FILE2.js");
}

var ast0 = ast(scriptArgs[0]);
var ast1 = ast(scriptArgs[1]);

quit(ast0 == ast1 ? 0 : 1);
