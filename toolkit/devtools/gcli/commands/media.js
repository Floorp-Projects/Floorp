/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const gcli = require("gcli/index");

exports.items = [
  {
    name: "media",
    description: gcli.lookup("mediaDesc")
  },
  {
    name: "media emulate",
    description: gcli.lookup("mediaEmulateDesc"),
    manual: gcli.lookup("mediaEmulateManual"),
    params: [
      {
        name: "type",
        description: gcli.lookup("mediaEmulateType"),
        type: {
           name: "selection",
           data: [
             "braille", "embossed", "handheld", "print", "projection",
             "screen", "speech", "tty", "tv"
           ]
        }
      }
    ],
    exec: function(args, context) {
      let markupDocumentViewer = context.environment.chromeWindow
                                        .gBrowser.markupDocumentViewer;
      markupDocumentViewer.emulateMedium(args.type);
    }
  },
  {
    name: "media reset",
    description: gcli.lookup("mediaResetDesc"),
    exec: function(args, context) {
      let markupDocumentViewer = context.environment.chromeWindow
                                        .gBrowser.markupDocumentViewer;
      markupDocumentViewer.stopEmulatingMedium();
    }
  }
];
