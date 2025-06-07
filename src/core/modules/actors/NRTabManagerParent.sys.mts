/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

export class NRTabManagerParent extends JSWindowActorParent {
  async receiveMessage(message: ReceiveMessageArgument) {
    switch (message.name) {
      case "Tabs:AddTab": {
        const { url, options } = message.data;
        const win = Services.wm.getMostRecentWindow(
          "navigator:browser",
        ) as Window;

        win.gBrowser.selectedTab = win.gBrowser.addTab(url, {
          relatedToCurrent: true,
          triggeringPrincipal: Services.scriptSecurityManager
            .getSystemPrincipal(),
          ...options,
        });
        this.sendAsyncMessage("Tabs:AddTab");
      }
    }
  }
}
