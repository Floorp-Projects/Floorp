/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const { setTimeout } = ChromeUtils.importESModule(
  "resource://gre/modules/Timer.sys.mjs",
);

export class NRWebContentModifierChild extends JSWindowActorChild {
  handleEvent(event) {
    if (event.type === "DOMContentLoaded") {
      const url = this.document.location.href;
      console.log(`NRWebContentModifier: DOMContentLoaded fired for ${url}`);

      if (url.includes("floorp.app")) {
        console.log(
          "NRWebContentModifier: floorp.app detected, attempting to modify heading",
        );
        setTimeout(() => this.modifyFloorpHeading(), 500);
      }
    }
  }

  receiveMessage(message) {
    console.log(`NRWebContentModifier: Received message: ${message.name}`);
    switch (message.name) {
      case "WebContentModifier:ModifyFloorpHeading":
        return this.modifyFloorpHeading();
    }
    return null;
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
        const elements = this.document.querySelectorAll(selector);
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
          url: this.document.location.href,
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
    } catch (e) {
      console.error("NRWebContentModifier: Error modifying Floorp heading:", e);
      return {
        success: false,
        error: e.toString(),
      };
    }
  }
}
