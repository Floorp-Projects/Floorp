/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

//------------------------------------------------------------------------------
// Requirements
//------------------------------------------------------------------------------

var rule = require("../lib/rules/use-ownerGlobal");

//------------------------------------------------------------------------------
// Tests
//------------------------------------------------------------------------------

function invalidCode(code) {
  let message = "use .ownerGlobal of .ownerDocument.defaultView";
  return {code: code, errors: [{message: message, type: "MemberExpression"}]};
}

exports.runTest = function(ruleTester) {
  ruleTester.run("use-ownerGlobal", rule, {
    valid: [
      "aEvent.target.ownerGlobal;",
      "this.DOMPointNode.ownerGlobal.getSelection();",
      "windowToMessageManager(node.ownerGlobal);",
    ],
    invalid: [
      invalidCode("aEvent.target.ownerDocument.defaultView;"),
      invalidCode("this.DOMPointNode.ownerDocument.defaultView.getSelection();"),
      invalidCode("windowToMessageManager(node.ownerDocument.defaultView);"),
    ]
  });
};
