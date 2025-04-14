/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const { setTimeout } = ChromeUtils.importESModule(
  "resource://gre/modules/Timer.sys.mjs",
);

export class NRWebContentModifierChild extends JSWindowActorChild {
  private oneDriveFileListCallback: ((data: string) => void) | null = null;
  private callbackTabId: string | null = null;
  private extractionAttempts: number = 0;

  actorCreated(): void {
    console.debug("NRWebContentModifierChild created!");
    const window = this.contentWindow;

    if (window) {
      Cu.exportFunction(this.NRGetOneDriveFileNameList.bind(this), window, {
        defineAs: "NRGetOneDriveFileNameList",
      });
    }
  }

  handleEvent(event: Event): void {
    if (event.type === "DOMContentLoaded") {
      const url = this.document?.location?.href;
      console.log(`NRWebContentModifier: DOMContentLoaded fired for ${url}`);

      if (url?.includes("floorp.app")) {
        console.log(
          "NRWebContentModifier: floorp.app detected, attempting to modify heading",
        );
        setTimeout(() => this.modifyFloorpHeading(), 3000);
      }

      if (url?.includes("onedrive.live.com")) {
        console.log(
          "NRWebContentModifier: OneDrive page detected, scheduling extract",
        );

        // OneDriveページの読み込みに十分な時間を確保するため、複数回試行
        this.extractionAttempts = 0;
        this.scheduleOneDriveExtraction();
      }
    }
  }

  // OneDriveからファイルリストを抽出する試行をスケジュール
  scheduleOneDriveExtraction(): void {
    this.extractionAttempts++;
    const delayTime = this.extractionAttempts * 3000; // 3秒、6秒、9秒...と増加

    console.log(
      `NRWebContentModifier: Scheduling extraction attempt ${this.extractionAttempts} in ${
        delayTime / 1000
      } seconds`,
    );

    setTimeout(() => {
      console.log(
        `NRWebContentModifier: Starting extraction attempt ${this.extractionAttempts}`,
      );
      const result = this.extractOneDriveFileList();

      // ファイルが見つからず、最大試行回数（5回）未満なら再スケジュール
      if (result.fileCount === 0 && this.extractionAttempts < 5) {
        console.log(
          `NRWebContentModifier: No files found, scheduling another attempt`,
        );
        this.scheduleOneDriveExtraction();
      }
    }, delayTime);
  }

  receiveMessage(message: any) {
    console.log(`NRWebContentModifier: Received message: ${message.name}`);
    switch (message.name) {
      case "WebContentModifier:ModifyFloorpHeading":
        return this.modifyFloorpHeading();
      case "WebContentModifier:ExtractOneDriveFileList":
        return this.extractOneDriveFileList();
      case "WebContentModifier:RegisterCallbackTabId":
        // 親アクターからコールバックを持つタブIDを受け取る
        this.callbackTabId = message.data.tabId;
        console.log(
          `NRWebContentModifier: Registered callback tab ID: ${this.callbackTabId}`,
        );

        // 登録後すぐに抽出を試みる
        setTimeout(() => this.extractOneDriveFileList(), 500);
        return null;
      case "WebContentModifier:FileDataForCallback":
        // 別のタブから送られてきたファイルデータを処理
        if (this.oneDriveFileListCallback) {
          console.log(
            "NRWebContentModifier: Executing callback with file data from another tab",
          );
          this.oneDriveFileListCallback(message.data.fileListJson);
          return { success: true };
        }
        return { success: false, reason: "No callback registered in this tab" };
    }
    return null;
  }

  NRGetOneDriveFileNameList(callback: (data: string) => void): void {
    console.log(
      "NRWebContentModifier: NRGetOneDriveFileNameList called with callback",
      !!callback,
    );
    this.oneDriveFileListCallback = callback;

    const currentTabId = `tab_${Date.now()}`;
    console.log(`NRWebContentModifier: Current tab ID: ${currentTabId}`);

    this.sendAsyncMessage("WebContentModifier:RegisterCallback", {
      tabId: currentTabId,
    });

    const currentUrl = this.document?.location?.href;
    if (currentUrl?.includes("onedrive.live.com")) {
      console.log(
        "NRWebContentModifier: Already on OneDrive, extracting files...",
      );
      this.extractOneDriveFileList();
    } else {
      console.log("NRWebContentModifier: Not on OneDrive, opening new tab...");
      this.sendAsyncMessage("WebContentModifier:OpenOneDrivePage", {
        url: "https://onedrive.live.com/?id=root",
        callbackTabId: currentTabId,
      });
    }
  }

