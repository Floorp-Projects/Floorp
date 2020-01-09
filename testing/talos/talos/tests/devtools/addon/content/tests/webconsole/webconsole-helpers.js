/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { reloadPageAndLog } = require("../head");

exports.reloadConsoleAndLog = async function(label, toolbox, expectedMessages) {
  let onReload = async function() {
    let webconsole = toolbox.getPanel("webconsole");
    await new Promise(done => {
      let messages = 0;
      let receiveMessages = () => {
        if (++messages == expectedMessages) {
          webconsole.hud.ui.off("new-messages", receiveMessages);
          done();
        }
      };
      webconsole.hud.ui.on("new-messages", receiveMessages);
    });
  };
  await reloadPageAndLog(label + ".webconsole", toolbox, onReload);
};
