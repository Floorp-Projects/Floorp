/// <reference path="../../libs/@types/gecko/index.d.ts" />
// SPDX-License-Identifier: MPL-2.0
// Test-only actor. It drives the vendor's documented macro bridge, not a macro engine.
export class CorpusAutomationChild extends JSWindowActorChild {
  /** @param {{name:string,data?:import("./automation-test-types.ts").AutomationActorData}} message */
  async receiveMessage({ name, data = {} }) {
    const document = this.document;
    const nativeWindow = this.contentWindow;
    if (!document || !nativeWindow) {
      throw Error("Private automation actor lost its document");
    }
    const window =
      /** @type {import("./automation-test-types.ts").AutomationContentWindow} */ (Cu
        .waiveXrays(nativeWindow));
    if (name === "Snapshot") {
      return {
        url: document.documentURI,
        ready: document.readyState,
        text: document.body?.innerText?.slice(0, 20000) || "",
        input: (/** @type {HTMLInputElement|null} */ (document.querySelector(
          "#customer",
        )))?.value,
        result: document.querySelector("#operation-result")?.textContent,
        operation: (/** @type {HTMLElement|null} */ (document.documentElement))
          ?.dataset.operation,
        clickCount: (/** @type {HTMLElement|null} */ (document.documentElement))
          ?.dataset.clickCount,
        rows: [...document.querySelectorAll("table tr")].map((row) =>
          [...row.querySelectorAll("th,td")].map((cell) =>
            (/** @type {HTMLElement} */ (cell)).innerText
          )
        ),
      };
    }
    if (name === "WebScraperControl" || name === "WebScraperControlState") {
      if (
        !/^moz-extension:\/\/.+\/devtools_web_scraper_panel\.html(?:#|$)/.test(
          document.documentURI,
        )
      ) {
        throw Error("Expected actual Web Scraper DevTools panel");
      }
      if (!data.control) throw Error("Web Scraper control is required");
      const controls = {
        createMenu: "#create-sitemap-nav-button",
        importForm: "a.import-sitemap",
        sitemapJSON: "#sitemapJSON",
        import: "#submit-import-sitemap",
        sitemapMenu: "#sitemap-nav-button",
        scrapeForm: "a.scrape",
        start: "#submit-scrape-sitemap",
      };
      const element =
        /** @type {HTMLElement|null|undefined} */ (data.control === "refresh"
          ? [...document.querySelectorAll("button")].find((button) =>
            (button.textContent || "").includes("Refresh")
          )
          : document.querySelector(
            controls[data.control] || "floorp-no-such-control",
          ));
      if (name === "WebScraperControlState") {
        element?.scrollIntoView({ block: "center", inline: "center" });
        const rect = element?.getBoundingClientRect();
        const visible = !!element && !!rect?.width && !!rect.height &&
          nativeWindow.getComputedStyle(element)?.getPropertyValue(
              "visibility",
            ) === "visible";
        const enabled = !!element && !element.hasAttribute("disabled") &&
          element.getAttribute("aria-disabled") !== "true" &&
          !element.classList.contains("disabled");
        const hit = visible && rect
          ? document.elementFromPoint(
            rect.x + rect.width / 2,
            rect.y + rect.height / 2,
          )
          : null;
        const table = document.querySelector(
          "#selector-tree table.selector-table",
        );
        const tableRect = table?.getBoundingClientRect();
        return {
          url: document.documentURI,
          ready: document.readyState,
          control: data.control,
          present: !!element,
          visible,
          enabled,
          unobstructed: !!element && (hit === element || element.contains(hit)),
          activeSitemapId:
            document.querySelector("#navbar-active-sitemap-id")?.textContent
              ?.trim() || "",
          selectorViewReady: !!tableRect?.width && !!tableRect.height,
        };
      }
      if (!element) {
        throw Error("Actual Web Scraper control missing: " + data.control);
      }
      element.scrollIntoView({ block: "center" });
      const rect = element.getBoundingClientRect();
      if (!rect.width || !rect.height) {
        throw Error("Actual Web Scraper control hidden");
      }
      if (data.control === "sitemapJSON") {
        element.focus();
        const input = Cc["@mozilla.org/text-input-processor;1"].createInstance(
          Ci.nsITextInputProcessor,
        );
        if (
          !input.beginInputTransactionForTests(
            /** @type {mozIDOMWindow} */ (/** @type {unknown} */ (nativeWindow)),
          )
        ) {
          throw Error("Native text input unavailable");
        }
        input.commitCompositionWith(JSON.stringify(data.sitemap));
        return { value: (/** @type {HTMLTextAreaElement} */ (element)).value };
      }
      const x = rect.x + rect.width / 2,
        y = rect.y + rect.height / 2;
      nativeWindow.synthesizeMouseEvent("mousedown", x, y, {
        button: 0,
        buttons: 1,
        clickCount: 1,
      });
      nativeWindow.synthesizeMouseEvent("mouseup", x, y, {
        button: 0,
        buttons: 0,
        clickCount: 1,
      });
      return { clickedActualControl: data.control };
    }
    if (name === "AcknowledgeUIVisionWelcome") {
      if (
        !/^moz-extension:\/\/[^/]+\/sidepanel\.html(?:[?#]|$)/.test(
          document.documentURI,
        ) ||
        document.nodePrincipal.addonId !==
          "gcbalfbdmfieckjlnblleoemohcganoc@chromium-extension"
      ) {
        throw Error("Expected the original UI.Vision side-panel document");
      }
      const dialog = document.querySelector(".macro-setup-dialog");
      const title =
        dialog?.querySelector(".ant-modal-title")?.textContent?.trim() || null;
      if (!dialog || !dialog.getBoundingClientRect().width) {
        return { shown: false, clicked: false, title, events: [] };
      }
      if (title !== "Your macros are all still here") {
        throw Error("Unknown UI.Vision onboarding dialog");
      }
      const button =
        /** @type {HTMLButtonElement|null} */ (dialog.querySelector(
          ".ant-modal-footer button.ant-btn-primary",
        ));
      if (!button || button.disabled || button.textContent?.trim() !== "OK") {
        throw Error("Actual UI.Vision welcome acknowledgement is unavailable");
      }
      button.scrollIntoView({ block: "center", inline: "center" });
      const rect = button.getBoundingClientRect();
      const x = rect.x + rect.width / 2, y = rect.y + rect.height / 2;
      const hit = document.elementFromPoint(x, y);
      if (
        !rect.width || !rect.height || (hit !== button && !button.contains(hit))
      ) {
        throw Error(
          "Actual UI.Vision welcome acknowledgement is hidden or occluded",
        );
      }
      /** @type {import("./automation-test-types.ts").UIVisionWelcomeObservation["events"]} */
      const events = [];
      /** @param {Event} event */
      const observe = (event) =>
        events.push({ type: event.type, trusted: event.isTrusted });
      for (const type of ["mousedown", "mouseup", "click"]) {
        button.addEventListener(type, observe);
      }
      try {
        nativeWindow.synthesizeMouseEvent("mousemove", x, y, {});
        nativeWindow.synthesizeMouseEvent("mousedown", x, y, {
          button: 0,
          buttons: 1,
          clickCount: 1,
        });
        nativeWindow.synthesizeMouseEvent("mouseup", x, y, {
          button: 0,
          buttons: 0,
          clickCount: 1,
        });
        await new Promise((resolve) => nativeWindow.setTimeout(resolve, 50));
        if (!events.some((event) => event.type === "click" && event.trusted)) {
          throw Error(
            "UI.Vision welcome control did not receive a trusted native click",
          );
        }
        return { shown: true, clicked: true, title, events };
      } finally {
        for (const type of ["mousedown", "mouseup", "click"]) {
          button.removeEventListener(type, observe);
        }
      }
    }
    if (name === "ConfigureUIVision") {
      if (!/^moz-extension:\/\/.+\/options\.html$/.test(document.documentURI)) {
        throw Error("Expected real extension options document");
      }
      if (!data.url) throw Error("Authored website URL is required");
      const { config = {} } = await window.chrome.storage.local.get("config");
      await window.chrome.storage.local.set(
        Cu.cloneInto(
          {
            config: {
              ...config,
              allowRunFromHttpSchema: true,
              websiteInvokePolicyVersion: 1,
              websiteWhiteList: [data.url],
              showClassicMacros: true,
              showTestCaseTab: true,
              openAiChatTabOnce: false,
              macroFreshInstall: false,
            },
          },
          window,
        ),
      );
      return { websiteInvokePolicyVersion: 1, websiteWhiteList: [data.url] };
    }
    if (name === "RunUIVision") {
      if (!data.url || !document.documentURI.startsWith(data.url)) {
        throw Error("Expected authored automation target");
      }
      window.dispatchEvent(
        new window.CustomEvent("kantuSaveAndRunMacro", {
          detail: Cu.cloneInto(
            { json: data.macro, direct: true, storageMode: "browser" },
            window,
          ),
        }),
      );
      return { dispatchedDocumentedBridge: true };
    }
    if (
      name === "ReadAutomaConfiguration" ||
      name === "WriteAutomaWorkflow" ||
      name === "ReadAutomaEngineState" ||
      name === "RunAutoma"
    ) {
      if (!/^moz-extension:\/\/.+\/popup\.html$/.test(document.documentURI)) {
        throw Error("Expected real Automa popup document");
      }
      if (name === "ReadAutomaConfiguration") {
        return window.chrome.storage.local.get(
          Cu.cloneInto(["workflows", "isFirstTime"], window),
        );
      }
      if (name === "ReadAutomaEngineState") {
        return window.chrome.storage.local.get(
          Cu.cloneInto(["logs", "workflowState"], window),
        );
      }
      if (name === "WriteAutomaWorkflow") {
        if (!data.workflow) throw Error("Authored workflow is required");
        const { workflows = {} } = await window.chrome.storage.local.get(
          "workflows",
        );
        await window.chrome.storage.local.set(
          Cu.cloneInto(
            {
              workflows: { ...workflows, [data.workflow.id]: data.workflow },
              isFirstTime: false,
            },
            window,
          ),
        );
        return { storedAuthoredWorkflow: true };
      }
      const card = [...document.querySelectorAll(".ui-card")].find(
        (card) => card.querySelector("p")?.textContent === data.name,
      );
      const button = card?.querySelector('button[title="Execute"]');
      if (!button) throw Error("Actual Automa Execute control missing");
      button.scrollIntoView({ block: "center" });
      const rect = button.getBoundingClientRect(),
        x = rect.x + rect.width / 2,
        y = rect.y + rect.height / 2;
      nativeWindow.synthesizeMouseEvent("mousedown", x, y, {
        button: 0,
        buttons: 1,
        clickCount: 1,
      });
      nativeWindow.synthesizeMouseEvent("mouseup", x, y, {
        button: 0,
        buttons: 0,
        clickCount: 1,
      });
      return { clickedActualExecuteControl: true };
    }
    throw Error("Unknown automation corpus operation");
  }
}
