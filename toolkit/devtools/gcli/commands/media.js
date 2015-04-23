/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const l10n = require("gcli/l10n");

exports.items = [
  {
    name: "media",
    description: l10n.lookup("mediaDesc")
  },
  {
    item: "command",
    runAt: "client",
    name: "media emulate",
    description: l10n.lookup("mediaEmulateDesc"),
    manual: l10n.lookup("mediaEmulateManual"),
    params: [
      {
        name: "type",
        description: l10n.lookup("mediaEmulateType"),
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
    item: "command",
    runAt: "client",
    name: "media reset",
    description: l10n.lookup("mediaResetDesc"),
    exec: function(args, context) {
      let markupDocumentViewer = context.environment.chromeWindow
                                        .gBrowser.markupDocumentViewer;
      markupDocumentViewer.stopEmulatingMedium();
    }
  }
];
