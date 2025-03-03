/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

export class NRWebContentModifierParent extends JSWindowActorParent {
  receiveMessage(message) {
    console.log(
      `NRWebContentModifierParent: Received message: ${message.name}`,
    );

    switch (message.name) {
      case "WebContentModifier:ModificationApplied":
        console.log(
          "NRWebContentModifierParent: Content modification applied:",
          message.data,
        );

        try {
          const { url, status } = message.data;
          if (status === "success") {
            Services.wm.getMostRecentWindow("navigator:browser")
              .PopupNotifications.show(
                this.browsingContext.topFrameElement,
                "floorp-content-modified",
                "Floorp.app content has been modified",
                "tab",
                null,
                null,
                {
                  persistent: false,
                  timeout: 3000,
                },
              );
          }
        } catch (e) {
          console.error(
            "NRWebContentModifierParent: Error showing notification:",
            e,
          );
        }
        break;
    }
    return null;
  }

  async checkAndModifyFloorpWebsite(browser) {
    try {
      if (
        !browser.browsingContext || !browser.browsingContext.currentWindowGlobal
      ) {
        console.log(
          "NRWebContentModifierParent: Browser context not available",
        );
        return null;
      }

      const actor = browser.browsingContext.currentWindowGlobal.getActor(
        "NRWebContentModifier",
      );
      const currentUrl = browser.currentURI.spec;

      console.log("NRWebContentModifierParent: Current URL:", currentUrl);

      if (currentUrl.includes("floorp.app")) {
        console.log(
          "NRWebContentModifierParent: Floorp website is opened:",
          currentUrl,
        );
        return actor.sendQuery("WebContentModifier:ModifyFloorpHeading");
      }
    } catch (e) {
      console.error(
        "NRWebContentModifierParent: Error in checkAndModifyFloorpWebsite:",
        e,
      );
    }
    return null;
  }
}
