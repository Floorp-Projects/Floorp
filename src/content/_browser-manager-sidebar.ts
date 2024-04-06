/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { render } from "../solid-xul/solid-xul.js";
import { webpanel } from "./webpanel/webpanel.jsx";
import { addContextBox } from "./webpanel/webpanel.jsx";

//@ts-expect-error ChromeUtils is Firefox-function
const { BrowserManagerSidebar } = ChromeUtils.importESModule(
  "resource:///modules/BrowserManagerSidebar.sys.mjs",
);

//@ts-expect-error ChromeUtils is Firefox-function
const { ContextualIdentityService } = ChromeUtils.importESModule(
  "resource://gre/modules/ContextualIdentityService.sys.mjs",
);

//@ts-expect-error ChromeUtils is Firefox-function
const { BrowserManagerSidebarPanelWindowUtils } = ChromeUtils.importESModule(
  "resource:///modules/BrowserManagerSidebarPanelWindowUtils.sys.mjs",
);

const gBrowserManagerSidebar = {
  _initialized: false,
  currentPanel: null,
  clickedWebpanel: null,
  webpanel: null,
  contextWebpanel: null,

  get STATIC_SIDEBAR_DATA() {
    return BrowserManagerSidebar.STATIC_SIDEBAR_DATA;
  },

  get BROWSER_SIDEBAR_DATA() {
    return JSON.parse(
      Services.prefs.getStringPref("floorp.browser.sidebar2.data", undefined),
    );
  },

  get sidebar_icons() {
    return [
      "sidebar2-back",
      "sidebar2-forward",
      "sidebar2-reload",
      "sidebar2-go-index",
    ];
  },

  getWebpanelIdBySelectedButtonId(selectId: string) {
    return selectId.replace("select-", "webpanel");
  },

  getSelectIdByWebpanelId(webpanelId: string) {
    return `select-${webpanelId}`;
  },

  getWebpanelObjectById(webpanelId: string) {
    return webpanelId.replace("select-", "");
  },

  async init() {
    if (this._initialized) {
      return;
    }

    addContextBox(
      "bsb-context-add",
      "bsb-context-add",
      (_ev) => {
        BrowserManagerSidebar.addPanel(
          //@ts-expect-error gContextMenu
          gContextMenu.browser.currentURI.spec,
          //@ts-expect-error gContextMenu
          gContextMenu.browser.getAttribute("usercontextid") ?? 0,
        );
      },
      "fill-login",
      () =>
        document.getElementById("context-viewsource").hidden ||
        !document.getElementById("context-viewimage").hidden,
    );

    addContextBox(
      "bsb-context-link-add",
      "bsb-context-link-add",
      (_ev) => {
        BrowserManagerSidebar.addPanel(
          BrowserManagerSidebar.addPanel(
            //@ts-expect-error gContextMenu
            gContextMenu.linkURL,
            //@ts-expect-error gContextMenu
            gContextMenu.browser.getAttribute("usercontextid") ?? 0,
          ),
        );
      },
      "context-sep-sendlinktodevice",
      () => document.getElementById("context-openlink").hidden,
    );

    Services.prefs.addObserver(
      "floorp.browser.sidebar2.global.webpanel.width",
      () => this.controllFunctions.setSidebarWidth(this.currentPanel),
    );
    Services.prefs.addObserver("floorp.browser.sidebar.enable", () =>
      this.controllFunctions.changeVisibleBrowserManagerSidebar(
        Services.prefs.getBoolPref("floorp.browser.sidebar.enable", true),
      ),
    );

    this.controllFunctions.changeVisibleBrowserManagerSidebar(
      Services.prefs.getBoolPref("floorp.browser.sidebar.enable", true),
    );
    Services.prefs.addObserver("floorp.browser.sidebar2.data", () => {
      for (const elem of gBrowserManagerSidebar.BROWSER_SIDEBAR_DATA.index) {
        if (
          document.querySelector(`#webpanel${elem}`) &&
          JSON.stringify(gBrowserManagerSidebar.BROWSER_SIDEBAR_DATA.data[elem])
        ) {
          if (
            gBrowserManagerSidebar.currentPanel === elem &&
            !(sidebarsplit2.getAttribute("hidden") === "true")
          ) {
            gBrowserManagerSidebar.controllFunctions.makeWebpanel(elem);
          } else {
            gBrowserManagerSidebar.controllFunctions.unloadWebpanel(elem);
          }
        }
      }
      gBrowserManagerSidebar.controllFunctions.makeSidebarIcon();
    });
    Services.obs.addObserver(
      gBrowserManagerSidebar.servicesObs,
      "obs-panel-re",
    );
    Services.obs.addObserver(
      gBrowserManagerSidebar.controllFunctions.changeVisibilityOfWebPanel,
      "floorp-change-panel-show",
    );

    const addbutton = document.getElementById("add-button");
    addbutton.ondragover = this.mouseEvent.dragOver;
    addbutton.ondragleave = this.mouseEvent.dragLeave;
    addbutton.ondrop = this.mouseEvent.drop;
    //startup functions
    this.controllFunctions.makeSidebarIcon();
    // sidebar display
    const sidebarsplit2 = document.getElementById("sidebar-splitter2");
    if (!(sidebarsplit2.getAttribute("hidden") === "true")) {
      this.controllFunctions.changeVisibilityOfWebPanel();
    }
    Object.assign(window, { gBrowserManagerSidebar: this });
    this._initialized = true;
  },

  // Sidebar button functions
  sidebarButtons(action: number) {
    const modeValuePref = this.currentPanel;
    const webpanel = document.getElementById(`webpanel${modeValuePref}`);
    switch (action) {
      case 0:
        if (webpanel.src.startsWith("chrome://browser/content/browser.xhtml")) {
          const webPanelId = webpanel.id.replace("webpanel", "");
          BrowserManagerSidebarPanelWindowUtils.goBackPanel(
            window,
            webPanelId,
            true,
          );
        } else {
          webpanel.goBack();
        }
        break;
      case 1:
        if (webpanel.src.startsWith("chrome://browser/content/browser.xhtml")) {
          const webPanelId = webpanel.id.replace("webpanel", "");
          BrowserManagerSidebarPanelWindowUtils.goForwardPanel(
            window,
            webPanelId,
            true,
          );
        } else {
          webpanel.goForward();
        }
        break;
      case 2:
        if (webpanel.src.startsWith("chrome://browser/content/browser.xhtml")) {
          const webPanelId = webpanel.id.replace("webpanel", "");
          BrowserManagerSidebarPanelWindowUtils.reloadPanel(
            window,
            webPanelId,
            true,
          );
        } else {
          webpanel.reloadWithFlags(Ci.nsIWebNavigation.LOAD_FLAGS_BYPASS_CACHE);
        }
        break;
      case 3:
        if (webpanel.src.startsWith("chrome://browser/content/browser.xhtml")) {
          const webPanelId = webpanel.id.replace("webpanel", "");
          BrowserManagerSidebarPanelWindowUtils.goIndexPagePanel(
            window,
            webPanelId,
            true,
          );
        } else {
          webpanel.gotoIndex();
        }
        break;
    }
  },

  // keep sidebar width for each webpanel
  keepWebPanelWidth() {
    const pref = this.currentPanel;
    const currentBSD = this.BROWSER_SIDEBAR_DATA;
    currentBSD.data[pref].width =
      document.getElementById("sidebar2-box").clientWidth;
    Services.prefs.setStringPref(
      "floorp.browser.sidebar2.data",
      JSON.stringify(currentBSD),
    );
  },

  // keep sidebar width for global
  keepWidthToGlobalValue() {
    Services.prefs.setIntPref(
      "floorp.browser.sidebar2.global.webpanel.width",
      document.getElementById("sidebar2-box").width,
    );
  },

  // Services Observer
  servicesObs(data_) {
    const data = data_.wrappedJSObject;
    switch (data.eventType) {
      case "mouseOver":
        document.getElementById(
          data.id.replace("BSB-", "select-"),
        ).style.border = "1px solid blue";
        gBrowserManagerSidebar.controllFunctions.setUserContextColorLine(
          data.id.replace("BSB-", ""),
        );
        break;
      case "mouseOut":
        document.getElementById(
          data.id.replace("BSB-", "select-"),
        ).style.border = "";
        gBrowserManagerSidebar.controllFunctions.setUserContextColorLine(
          data.id.replace("BSB-", ""),
        );
        break;
    }
  },

  selectSidebarItem(event: MouseEvent) {
    const target = event.target as HTMLElement;

    const custom_url_id = target.id.replace("select-", "");

    if (this.currentPanel === custom_url_id) {
      this.controllFunctions.changeVisibilityOfWebPanel();
    } else {
      this.currentPanel = custom_url_id;
      this.controllFunctions.visibleWebpanel();
    }
  },

  mouseEvent: {
    mouseOver(event) {
      Services.obs.notifyObservers(
        {
          eventType: "mouseOver",
          id: event.target.id,
        },
        "obs-panel",
      );
    },

    mouseOut(event) {
      Services.obs.notifyObservers(
        {
          eventType: "mouseOut",
          id: event.target.id,
        },
        "obs-panel",
      );
    },

    dragStart(event) {
      event.dataTransfer.setData("text/plain", event.target.id);
    },

    dragOver(event) {
      event.preventDefault();
      event.currentTarget.style.borderTop = "2px solid blue";
    },

    dragLeave(event) {
      event.currentTarget.style.borderTop = "";
    },

    drop(event) {
      event.preventDefault();
      const id = event.dataTransfer.getData("text/plain");
      const elm_drag = document.getElementById(id);
      event.currentTarget.parentNode.insertBefore(
        elm_drag,
        event.currentTarget,
      );
      event.currentTarget.style.borderTop = "";
      const currentBSD = gBrowserManagerSidebar.BROWSER_SIDEBAR_DATA;
      currentBSD.index.splice(0);
      for (const elem of Array.from(document.querySelectorAll(".sicon-list"))) {
        currentBSD.index.push(
          gBrowserManagerSidebar.getWebpanelObjectById(elem.id),
        );
      }
      Services.prefs.setStringPref(
        "floorp.browser.sidebar2.data",
        JSON.stringify(currentBSD),
      );
    },
  },

  // Browser Manager Sidebar's webpanel selected vbox's context menu
  contextMenu: {
    show(event) {
      if (!event.explicitOriginalTarget.classList.contains("sidepanel-icon")) {
        return;
      }

      gBrowserManagerSidebar.clickedWebpanel = event.explicitOriginalTarget.id;
      gBrowserManagerSidebar.webpanel =
        gBrowserManagerSidebar.getWebpanelIdBySelectedButtonId(
          gBrowserManagerSidebar.clickedWebpanel,
        );
      gBrowserManagerSidebar.contextWebpanel = document.getElementById(
        gBrowserManagerSidebar.webpanel,
      );

      const needLoadedWebpanel =
        document.getElementsByClassName("needLoadedWebpanel");
      for (let i = 0; i < needLoadedWebpanel.length; i++) {
        needLoadedWebpanel[i].disabled = false;
        if (gBrowserManagerSidebar.contextWebpanel == null) {
          needLoadedWebpanel[i].disabled = true;
        } else if (
          needLoadedWebpanel[i].classList.contains(
            "needRunningExtensionsPanel",
          ) &&
          !gBrowserManagerSidebar.contextWebpanel.src.startsWith(
            "chrome://browser/content/browser.xhtml",
          )
        ) {
          needLoadedWebpanel[i].disabled = true;
        }
      }
    },

    showWithNumber(num: number) {
      const targetWebpanel = document.getElementsByClassName("sicon-list")[num];
      targetWebpanel.click();
    },

    unloadWebpanel() {
      gBrowserManagerSidebar.controllFunctions.unloadWebpanel(
        gBrowserManagerSidebar.getWebpanelObjectById(
          gBrowserManagerSidebar.clickedWebpanel,
        ),
      );
    },

    zoomIn() {
      const webPanelId = gBrowserManagerSidebar.contextWebpanel.id.replace(
        "webpanel",
        "",
      );
      BrowserManagerSidebarPanelWindowUtils.zoomInPanel(
        window,
        webPanelId,
        true,
      );
    },

    zoomOut() {
      const webPanelId = gBrowserManagerSidebar.contextWebpanel.id.replace(
        "webpanel",
        "",
      );
      BrowserManagerSidebarPanelWindowUtils.zoomOutPanel(
        window,
        webPanelId,
        true,
      );
    },

    resetZoom() {
      const webPanelId = gBrowserManagerSidebar.contextWebpanel.id.replace(
        "webpanel",
        "",
      );
      BrowserManagerSidebarPanelWindowUtils.resetZoomPanel(
        window,
        webPanelId,
        true,
      );
    },

    changeUserAgent() {
      const id = gBrowserManagerSidebar.getWebpanelObjectById(
        gBrowserManagerSidebar.clickedWebpanel,
      );
      const currentBSD = gBrowserManagerSidebar.BROWSER_SIDEBAR_DATA;

      const pref = currentBSD.data[id];
      const currentUserAgentPref = !!pref.userAgent;

      currentBSD.data[id].userAgent = !currentUserAgentPref;

      Services.prefs.setStringPref(
        "floorp.browser.sidebar2.data",
        JSON.stringify(currentBSD),
      );

      // reload webpanel if it is
      // selected. If not, unload it
      if (gBrowserManagerSidebar.currentPanel === id) {
        gBrowserManagerSidebar.controllFunctions.unloadWebpanel(id);
        gBrowserManagerSidebar.currentPanel = id;
        gBrowserManagerSidebar.controllFunctions.visibleWebpanel();
      } else {
        gBrowserManagerSidebar.controllFunctions.unloadWebpanel(id);
      }
    },

    deleteWebpanel() {
      if (
        document.getElementById("sidebar-splitter2").getAttribute("hidden") !==
          "true" &&
        gBrowserManagerSidebar.currentPanel ===
          gBrowserManagerSidebar.clickedWebpanel
      ) {
        gBrowserManagerSidebar.controllFunctions.changeVisibilityOfWebPanel();
      }

      const currentBSD = gBrowserManagerSidebar.BROWSER_SIDEBAR_DATA;
      const index = currentBSD.index.indexOf(
        gBrowserManagerSidebar.getWebpanelObjectById(
          gBrowserManagerSidebar.clickedWebpanel,
        ),
      );
      currentBSD.index.splice(index, 1);
      delete currentBSD.data[
        gBrowserManagerSidebar.getWebpanelObjectById(
          gBrowserManagerSidebar.clickedWebpanel,
        )
      ];
      Services.prefs.setStringPref(
        "floorp.browser.sidebar2.data",
        JSON.stringify(currentBSD),
      );
      gBrowserManagerSidebar.contextWebpanel?.remove();
      document.getElementById(gBrowserManagerSidebar.clickedWebpanel)?.remove();
    },

    muteWebpanel() {
      if (gBrowserManagerSidebar.contextWebpanel.audioMuted) {
        gBrowserManagerSidebar.contextWebpanel.unmute();
      } else {
        gBrowserManagerSidebar.contextWebpanel.mute();
      }
      gBrowserManagerSidebar.contextMenu.setMuteIcon();

      if (
        gBrowserManagerSidebar.contextWebpanel.src.startsWith(
          "chrome://browser/content/browser.xhtml",
        )
      ) {
        const webPanelId = gBrowserManagerSidebar.contextWebpanel.id.replace(
          "webpanel",
          "",
        );
        BrowserManagerSidebarPanelWindowUtils.toggleMutePanel(
          window,
          webPanelId,
          true,
        );
      }
    },

    setMuteIcon() {
      if (gBrowserManagerSidebar.contextWebpanel.audioMuted) {
        document
          .getElementById(gBrowserManagerSidebar.clickedWebpanel)
          .setAttribute("muted", "true");
      } else {
        document
          .getElementById(gBrowserManagerSidebar.clickedWebpanel)
          .removeAttribute("muted");
      }
    },
  },

  controllFunctions: {
    visiblePanelBrowserElem() {
      const modeValuePref = gBrowserManagerSidebar.currentPanel;
      const selectedwebpanel = document.getElementById(
        `webpanel${modeValuePref}`,
      );
      const selectedURL =
        gBrowserManagerSidebar.BROWSER_SIDEBAR_DATA.data[modeValuePref].url ??
        "";
      gBrowserManagerSidebar.controllFunctions.changeVisibleCommandButton(
        selectedURL.startsWith("floorp//"),
      );
      for (const elem of Array.from(
        document.getElementsByClassName("webpanels"),
      )) {
        elem.hidden = true;
        if (
          elem.classList.contains("isFloorp") ||
          elem.classList.contains("isExtension")
        ) {
          const src = elem.getAttribute("src");
          elem.setAttribute("src", "");
          elem.setAttribute("src", src);
        }
      }
      if (
        document.getElementById("sidebar-splitter2").getAttribute("hidden") ===
        "true"
      ) {
        gBrowserManagerSidebar.controllFunctions.changeVisibilityOfWebPanel();
      }
      gBrowserManagerSidebar.controllFunctions.changeCheckPanel(
        document.getElementById("sidebar-splitter2").getAttribute("hidden") !==
          "true",
      );
      if (selectedwebpanel != null) {
        selectedwebpanel.hidden = false;
      }
    },

    unloadWebpanel(id) {
      const sidebarsplit2 = document.getElementById("sidebar-splitter2");
      if (id === gBrowserManagerSidebar.currentPanel) {
        gBrowserManagerSidebar.currentPanel = null;
        if (sidebarsplit2.getAttribute("hidden") !== "true") {
          gBrowserManagerSidebar.controllFunctions.changeVisibilityOfWebPanel();
        }
      }
      document.getElementById(`webpanel${id}`)?.remove();
      document.getElementById(`select-${id}`).removeAttribute("muted");
    },

    unloadAllWebpanel() {
      for (const elem of Array.from(
        document.getElementsByClassName("webpanels"),
      )) {
        elem.remove();
      }
      for (const elem of Array.from(
        document.getElementsByClassName("sidepanel-icon"),
      )) {
        elem.removeAttribute("muted");
      }
      gBrowserManagerSidebar.currentPanel = null;
    },

    setUserContextColorLine(id: string) {
      const webpanel_usercontext =
        gBrowserManagerSidebar.BROWSER_SIDEBAR_DATA.data[id].usercontext ?? 0;
      const container_list = ContextualIdentityService.getPublicIdentities();
      if (
        webpanel_usercontext !== 0 &&
        container_list.findIndex(
          (e) => e.userContextId === webpanel_usercontext,
        ) !== -1
      ) {
        const container_color =
          container_list[
            container_list.findIndex(
              (e) => e.userContextId === webpanel_usercontext,
            )
          ].color;
        document.getElementById(`select-${id}`).style.borderLeft = `solid 2px ${
          container_color === "toolbar"
            ? "var(--toolbar-field-color)"
            : container_color
        }`;
      } else if (
        document.getElementById(`select-${id}`).style.border !==
        "1px solid blue"
      ) {
        document.getElementById(`select-${id}`).style.borderLeft = "";
      }
    },

    changeCheckPanel(doChecked: boolean) {
      for (const elem of Array.from(
        document.getElementsByClassName("sidepanel-icon"),
      )) {
        elem.setAttribute("checked", "false");
      }
      if (doChecked) {
        const selectedNode = document.querySelector(
          `#select-${gBrowserManagerSidebar.currentPanel}`,
        );
        if (selectedNode != null) {
          selectedNode.setAttribute("checked", "true");
        }
      }
    },

    changeVisibleBrowserManagerSidebar(doVisible) {
      if (doVisible) {
        document.querySelector("html").removeAttribute("invisibleBMS");
      } else {
        document.querySelector("html").setAttribute("invisibleBMS", "true");
      }
    },

    changeVisibleCommandButton(hidden) {
      for (const elem of gBrowserManagerSidebar.sidebar_icons) {
        document.getElementById(elem).hidden = hidden;
      }
    },

    changeVisibilityOfWebPanel() {
      const siderbar2header = document.getElementById("sidebar2-header");
      const sidebarsplit2 = document.getElementById("sidebar-splitter2");
      const sidebar2box = document.getElementById("sidebar2-box");
      const sidebarSetting = {
        true: ["auto", "", "", "false"],
        false: ["0", "0", "none", "true"],
      };
      const doDisplay = sidebarsplit2.getAttribute("hidden") === "true";
      sidebar2box.style.minWidth = sidebarSetting[Number(doDisplay)][0];
      sidebar2box.style.maxWidth = sidebarSetting[Number(doDisplay)][1];
      siderbar2header.style.display = sidebarSetting[Number(doDisplay)][2];
      sidebarsplit2.setAttribute(
        "hidden",
        sidebarSetting[Number(doDisplay)][3],
      );
      gBrowserManagerSidebar.controllFunctions.changeCheckPanel(doDisplay);
      Services.prefs.setBoolPref(
        "floorp.browser.sidebar.is.displayed",
        doDisplay,
      );

      // We should focus to parent window to avoid focus to webpanel
      if (!doDisplay) {
        window.focus();
      }

      if (
        Services.prefs.getBoolPref(
          "floorp.browser.sidebar2.hide.to.unload.panel.enabled",
          false,
        ) &&
        !doDisplay
      ) {
        gBrowserManagerSidebar.controllFunctions.unloadAllWebpanel();
      }
    },

    setSidebarWidth(webpanel_id) {
      if (
        webpanel_id != null &&
        gBrowserManagerSidebar.BROWSER_SIDEBAR_DATA.index.includes(webpanel_id)
      ) {
        const panelWidth =
          gBrowserManagerSidebar.BROWSER_SIDEBAR_DATA.data[webpanel_id].width ??
          Services.prefs.getIntPref(
            "floorp.browser.sidebar2.global.webpanel.width",
            undefined,
          );
        document.getElementById("sidebar2-box").style.width = `${panelWidth}px`;
      }
    },

    visibleWebpanel() {
      const webpanel_id = gBrowserManagerSidebar.currentPanel;
      if (
        webpanel_id != null &&
        gBrowserManagerSidebar.BROWSER_SIDEBAR_DATA.index.includes(webpanel_id)
      ) {
        gBrowserManagerSidebar.controllFunctions.makeWebpanel(webpanel_id);
      }
    },

    makeWebpanel(webpanel_id: string) {
      const webpandata =
        gBrowserManagerSidebar.BROWSER_SIDEBAR_DATA.data[webpanel_id];
      let webpanobject = document.getElementById(`webpanel${webpanel_id}`);
      let webpanelURL = webpandata.url;
      const webpanel_usercontext = webpandata.usercontext ?? 0;
      const webpanel_userAgent = webpandata.userAgent ?? false;
      const isExtension = webpanelURL.slice(0, 9) === "extension";
      let isWeb = true;
      let isFloorp = false;
      gBrowserManagerSidebar.controllFunctions.setSidebarWidth(webpanel_id);
      if (webpanelURL.startsWith("floorp//")) {
        isFloorp = true;
        webpanelURL =
          gBrowserManagerSidebar.STATIC_SIDEBAR_DATA[webpanelURL].url;
        isWeb = false;
      }
      if (
        webpanobject != null &&
        ((!(
          webpanobject?.getAttribute("changeuseragent") === "" &&
          !webpanel_userAgent
        ) &&
          !(
            webpanobject?.getAttribute("usercontextid") === "" &&
            webpanel_usercontext === 0
          ) &&
          ((webpanobject?.getAttribute("changeuseragent") ?? "false") !==
            String(webpanel_userAgent) ||
            (webpanobject?.getAttribute("usercontextid") ?? "0") !==
              String(webpanel_usercontext))) ||
          ((webpanobject.className.includes("isFloorp") ||
            webpanobject.className.includes("isWeb")) &&
            isFloorp))
      ) {
        webpanobject.remove();
        webpanobject = null;
      }

      // Add-on Capability
      if (
        Services.prefs.getBoolPref("floorp.browser.sidebar2.addons.enabled") &&
        !isExtension
      ) {
        isWeb = false;
      }

      if (webpanelURL.slice(0, 9) === "extension") {
        webpanelURL = webpanelURL.split(",")[3];
      }

      if (webpanobject == null) {
        const addonsEnabled = Services.prefs.getBoolPref(
          "floorp.browser.sidebar2.addons.enabled",
        );

        const src =
          addonsEnabled && !isFloorp && !isExtension
            ? `chrome://browser/content/browser.xhtml?${webpanelURL}?${webpanel_usercontext}?${webpanel_userAgent}?${webpanel_id}`
            : webpanelURL;
        const elem = webpanel(
          src,
          isFloorp,
          isExtension,
          isWeb,
          webpanel_userAgent,
        );
        render(() => elem, document.getElementById("sidebar2-box"));
        // .appendChild(webpanelElem);
      } else {
        if (
          !Services.prefs.getBoolPref("floorp.browser.sidebar2.addons.enabled")
        ) {
          webpanobject.setAttribute("src", webpanelURL);
        }
      }
      gBrowserManagerSidebar.controllFunctions.visiblePanelBrowserElem();
    },

    // Add Sidebar Icon to Sidebar's select box
    makeSidebarIcon() {
      for (const elem of gBrowserManagerSidebar.BROWSER_SIDEBAR_DATA.index) {
        if (document.getElementById(`select-${elem}`) == null) {
          const sidebarItem = document.createXULElement("toolbarbutton");
          sidebarItem.id = `select-${elem}`;
          sidebarItem.classList.add("sidepanel-icon");
          sidebarItem.classList.add("sicon-list");
          sidebarItem.setAttribute(
            "oncommand",
            "gBrowserManagerSidebar.selectSidebarItem(event)",
          );
          if (
            gBrowserManagerSidebar.BROWSER_SIDEBAR_DATA.data[elem].url.slice(
              0,
              8,
            ) === "floorp//"
          ) {
            if (
              gBrowserManagerSidebar.BROWSER_SIDEBAR_DATA.data[elem].url in
              gBrowserManagerSidebar.STATIC_SIDEBAR_DATA
            ) {
              //0~4 - StaticModeSetter | Browser Manager, Bookmark, History, Downloads
              sidebarItem.setAttribute(
                "data-l10n-id",
                `show-${
                  gBrowserManagerSidebar.STATIC_SIDEBAR_DATA[
                    gBrowserManagerSidebar.BROWSER_SIDEBAR_DATA.data[elem].url
                  ].l10n
                }`,
              );
              sidebarItem.setAttribute("context", "all-panel-context");
            }
          } else {
            //5~ CustomURLSetter | Custom URL have l10n, Userangent, Delete panel & etc...
            sidebarItem.classList.add("webpanel-icon");
            sidebarItem.setAttribute("context", "webpanel-context");
            sidebarItem.setAttribute(
              "tooltiptext",
              gBrowserManagerSidebar.BROWSER_SIDEBAR_DATA.data[elem].url,
            );
          }

          if (
            gBrowserManagerSidebar.BROWSER_SIDEBAR_DATA.data[elem].url.slice(
              0,
              9,
            ) === "extension"
          ) {
            sidebarItem.setAttribute(
              "tooltiptext",
              gBrowserManagerSidebar.BROWSER_SIDEBAR_DATA.data[elem].url.split(
                ",",
              )[1],
            );
            sidebarItem.className += " extension-icon";
            const listTexts =
              "chrome://browser/content/BMS-extension-needs-white-bg.txt";
            fetch(listTexts)
              .then((response) => {
                return response.text();
              })
              .then((text) => {
                const lines = text.split(/\r?\n/);
                for (const line of lines) {
                  if (
                    line ===
                    gBrowserManagerSidebar.BROWSER_SIDEBAR_DATA.data[
                      elem
                    ].url.split(",")[2]
                  ) {
                    sidebarItem.className += " extension-icon-add-white";
                    break;
                  }
                }
              });
          } else {
            sidebarItem.style.listStyleImage = "";
          }

          sidebarItem.onmouseover = gBrowserManagerSidebar.mouseEvent.mouseOver;
          sidebarItem.onmouseout = gBrowserManagerSidebar.mouseEvent.mouseOut;
          sidebarItem.ondragstart = gBrowserManagerSidebar.mouseEvent.dragStart;
          sidebarItem.ondragover = gBrowserManagerSidebar.mouseEvent.dragOver;
          sidebarItem.ondragleave = gBrowserManagerSidebar.mouseEvent.dragLeave;
          sidebarItem.ondrop = gBrowserManagerSidebar.mouseEvent.drop;
          const sidebarItemImage = document.createXULElement("image");
          sidebarItemImage.classList.add("toolbarbutton-icon");
          sidebarItem.appendChild(sidebarItemImage);
          const sidebarItemLabel = document.createXULElement("label");
          sidebarItemLabel.classList.add("toolbarbutton-text");
          sidebarItemLabel.setAttribute("crop", "right");
          sidebarItemLabel.setAttribute("flex", "1");
          sidebarItem.appendChild(sidebarItemLabel);
          document
            .getElementById("panelBox")
            .insertBefore(sidebarItem, document.getElementById("add-button"));
        } else {
          const sidebarItem = document.getElementById(`select-${elem}`);
          if (
            gBrowserManagerSidebar.BROWSER_SIDEBAR_DATA.data[elem].url.slice(
              0,
              8,
            ) === "floorp//"
          ) {
            if (
              gBrowserManagerSidebar.BROWSER_SIDEBAR_DATA.data[elem].url in
              gBrowserManagerSidebar.STATIC_SIDEBAR_DATA
            ) {
              sidebarItem.classList.remove("webpanel-icon");
              sidebarItem.setAttribute(
                "data-l10n-id",
                `show-${
                  gBrowserManagerSidebar.STATIC_SIDEBAR_DATA[
                    gBrowserManagerSidebar.BROWSER_SIDEBAR_DATA.data[elem].url
                  ].l10n
                }`,
              );
              sidebarItem.setAttribute("context", "all-panel-context");
            }
          } else {
            sidebarItem.classList.add("webpanel-icon");
            sidebarItem.removeAttribute("data-l10n-id");
            sidebarItem.setAttribute("context", "webpanel-context");
          }
          document
            .getElementById("panelBox")
            .insertBefore(sidebarItem, document.getElementById("add-button"));
        }
      }
      const siconAll = document.querySelectorAll(".sicon-list");
      const sicon = siconAll.length;
      const side = gBrowserManagerSidebar.BROWSER_SIDEBAR_DATA.index.length;
      if (sicon > side) {
        for (let i = 0; i < sicon - side; i++) {
          if (
            document.getElementById(
              siconAll[i].id.replace("select-", "webpanel"),
            ) != null
          ) {
            const sidebarsplit2 = document.getElementById("sidebar-splitter2");
            if (
              gBrowserManagerSidebar.currentPanel ===
              siconAll[i].id.replace("select-", "")
            ) {
              gBrowserManagerSidebar.currentPanel = null;
              gBrowserManagerSidebar.controllFunctions.visibleWebpanel();
              if (sidebarsplit2.getAttribute("hidden") !== "true") {
                gBrowserManagerSidebar.controllFunctions.changeVisibilityOfWebPanel();
              }
            }
            document
              .getElementById(siconAll[i].id.replace("select-", "webpanel"))
              .remove();
          }
          siconAll[i].remove();
        }
      }
      for (const elem of Array.from(
        document.querySelectorAll(".sidepanel-icon"),
      )) {
        if (elem.className.includes("webpanel-icon")) {
          const sbar_url =
            gBrowserManagerSidebar.BROWSER_SIDEBAR_DATA.data[elem.id.slice(7)]
              .url;
          BrowserManagerSidebar.getFavicon(
            sbar_url,
            document.getElementById(`${elem.id}`),
          );
          gBrowserManagerSidebar.controllFunctions.setUserContextColorLine(
            elem.id.slice(7),
          );
        } else {
          elem.style.removeProperty("--BMSIcon");
        }
      }
    },

    toggleBMSShortcut() {
      if (!Services.prefs.getBoolPref("floorp.browser.sidebar.enable")) {
        return;
      }

      if (gBrowserManagerSidebar.currentPanel == null) {
        gBrowserManagerSidebar.currentPanel =
          gBrowserManagerSidebar.BROWSER_SIDEBAR_DATA.index[0];
        gBrowserManagerSidebar.controllFunctions.visibleWebpanel();
        gBrowserManagerSidebar.controllFunctions.changeVisibilityOfWebPanel();
      }
      gBrowserManagerSidebar.controllFunctions.changeVisibilityOfWebPanel();
    },
  },
};

// Initialize Browser Manager Sidebar
gBrowserManagerSidebar.init();
