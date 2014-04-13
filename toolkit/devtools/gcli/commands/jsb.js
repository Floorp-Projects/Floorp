/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Cc, Ci, Cu } = require("chrome");
const gcli = require("gcli/index");
const XMLHttpRequest = Cc["@mozilla.org/xmlextras/xmlhttprequest;1"];

loader.lazyImporter(this, "js_beautify", "resource:///modules/devtools/Jsbeautify.jsm");

exports.items = [
  {
    name: "jsb",
    description: gcli.lookup("jsbDesc"),
    returnValue:"string",
    params: [
      {
        name: "url",
        type: "string",
        description: gcli.lookup("jsbUrlDesc")
      },
      {
        group: gcli.lookup("jsbOptionsDesc"),
        params: [
          {
            name: "indentSize",
            type: "number",
            description: gcli.lookup("jsbIndentSizeDesc"),
            manual: gcli.lookup("jsbIndentSizeManual"),
            defaultValue: 2
          },
          {
            name: "indentChar",
            type: {
              name: "selection",
              lookup: [
                { name: "space", value: " " },
                { name: "tab", value: "\t" }
              ]
            },
            description: gcli.lookup("jsbIndentCharDesc"),
            manual: gcli.lookup("jsbIndentCharManual"),
            defaultValue: " ",
          },
          {
            name: "doNotPreserveNewlines",
            type: "boolean",
            description: gcli.lookup("jsbDoNotPreserveNewlinesDesc")
          },
          {
            name: "preserveMaxNewlines",
            type: "number",
            description: gcli.lookup("jsbPreserveMaxNewlinesDesc"),
            manual: gcli.lookup("jsbPreserveMaxNewlinesManual"),
            defaultValue: -1
          },
          {
            name: "jslintHappy",
            type: "boolean",
            description: gcli.lookup("jsbJslintHappyDesc"),
            manual: gcli.lookup("jsbJslintHappyManual")
          },
          {
            name: "braceStyle",
            type: {
              name: "selection",
              data: ["collapse", "expand", "end-expand", "expand-strict"]
            },
            description: gcli.lookup("jsbBraceStyleDesc2"),
            manual: gcli.lookup("jsbBraceStyleManual2"),
            defaultValue: "collapse"
          },
          {
            name: "noSpaceBeforeConditional",
            type: "boolean",
            description: gcli.lookup("jsbNoSpaceBeforeConditionalDesc")
          },
          {
            name: "unescapeStrings",
            type: "boolean",
            description: gcli.lookup("jsbUnescapeStringsDesc"),
            manual: gcli.lookup("jsbUnescapeStringsManual")
          }
        ]
      }
    ],
    exec: function(args, context) {
      let opts = {
        indent_size: args.indentSize,
        indent_char: args.indentChar,
        preserve_newlines: !args.doNotPreserveNewlines,
        max_preserve_newlines: args.preserveMaxNewlines == -1 ?
                              undefined : args.preserveMaxNewlines,
        jslint_happy: args.jslintHappy,
        brace_style: args.braceStyle,
        space_before_conditional: !args.noSpaceBeforeConditional,
        unescape_strings: args.unescapeStrings
      };

      let xhr = new XMLHttpRequest();

      try {
        xhr.open("GET", args.url, true);
      } catch(e) {
        return gcli.lookup("jsbInvalidURL");
      }

      let deferred = context.defer();

      xhr.onreadystatechange = function(aEvt) {
        if (xhr.readyState == 4) {
          if (xhr.status == 200 || xhr.status == 0) {
            let browserDoc = context.environment.chromeDocument;
            let browserWindow = browserDoc.defaultView;
            let gBrowser = browserWindow.gBrowser;
            let result = js_beautify(xhr.responseText, opts);

            browserWindow.Scratchpad.ScratchpadManager.openScratchpad({text: result});

            deferred.resolve();
          } else {
            deferred.resolve("Unable to load page to beautify: " + args.url + " " +
                             xhr.status + " " + xhr.statusText);
          }
        };
      }
      xhr.send(null);
      return deferred.promise;
    }
  }
];