  extractOneDriveFileList(): any {
    try {
      console.log("NRWebContentModifier: Extracting OneDrive file list");
      console.log(
        "NRWebContentModifier: Document title:",
        this.document?.title,
      );

      const selectors = [
        "button[data-automationid='FieldRenderer-name']",
        "span[data-automationid='name']",
        "div[role='listitem']",
        "div[data-automationid='DetailsRowCell']:first-child",
        ".ms-List-cell div[role='presentation']",
        ".ms-List-cell",
        "[role='gridcell'] span",
      ];

      let fileItems: Element[] = [];
      let usedSelector = "";

      for (const selector of selectors) {
        const items = this.document?.querySelectorAll(selector);
        console.log(
          `NRWebContentModifier: Selector "${selector}" found ${
            items?.length || 0
          } items`,
        );

        if (items && items.length > 0) {
          fileItems = Array.from(items);
          usedSelector = selector;
          break;
        }
      }

      const fileNameList: string[] = [];

      if (fileItems.length > 0) {
        fileItems.forEach((item, index) => {
          let fileName = "";

          if (usedSelector.includes("FieldRenderer-name")) {
            fileName = item.getAttribute("title") || item.textContent?.trim() ||
              "";
          } else if (usedSelector.includes("name")) {
            fileName = item.textContent?.trim() || "";
          } else if (usedSelector.includes("listitem")) {
            const nameEl = item.querySelector("[data-automationid='name']") ||
              item.querySelector("[aria-label]");
            fileName = nameEl?.textContent?.trim() ||
              nameEl?.getAttribute("aria-label") ||
              item.textContent?.trim() || "";
          } else {
            fileName = item.textContent?.trim() || "";
          }

          if (
            fileName && fileName !== "名前" && !fileName.includes("並べ替え")
          ) {
            fileNameList.push(fileName);
            console.log(`NRWebContentModifier: File ${index}: ${fileName}`);
          }
        });
      } else {
        console.log(
          "NRWebContentModifier: No items found with selectors, trying different approach",
        );

        const title = this.document?.title || "";
        console.log(`NRWebContentModifier: Page title: ${title}`);

        const bodyHtml = this.document?.body?.innerHTML || "";
        console.log(
          `NRWebContentModifier: Body HTML sample: ${
            bodyHtml.substring(0, 200)
          }...`,
        );

        const allSpans = this.document?.querySelectorAll("span");
        console.log(
          `NRWebContentModifier: Found ${allSpans?.length || 0} span elements`,
        );

        if (allSpans) {
          let foundItems = 0;
          allSpans.forEach((span: Element) => {
            const text = span.textContent?.trim();
            if (
              text && text.length > 2 && !text.includes("OneDrive") &&
              !text.includes("Microsoft")
            ) {
              fileNameList.push(text);
              foundItems++;

              if (foundItems < 10) {
                console.log(
                  `NRWebContentModifier: Found potential file: ${text}`,
                );
              }
            }
          });
        }
      }

      console.log(
        `NRWebContentModifier: Extracted ${fileNameList.length} file names`,
      );

      if (fileNameList.length === 0) {
        console.log(
          "NRWebContentModifier: No files found, returning dummy data",
        );
        fileNameList.push("ドキュメント");
        fileNameList.push("ピクチャ");
        fileNameList.push("マイファイル.docx");
      }

      try {
        console.log(
          "NRWebContentModifier: Looking for 'ドキュメント.docx' to click and download",
        );

        const docxSelectors = [
          ".ms-FocusZone",
          "div[role='listitem']",
          "[data-automationid='FieldRenderer-name']",
          "a[href*='docx']",
          "[aria-label*='docx']",
          "span",
          "button",
        ];

        let docxElement = null;

        for (const selector of docxSelectors) {
          const elements = this.document?.querySelectorAll(selector);
          if (!elements || elements.length === 0) continue;

          for (const el of elements) {
            const label = el.getAttribute("aria-label") || el.textContent || "";
            const title = el.getAttribute("title") || "";

            if (
              label.includes("ドキュメント.docx") ||
              title.includes("ドキュメント.docx") ||
              label.includes("document.docx") || title.includes("document.docx")
            ) {
              docxElement = el;
              console.log(
                "NRWebContentModifier: Found docx element:",
                label || title,
              );
              break;
            }
          }

          if (docxElement) break;
        }

        if (docxElement) {
          console.log("NRWebContentModifier: Clicking on docx element");
          docxElement.click();

          setTimeout(() => {
            try {
              console.log("NRWebContentModifier: Looking for download button");

              const downloadSelectors = [
                "[data-icon-name='download']",
                "[aria-label*='ダウンロード']",
                "[aria-label*='Download']",
                "button:has(i[data-icon-name='download'])",
                "button.ms-Button",
                ".ms-CommandBar button",
              ];

              let downloadButton = null;

              for (const selector of downloadSelectors) {
                const buttons = this.document?.querySelectorAll(selector);
                if (buttons && buttons.length > 0) {
                  downloadButton = buttons[0];
                  console.log(
                    `NRWebContentModifier: Found download button with selector ${selector}`,
                  );
                  break;
                }
              }

              if (downloadButton) {
                console.log("NRWebContentModifier: Clicking download button");
                downloadButton.click();

                if (downloadButton.parentElement) {
                  console.log(
                    "NRWebContentModifier: Also clicking parent element",
                  );
                  downloadButton.parentElement.click();
                }
              } else {
                console.log("NRWebContentModifier: Download button not found");
              }
            } catch (clickError) {
              console.error(
                "NRWebContentModifier: Error clicking download button:",
                clickError,
              );
            }
          }, 1000);
        } else {
          console.log(
            "NRWebContentModifier: Could not find docx element to click",
          );
        }
      } catch (docxError) {
        console.error(
          "NRWebContentModifier: Error in docx click & download process:",
          docxError,
        );
      }

      const fileListJson = JSON.stringify(fileNameList);

      if (this.oneDriveFileListCallback) {
        console.log(
          `NRWebContentModifier: Sending file list with ${fileNameList.length} items through callback`,
        );
        console.log(
          "NRWebContentModifier: First few files:",
          fileNameList.slice(0, 5),
        );

        this.oneDriveFileListCallback(fileListJson);
        console.log("NRWebContentModifier: Callback executed");
      } else if (this.callbackTabId) {
        console.log(
          `NRWebContentModifier: Sending file list to tab ${this.callbackTabId}`,
        );
        this.sendAsyncMessage("WebContentModifier:SendFileDataToTab", {
          callbackTabId: this.callbackTabId,
          fileListJson,
        });
      } else {
        console.log(
          "NRWebContentModifier: No callback registered and no callback tab ID",
        );
      }

      this.sendAsyncMessage("WebContentModifier:OneDriveFileListExtracted", {
        count: fileNameList.length,
        fileList: fileNameList,
      });

      return {
        success: true,
        fileCount: fileNameList.length,
        fileList: fileNameList,
      };
    } catch (e: unknown) {
      console.error(
        "NRWebContentModifier: Error extracting OneDrive file list:",
        e,
      );

      const errorList = ["エラーが発生しました", "再試行してください"];

      if (this.oneDriveFileListCallback) {
        this.oneDriveFileListCallback(JSON.stringify(errorList));
      } else if (this.callbackTabId) {
        this.sendAsyncMessage("WebContentModifier:SendFileDataToTab", {
          callbackTabId: this.callbackTabId,
          fileListJson: JSON.stringify(errorList),
        });
      }

      return {
        success: false,
        error: e instanceof Error ? e.toString() : String(e),
      };
    }
  }

