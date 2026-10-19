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
    if (
      name === "ReadUIVisionWebsiteConfiguration" ||
      name === "UIVisionWebsiteControlState" ||
      name === "UIVisionWebsiteControl"
    ) {
      if (
        !/^moz-extension:\/\/[^/]+\/options\.html#api$/.test(
          document.documentURI,
        ) ||
        document.nodePrincipal.addonId !==
          "gcbalfbdmfieckjlnblleoemohcganoc@chromium-extension"
      ) {
        throw Error("Expected original UI.Vision API settings document");
      }
      if (name === "ReadUIVisionWebsiteConfiguration") {
        const { config = {} } = await window.chrome.storage.local.get("config");
        return {
          allowRunFromHttpSchema: config.allowRunFromHttpSchema,
          websiteInvokePolicyVersion: config.websiteInvokePolicyVersion,
          websiteWhiteList: config.websiteWhiteList,
        };
      }
      const label = [...document.querySelectorAll("label")].find((element) =>
        element.textContent?.includes(
          "Run embedded macros from public websites",
        )
      );
      const dialog = document.querySelector(".whitelist-modal");
      const dialogTitle = dialog?.querySelector(".ant-modal-title")?.textContent
        ?.trim() || null;
      const control = data.uiVisionControl;
      const element = /** @type {HTMLElement|null|undefined} */ (
        control === "websiteRuns"
          ? label?.querySelector('input[type="checkbox"]')
          : control === "editWhitelist"
          ? [...(label?.querySelectorAll("a") || [])].find((link) =>
            link.textContent?.trim() === "Edit Whitelist"
          )
          : control === "whitelistURL"
          ? dialog?.querySelector(
            'textarea[placeholder="One URL prefix per line, e. g. https://ui.vision/rpa or ui.vision (covers http and https)"]',
          )
          : control === "saveWhitelist"
          ? [...(dialog?.querySelectorAll("button.ant-btn-primary") || [])]
            .find(
              (button) => button.textContent?.trim() === "Save",
            )
          : null
      );
      element?.scrollIntoView({ block: "center", inline: "center" });
      const rect = element?.getBoundingClientRect();
      const visible = !!element && !!rect?.width && !!rect.height &&
        nativeWindow.getComputedStyle(element)?.getPropertyValue(
            "visibility",
          ) ===
          "visible";
      const enabled = !!element && !element.hasAttribute("disabled") &&
        element.getAttribute("aria-disabled") !== "true";
      const x = rect ? rect.x + rect.width / 2 : 0;
      const y = rect ? rect.y + rect.height / 2 : 0;
      const hit = visible ? document.elementFromPoint(x, y) : null;
      const unobstructed = !!element &&
        (hit === element || element.contains(hit));
      const state = {
        url: document.documentURI,
        ready: document.readyState,
        control,
        present: !!element,
        visible,
        enabled,
        unobstructed,
        checked: control === "websiteRuns"
          ? /** @type {HTMLInputElement|null|undefined} */ (element)?.checked
          : undefined,
        value: control === "whitelistURL"
          ? /** @type {HTMLTextAreaElement|null|undefined} */ (element)?.value
          : undefined,
        dialogTitle,
        dialogVisible: !!dialog?.getBoundingClientRect().width,
      };
      if (name === "UIVisionWebsiteControlState") return state;
      if (
        !element || !visible || !enabled || !unobstructed ||
        ((control === "whitelistURL" || control === "saveWhitelist") &&
          dialogTitle !== "Embedded Macros Website Whitelist")
      ) {
        throw Error(
          "Genuine UI.Vision website control is unavailable: " + control,
        );
      }
      /** @type {{type:string,trusted:boolean}[]} */
      const events = [];
      /** @param {Event} event */
      const observe = (event) =>
        events.push({ type: event.type, trusted: event.isTrusted });
      const types = control === "whitelistURL"
        ? ["beforeinput", "input"]
        : ["mousedown", "mouseup", "click"];
      for (const type of types) element.addEventListener(type, observe);
      try {
        if (control === "whitelistURL") {
          if (!data.url) {
            throw Error("Authored loopback website URL is required");
          }
          const url = new URL(data.url);
          if (
            url.protocol !== "http:" || url.hostname !== "127.0.0.1" ||
            !url.port || url.pathname !== "/automation" || url.search ||
            url.hash || url.username || url.password
          ) {
            throw Error(
              "Website whitelist must contain only the authored loopback operation",
            );
          }
          const textarea = /** @type {HTMLTextAreaElement} */ (element);
          textarea.focus();
          textarea.select();
          const input = Cc["@mozilla.org/text-input-processor;1"]
            .createInstance(
              Ci.nsITextInputProcessor,
            );
          if (
            !input.beginInputTransactionForTests(
              /** @type {mozIDOMWindow} */ (/** @type {unknown} */ (nativeWindow)),
            )
          ) {
            throw Error("Native UI.Vision whitelist text input unavailable");
          }
          input.commitCompositionWith(data.url);
        } else {
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
        }
        await new Promise((resolve) => nativeWindow.setTimeout(resolve, 50));
        const expectedEvent = control === "whitelistURL" ? "input" : "click";
        if (
          !events.some((event) => event.type === expectedEvent && event.trusted)
        ) {
          throw Error(
            "UI.Vision setting did not receive trusted native " + expectedEvent,
          );
        }
        return { ...state, events };
      } finally {
        for (const type of types) element.removeEventListener(type, observe);
      }
    }
    if (name === "UIVisionLogControlState" || name === "UIVisionLogControl") {
      if (
        !/^moz-extension:\/\/[^/]+\/sidepanel\.html(?:[?#]|$)/.test(
          document.documentURI,
        ) ||
        document.nodePrincipal.addonId !==
          "gcbalfbdmfieckjlnblleoemohcganoc@chromium-extension"
      ) {
        throw Error(
          "Expected original UI.Vision panel for genuine log navigation",
        );
      }
      const control = data.uiVisionLogControl;
      const selector = control === "data"
        ? '.sidebar-inner > .ant-tabs > .ant-tabs-nav .ant-tabs-tab[data-node-key="Logs"] [role="tab"]'
        : control === "logs"
        ? '.data-tab > .ant-tabs > .ant-tabs-nav .ant-tabs-tab[data-node-key="Logs"] [role="tab"]'
        : null;
      if (!selector) throw Error("Unknown genuine UI.Vision log control");
      const element =
        /** @type {HTMLElement|null} */ (document.querySelector(selector));
      if (
        element &&
        element.textContent?.trim() !== (control === "data" ? "Data" : "Logs")
      ) {
        throw Error("Unknown UI.Vision log control label");
      }
      element?.scrollIntoView({ block: "center", inline: "center" });
      const rect = element?.getBoundingClientRect();
      const x = rect ? rect.x + rect.width / 2 : 0;
      const y = rect ? rect.y + rect.height / 2 : 0;
      const visible = !!element && !!rect?.width && !!rect.height &&
        nativeWindow.getComputedStyle(element)?.getPropertyValue(
            "visibility",
          ) === "visible";
      const enabled = !!element &&
        element.getAttribute("aria-disabled") !== "true" &&
        !element.closest(".ant-tabs-tab-disabled");
      const hit = visible ? document.elementFromPoint(x, y) : null;
      const view = document.querySelector(
        control === "data"
          ? ".data-tab"
          : '.data-tab [role="tabpanel"][id$="-panel-Logs"]',
      );
      const viewRect = view?.getBoundingClientRect();
      const state = {
        url: document.documentURI,
        ready: document.readyState,
        control,
        present: !!element,
        visible,
        enabled,
        unobstructed: !!element && (hit === element || element.contains(hit)),
        selected: element?.getAttribute("aria-selected") === "true",
        viewVisible: !!viewRect?.width && !!viewRect.height,
        rect: { x, y, width: rect?.width || 0, height: rect?.height || 0 },
      };
      if (name === "UIVisionLogControlState") return state;
      if (!element || !visible || !enabled || !state.unobstructed) {
        throw Error("Actual UI.Vision log control is unavailable: " + control);
      }
      /** @type {{type:string,trusted:boolean}[]} */
      const events = [];
      /** @param {Event} event */
      const observe = (event) =>
        events.push({ type: event.type, trusted: event.isTrusted });
      const types = ["mousedown", "mouseup", "click"];
      for (const type of types) element.addEventListener(type, observe);
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
            "Actual UI.Vision log tab did not receive trusted native click",
          );
        }
        return { ...state, events };
      } finally {
        for (const type of types) element.removeEventListener(type, observe);
      }
    }
    if (name === "RunUIVision") {
      if (!data.url || document.documentURI !== data.url) {
        throw Error("Expected exact authored automation target");
      }
      if (!data.witnessToken || !/^[a-f0-9-]{36}$/.test(data.witnessToken)) {
        throw Error("Owned UI.Vision public witness token is required");
      }
      const observerToken = data.witnessToken;
      /** @type {import("./automation-test-types.ts").UIVisionInvocationObservation["events"]} */
      const events = [];
      /** @param {Event} event */
      const observe = (event) => {
        if (events.length >= 20) return;
        const observed = {
          type: event.type,
          message: event.type === "error"
            ? String(/** @type {ErrorEvent} */ (event).message || "").slice(
              0,
              1000,
            )
            : event.type === "unhandledrejection"
            ? String(/** @type {PromiseRejectionEvent} */ (event).reason).slice(
              0,
              1000,
            )
            : undefined,
        };
        events.push(observed);
        // Browser.log retains this diagnostic even when the real macro's open
        // command navigates and destroys this document/actor. It is not a pass.
        console.log(
          "[Actual UI.Vision public witness] " + JSON.stringify({
            observerToken,
            url: document.documentURI,
            event: observed,
          }),
        );
      };
      const types = ["kantuInvokeSuccess", "error", "unhandledrejection"];
      const finish = () => {
        for (const type of types) {
          nativeWindow.removeEventListener(type, observe);
        }
      };
      for (const type of types) nativeWindow.addEventListener(type, observe);
      const timer = nativeWindow.setTimeout(finish, 3000);
      try {
        window.dispatchEvent(
          new window.CustomEvent(
            "kantuSaveAndRunMacro",
            Cu.cloneInto({
              detail: {
                json: data.macro,
                direct: true,
                storageMode: "browser",
              },
            }, window),
          ),
        );
        // A query response must not await a Promise owned by the document that
        // this unchanged macro deliberately navigates. Later events use the log.
        return {
          dispatchedDocumentedBridge: true,
          witnessMs: 3000,
          events: [...events],
          observerToken,
          transport: "bounded-owned-document-console",
        };
      } catch (error) {
        nativeWindow.clearTimeout(timer);
        finish();
        throw error;
      }
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
