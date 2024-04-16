import type { CBrowserManagerSidebar } from ".";
export class BMSControlFunctions {
  public bmsInstance: CBrowserManagerSidebar;
  constructor(BMSInstance: CBrowserManagerSidebar) {
    // Do not write DOM-related process in here.
    // This runned on create of BMSInstance.
    this.bmsInstance = BMSInstance;
  }
  visiblePanelBrowserElem() {
    const modeValuePref = this.bmsInstance.currentPanel;
    const selectedwebpanel = document.getElementById(
      `webpanel${modeValuePref}`,
    );
    const selectedURL =
      this.bmsInstance.BROWSER_SIDEBAR_DATA.data[modeValuePref].url ?? "";
    this.changeVisibleCommandButton(selectedURL.startsWith("floorp//"));
    for (const elem of document.getElementsByClassName("webpanels")) {
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
      this.changeVisibilityOfWebPanel();
    }
    this.changeCheckPanel(
      document.getElementById("sidebar-splitter2").getAttribute("hidden") !==
        "true",
    );
    if (selectedwebpanel != null) {
      selectedwebpanel.hidden = false;
    }
  }

  unloadWebpanel(id: string) {
    const sidebarsplit2 = document.getElementById("sidebar-splitter2");
    if (id === this.bmsInstance.currentPanel) {
      this.bmsInstance.currentPanel = null;
      if (sidebarsplit2.getAttribute("hidden") !== "true") {
        this.changeVisibilityOfWebPanel();
      }
    }
    document.getElementById(`webpanel${id}`)?.remove();
    document.getElementById(`select-${id}`).removeAttribute("muted");
  }

  unloadAllWebpanel() {
    for (const elem of document.getElementsByClassName("webpanels")) {
      elem.remove();
    }
    for (const elem of document.getElementsByClassName("sidepanel-icon")) {
      elem.removeAttribute("muted");
    }
    this.bmsInstance.currentPanel = null;
  }