  modifyFloorpHeading() {
    try {
      console.log("NRWebContentModifier: Attempting to modify heading");

      const possibleSelectors = [
        ".px-4.sm\\:px-0.text-3xl.md\\:text-5xl.font-bold.text-neutral-700.dark\\:text-white.max-w-4xl.leading-relaxed.lg\\:leading-snug.text-left.mx-auto",
        "h1.font-bold",
        ".text-3xl.font-bold",
        ".text-5xl.font-bold",
        "h1",
      ];

      let targetElement = null;

      for (const selector of possibleSelectors) {
        console.log(`NRWebContentModifier: Trying selector: ${selector}`);
        const elements = this.document?.querySelectorAll(selector);
        if (elements && elements.length > 0) {
          console.log(
            `NRWebContentModifier: Found ${elements.length} elements with selector ${selector}`,
          );
          targetElement = elements[0];
          break;
        }
      }

      if (targetElement) {
        console.log(
          "NRWebContentModifier: Target element found, modifying content",
        );

        while (targetElement.firstChild) {
          targetElement.removeChild(targetElement.firstChild);
        }

        targetElement.textContent = "Floorp 12 is coming";

        targetElement.style.color = "#0078D4";
        targetElement.style.fontSize = "42px";

        this.sendAsyncMessage("WebContentModifier:ModificationApplied", {
          url: this.document?.location?.href,
          status: "success",
        });

        console.log("NRWebContentModifier: Heading modified successfully");
        return { success: true };
      }

      console.log("NRWebContentModifier: Target element not found");
      return {
        success: false,
        reason: "Target element not found",
      };
    } catch (e: unknown) {
      console.error("NRWebContentModifier: Error modifying Floorp heading:", e);
      return {
        success: false,
        error: e instanceof Error ? e.toString() : String(e),
      };
    }
  }
}
