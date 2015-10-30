/**
 * @fileoverview Import globals from head.js and from any files that were
 * imported by head.js (as far as we can correctly resolve the path).
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
"use strict";

//------------------------------------------------------------------------------
// Rule Definition
//------------------------------------------------------------------------------

var fs = require("fs");
var path = require("path");
var helpers = require("../helpers");

module.exports = function(context) {
  //--------------------------------------------------------------------------
  // Helpers
  //--------------------------------------------------------------------------

  function checkFile(fileArray, context) {
    var filePath = fileArray.pop();

    while (filePath) {
      var headText;

      try {
        headText = fs.readFileSync(filePath, "utf8");
      } catch(e) {
        // Couldn't find file, continue.
        filePath = fileArray.pop();
        continue;
      }

      var ast = helpers.getAST(headText);
      var globalVars = helpers.getGlobals(ast);

      for (var i = 0; i < globalVars.length; i++) {
        var name = globalVars[i];
        helpers.addVarToScope(name, context);
      }

      for (var index in ast.body) {
        var node = ast.body[index];
        var source = helpers.getTextForNode(node, headText);
        var name = helpers.getVarNameFromImportSource(source);

        if (name) {
          helpers.addVarToScope(name, context);
          continue;
        }

        // Scripts loaded using loadSubScript or loadHelperScript
        var matches =
          source.match(/^(?:Services\.scriptloader\.|loader)?loadSubScript\((.+)[",]/);
        if (!matches) {
          matches = source.match(/^loadHelperScript\((.+)[",]/);
        }
        if (matches) {
          var cwd = process.cwd();

          filePath = matches[1];
          filePath = filePath.replace("chrome://mochitests/content/browser", cwd + "/../../../..");
          filePath = filePath.replace(/testdir\s*\+\s*["']/gi, cwd + "/");
          filePath = filePath.replace(/test_dir\s*\+\s*["']/gi, cwd);
          filePath = filePath.replace(/["']/gi, "");
          filePath = path.normalize(filePath);

          fileArray.push(filePath);
        }
      }

      filePath = fileArray.pop();
    }
  }

  //--------------------------------------------------------------------------
  // Public
  //--------------------------------------------------------------------------

  return {
    Program: function(node) {
      if (!helpers.getIsBrowserMochitest(this)) {
        return;
      }

      var pathAndFilename = this.getFilename();
      var processPath = process.cwd();
      var testFilename = path.basename(pathAndFilename);
      var testPath = path.join(processPath, testFilename);
      var headjs = path.join(processPath, "head.js");
      checkFile([testPath, headjs], context);
    }
  };
};