  setUserContextColorLine(id: string) {
    const webpanel_usercontext =
      this.bmsInstance.BROWSER_SIDEBAR_DATA.data[id].usercontext ?? 0;
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
      document.getElementById(`select-${id}`).style.border !== "1px solid blue"
    ) {
      document.getElementById(`select-${id}`).style.borderLeft = "";
    }
  }

  changeCheckPanel(doChecked: boolean) {
    for (const elem of document.getElementsByClassName("sidepanel-icon")) {
      elem.setAttribute("checked", "false");
    }
    if (doChecked) {
      const selectedNode = document.querySelector(
        `#select-${this.bmsInstance.currentPanel}`,
      );
      if (selectedNode != null) {
        selectedNode.setAttribute("checked", "true");
      }
    }
  }

  changeVisibleBrowserManagerSidebar(doVisible: boolean) {
    if (doVisible) {
      document.querySelector("html").removeAttribute("invisibleBMS");
    } else {
      document.querySelector("html").setAttribute("invisibleBMS", "true");
    }
  }

  changeVisibleCommandButton(hidden: boolean) {
    for (const elem of this.bmsInstance.sidebar_icons) {
      document.getElementById(elem).hidden = hidden;
    }
  }

  changeVisibilityOfWebPanel() {
    const siderbar2header = document.getElementById("sidebar2-header");
    const sidebarsplit2 = document.getElementById("sidebar-splitter2");
    const sidebar2box = document.getElementById("sidebar2-box");
    const sidebarSetting = {
      true: ["auto", "", "", "false"],
      false: ["0", "0", "none", "true"],
    };
    const doDisplay = sidebarsplit2.getAttribute("hidden") === "true";
    sidebar2box.style.minWidth = sidebarSetting[doDisplay][0];
    sidebar2box.style.maxWidth = sidebarSetting[doDisplay][1];
    siderbar2header.style.display = sidebarSetting[doDisplay][2];
    sidebarsplit2.setAttribute("hidden", sidebarSetting[doDisplay][3]);
    this.changeCheckPanel(doDisplay);
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
      this.unloadAllWebpanel();
    }
  }

  setSidebarWidth(webpanel_id: string) {
    if (
      webpanel_id != null &&
      this.bmsInstance.BROWSER_SIDEBAR_DATA.index.includes(webpanel_id)
    ) {
      const panelWidth =
        this.bmsInstance.BROWSER_SIDEBAR_DATA.data[webpanel_id].width ??
        Services.prefs.getIntPref(
          "floorp.browser.sidebar2.global.webpanel.width",
          undefined,
        );
      document.getElementById("sidebar2-box").style.width = `${panelWidth}px`;
    }
  }

  visibleWebpanel() {
    const webpanel_id = this.bmsInstance.currentPanel;
    if (
      webpanel_id != null &&
      this.bmsInstance.BROWSER_SIDEBAR_DATA.index.includes(webpanel_id)
    ) {
      this.makeWebpanel(webpanel_id);
    }
  }

  makeWebpanel(webpanel_id: string) {
    const webpandata = this.bmsInstance.BROWSER_SIDEBAR_DATA.data[webpanel_id];
    const webpanobject = document.getElementById(`webpanel${webpanel_id}`);
    let webpanelURL = webpandata.url;
    const webpanel_usercontext = webpandata.usercontext ?? 0;
    const webpanel_userAgent = webpandata.userAgent ?? false;
    const isExtension = webpanelURL.slice(0, 9) === "extension";
    let isWeb = true;
    let isFloorp = false;
    this.setSidebarWidth(webpanel_id);

    if (webpanelURL.startsWith("floorp//")) {
      isFloorp = true;
      webpanelURL = this.bmsInstance.STATIC_SIDEBAR_DATA[webpanelURL].url;
      isWeb = false;
    }

    // Add-on Capability
    if (
      Services.prefs.getBoolPref("floorp.browser.sidebar2.addons.enabled") &&
      !isExtension
    ) {
      isWeb = false;
    }

    if (webpanobject == null) {
      const webpanelElem = window.MozXULElement.parseXULToFragment(`
              <browser
                id="webpanel${webpanel_id}"
                class="webpanels ${isFloorp ? "isFloorp" : "isWeb"} ${
                  isExtension ? "isExtension" : ""
                }"
                flex="1"
                xmlns="http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul"
                disablehistory="true"
                disablefullscreen="true"
                tooltip="aHTMLTooltip"
                autoscroll="false"
                disableglobalhistory="true"
                messagemanagergroup="browsers"
                autocompletepopup="PopupAutoComplete"
                initialBrowsingContextGroupId="40"
                ${
                  isWeb
                    ? `usercontextid="${
                        typeof webpanel_usercontext === "number"
                          ? String(webpanel_usercontext)
                          : "0"
                      }"
                changeuseragent="${webpanel_userAgent ? "true" : "false"}"
                webextension-view-type="sidebar"
                type="content"
                remote="true"
                maychangeremoteness="true"
                context=""
                `
                    : ""
                }
              />
                `);
      if (webpanelURL.slice(0, 9) === "extension") {
        webpanelURL = webpanelURL.split(",")[3];
      }

      if (
        Services.prefs.getBoolPref("floorp.browser.sidebar2.addons.enabled") &&
        !isFloorp &&
        !isExtension
      ) {
        webpanelElem.firstChild.setAttribute(
          "src",
          `chrome://browser/content/browser.xhtml?&floorpWebPanelId=${webpanel_id}`,
        );
      } else {
        webpanelElem.firstChild.setAttribute("src", webpanelURL);
      }
      document.getElementById("sidebar2-box").appendChild(webpanelElem);
    } else {
      if (webpanelURL.slice(0, 9) === "extension") {
        webpanelURL = webpanelURL.split(",")[3];
      }

      if (
        Services.prefs.getBoolPref("floorp.browser.sidebar2.addons.enabled")
      ) {
        /* empty */
      } else {
        webpanobject.setAttribute("src", webpanelURL);
      }
    }
    this.visiblePanelBrowserElem();
  }

  // Add Sidebar Icon to Sidebar's select box
  makeSidebarIcon() {
    for (const elem of this.bmsInstance.BROWSER_SIDEBAR_DATA.index) {
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
          this.bmsInstance.BROWSER_SIDEBAR_DATA.data[elem].url.slice(0, 8) ===
          "floorp//"
        ) {
          if (
            this.bmsInstance.BROWSER_SIDEBAR_DATA.data[elem].url in
            this.bmsInstance.STATIC_SIDEBAR_DATA
          ) {
            //0~4 - StaticModeSetter | Browser Manager, Bookmark, History, Downloads
            sidebarItem.setAttribute(
              "data-l10n-id",
              "show-" +
                this.bmsInstance.STATIC_SIDEBAR_DATA[
                  this.bmsInstance.BROWSER_SIDEBAR_DATA.data[elem].url
                ].l10n,
            );
            sidebarItem.setAttribute("context", "all-panel-context");
          }
        } else {
          //5~ CustomURLSetter | Custom URL have l10n, Userangent, Delete panel & etc...
          sidebarItem.classList.add("webpanel-icon");
          sidebarItem.setAttribute("context", "webpanel-context");
          sidebarItem.setAttribute(
            "tooltiptext",
            this.bmsInstance.BROWSER_SIDEBAR_DATA.data[elem].url,
          );
        }

        if (
          this.bmsInstance.BROWSER_SIDEBAR_DATA.data[elem].url.slice(0, 9) ===
          "extension"
        ) {
          sidebarItem.setAttribute(
            "tooltiptext",
            this.bmsInstance.BROWSER_SIDEBAR_DATA.data[elem].url.split(",")[1],
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
                  this.bmsInstance.BROWSER_SIDEBAR_DATA.data[elem].url.split(
                    ",",
                  )[2]
                ) {
                  sidebarItem.className += " extension-icon-add-white";
                  break;
                }
              }
            });
        } else {
          sidebarItem.style.listStyleImage = "";
        }

        sidebarItem.onmouseover = this.bmsInstance.mouseEvent.mouseOver;
        sidebarItem.onmouseout = this.bmsInstance.mouseEvent.mouseOut;
        sidebarItem.ondragstart = this.bmsInstance.mouseEvent.dragStart;
        sidebarItem.ondragover = this.bmsInstance.mouseEvent.dragOver;
        sidebarItem.ondragleave = this.bmsInstance.mouseEvent.dragLeave;
        sidebarItem.ondrop = this.bmsInstance.mouseEvent.drop;
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
          this.bmsInstance.BROWSER_SIDEBAR_DATA.data[elem].url.slice(0, 8) ===
          "floorp//"
        ) {
          if (
            this.bmsInstance.BROWSER_SIDEBAR_DATA.data[elem].url in
            this.bmsInstance.STATIC_SIDEBAR_DATA
          ) {
            sidebarItem.classList.remove("webpanel-icon");
            sidebarItem.setAttribute(
              "data-l10n-id",
              "show-" +
                this.bmsInstance.STATIC_SIDEBAR_DATA[
                  this.bmsInstance.BROWSER_SIDEBAR_DATA.data[elem].url
                ].l10n,
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
    const side = this.bmsInstance.BROWSER_SIDEBAR_DATA.index.length;
    if (sicon > side) {
      for (let i = 0; i < sicon - side; i++) {
        if (
          document.getElementById(
            siconAll[i].id.replace("select-", "webpanel"),
          ) != null
        ) {
          const sidebarsplit2 = document.getElementById("sidebar-splitter2");
          if (
            this.bmsInstance.currentPanel ===
            siconAll[i].id.replace("select-", "")
          ) {
            this.bmsInstance.currentPanel = null;
            this.visibleWebpanel();
            if (sidebarsplit2.getAttribute("hidden") !== "true") {
              this.changeVisibilityOfWebPanel();
            }
          }
          document
            .getElementById(siconAll[i].id.replace("select-", "webpanel"))
            .remove();
        }
        siconAll[i].remove();
      }
    }
    for (const elem of document.querySelectorAll(".sidepanel-icon")) {
      if (elem.className.includes("webpanel-icon")) {
        const sbar_url =
          this.bmsInstance.BROWSER_SIDEBAR_DATA.data[elem.id.slice(7)].url;
        BrowserManagerSidebar.getFavicon(
          sbar_url,
          document.getElementById(`${elem.id}`),
        );
        this.setUserContextColorLine(elem.id.slice(7));
      } else {
        elem.style.removeProperty("--BMSIcon");
      }
    }
  }

  toggleBMSShortcut() {
    if (!Services.prefs.getBoolPref("floorp.browser.sidebar.enable")) {
      return;
    }

    if (this.bmsInstance.currentPanel == null) {
      this.bmsInstance.currentPanel =
        this.bmsInstance.BROWSER_SIDEBAR_DATA.index[0];
      this.visibleWebpanel();
      this.changeVisibilityOfWebPanel();
    }
    this.changeVisibilityOfWebPanel();
  }
}
