/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

export class NRWebContentModifierParent extends JSWindowActorParent {
  private static tabCallbacks: Map<string, any> = new Map();

  receiveMessage(message: any) {
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
            const win = Services.wm.getMostRecentWindow(
              "navigator:browser",
            ) as any;
            if (
              win?.PopupNotifications && this.browsingContext?.topFrameElement
            ) {
              win.PopupNotifications.show(
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
          }
        } catch (e) {
          console.error(
            "NRWebContentModifierParent: Error showing notification:",
            e,
          );
        }
        break;

      case "WebContentModifier:RegisterCallback":
        try {
          const { tabId } = message.data;
          console.log(
            `NRWebContentModifierParent: Registering callback for tab ${tabId}`,
          );

          NRWebContentModifierParent.tabCallbacks.set(tabId, {
            browsingContext: this.browsingContext,
            registrationTime: Date.now(),
          });

          console.log(
            `NRWebContentModifierParent: Total registered tabs: ${NRWebContentModifierParent.tabCallbacks.size}`,
          );
        } catch (e) {
          console.error(
            "NRWebContentModifierParent: Error registering callback:",
            e,
          );
        }
        break;

      case "WebContentModifier:OpenOneDrivePage":
        console.log(
          "NRWebContentModifierParent: Opening OneDrive page:",
          message.data,
        );
        this.openOneDrivePage(message.data.url, message.data.callbackTabId);
        break;

      case "WebContentModifier:SendFileDataToTab":
        try {
          const { callbackTabId, fileListJson } = message.data;
          console.log(
            `NRWebContentModifierParent: Sending file data to tab ${callbackTabId}`,
          );

          this.sendFileDataToTab(callbackTabId, fileListJson);
        } catch (e) {
          console.error(
            "NRWebContentModifierParent: Error sending file data to tab:",
            e,
          );
        }
        break;

      case "WebContentModifier:OneDriveFileListExtracted":
        console.log(
          "NRWebContentModifierParent: OneDrive file list extracted:",
          message.data,
        );
        try {
          const { count, fileList } = message.data;

          if (fileList && this.browsingContext && this.browsingContext.top) {
            for (
              const [tabId, tabInfo] of NRWebContentModifierParent.tabCallbacks
                .entries()
            ) {
              const isRecent = (Date.now() - tabInfo.registrationTime) < 60000;
              if (isRecent) {
                console.log(
                  `NRWebContentModifierParent: Trying to send data to registered tab ${tabId}`,
                );
                this.sendFileDataToTab(tabId, JSON.stringify(fileList));
              }
            }
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

  sendFileDataToTab(tabId: string, fileListJson: string): void {
    try {
      const tabInfo = NRWebContentModifierParent.tabCallbacks.get(tabId);
      if (!tabInfo || !tabInfo.browsingContext) {
        console.error(
          `NRWebContentModifierParent: Tab ${tabId} not found or invalid`,
        );
        return;
      }

      console.log(
        `NRWebContentModifierParent: Found tab ${tabId}, sending data`,
      );

      try {
        if (!tabInfo.browsingContext.currentWindowGlobal) {
          console.error(
            `NRWebContentModifierParent: Tab ${tabId} has no currentWindowGlobal`,
          );
          return;
        }

        const actor = tabInfo.browsingContext.currentWindowGlobal.getActor(
          "NRWebContentModifier",
        );

        actor.sendAsyncMessage("WebContentModifier:FileDataForCallback", {
          fileListJson,
        });

        console.log(
          `NRWebContentModifierParent: Sent file data to tab ${tabId}`,
        );
      } catch (sendError) {
        console.error(
          `NRWebContentModifierParent: Error sending to tab ${tabId}:`,
          sendError,
        );
      }
    } catch (e) {
      console.error(
        "NRWebContentModifierParent: Error in sendFileDataToTab:",
        e,
      );
    }
  }

  async openOneDrivePage(url: string, callbackTabId?: string) {
    try {
      console.log(
        `NRWebContentModifierParent: Opening OneDrive page: ${url} (callback: ${
          callbackTabId || "none"
        })`,
      );

      const window = Services.wm.getMostRecentWindow(
        "navigator:browser",
      ) as any;
      if (!window) {
        console.error("NRWebContentModifierParent: No browser window found");
        return;
      }

      const registerCallbackToNewTab = (newTab: any, tabId: string) => {
        setTimeout(() => {
          try {
            if (newTab.linkedBrowser && newTab.linkedBrowser.browsingContext) {
              const browserContext = newTab.linkedBrowser.browsingContext;
              if (browserContext.currentWindowGlobal) {
                const actor = browserContext.currentWindowGlobal.getActor(
                  "NRWebContentModifier",
                );
                actor.sendAsyncMessage(
                  "WebContentModifier:RegisterCallbackTabId",
                  {
                    tabId: tabId,
                  },
                );
                console.log(
                  `NRWebContentModifierParent: First attempt to register callback tab ID ${tabId} (2秒後)`,
                );
              }
            }
          } catch (error) {
            console.log(
              `NRWebContentModifierParent: First registration attempt failed: ${error}`,
            );
          }

          setTimeout(() => {
            try {
              if (
                newTab.linkedBrowser && newTab.linkedBrowser.browsingContext
              ) {
                const browserContext = newTab.linkedBrowser.browsingContext;
                if (browserContext.currentWindowGlobal) {
                  const actor = browserContext.currentWindowGlobal.getActor(
                    "NRWebContentModifier",
                  );
                  actor.sendAsyncMessage(
                    "WebContentModifier:RegisterCallbackTabId",
                    {
                      tabId: tabId,
                    },
                  );
                  console.log(
                    `NRWebContentModifierParent: Second attempt to register callback tab ID ${tabId} (5秒後)`,
                  );
                }
              }
            } catch (error) {
              console.log(
                `NRWebContentModifierParent: Second registration attempt failed: ${error}`,
              );
            }
          }, 3000);
        }, 2000);
      };

      try {
        const browser = window.gBrowser;
        const newtab = browser.addTab(url, {
          inBackground: true,
          triggeringPrincipal: Services.scriptSecurityManager
            .getSystemPrincipal(),
        });

        console.log("NRWebContentModifierParent: Opened OneDrive in new tab");

        if (callbackTabId) {
          registerCallbackToNewTab(newtab, callbackTabId);
        }

        return;
      } catch (error) {
        console.error(
          "NRWebContentModifierParent: Error opening tab:",
          error,
        );
      }
    } catch (e) {
      console.error(
        "NRWebContentModifierParent: Error opening OneDrive page:",
        e,
      );
    }
  }

  async checkAndModifyFloorpWebsite(browser: any) {
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

      if (currentUrl.includes("onedrive.live.com")) {
        console.log(
          "NRWebContentModifierParent: OneDrive website is opened:",
          currentUrl,
        );
        return actor.sendQuery("WebContentModifier:ExtractOneDriveFileList");
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
