import type { CBrowserManagerSidebar } from ".";

export class BMSContextMenu {
  private bmsInstance: CBrowserManagerSidebar;
  constructor(bmsInstance: CBrowserManagerSidebar) {
    // Do not write DOM-related process in here.
    // This runned on create of BMSInstance.
    this.bmsInstance = bmsInstance;
  }
  show(event) {
    if (!event.explicitOriginalTarget.classList.contains("sidepanel-icon")) {
      return;
    }

    this.bmsInstance.clickedWebpanel = event.explicitOriginalTarget.id;
    this.bmsInstance.webpanel =
      this.bmsInstance.getWebpanelIdBySelectedButtonId(
        this.bmsInstance.clickedWebpanel,
      );
    this.bmsInstance.contextWebpanel = document.getElementById(
      this.bmsInstance.webpanel,
    );

    const needLoadedWebpanel =
      document.getElementsByClassName("needLoadedWebpanel");
    for (let i = 0; i < needLoadedWebpanel.length; i++) {
      needLoadedWebpanel[i].disabled = false;
      if (this.bmsInstance.contextWebpanel == null) {
        needLoadedWebpanel[i].disabled = true;
      } else if (
        needLoadedWebpanel[i].classList.contains(
          "needRunningExtensionsPanel",
        ) &&
        !this.bmsInstance.contextWebpanel.src.startsWith(
          "chrome://browser/content/browser.xhtml",
        )
      ) {
        needLoadedWebpanel[i].disabled = true;
      }
    }
  }

  showWithNumber(num) {
    const targetWebpanel = document.getElementsByClassName("sicon-list")[num];
    targetWebpanel.click();
  }

  unloadWebpanel() {
    this.bmsInstance.controlFunctions.unloadWebpanel(
      this.bmsInstance.getWebpanelObjectById(this.bmsInstance.clickedWebpanel),
    );
  }

  zoomIn() {
    const webPanelId = this.bmsInstance.contextWebpanel.id.replace(
      "webpanel",
      "",
    );
    BrowserManagerSidebarPanelWindowUtils.zoomInPanel(window, webPanelId, true);
  }

  zoomOut() {
    const webPanelId = this.bmsInstance.contextWebpanel.id.replace(
      "webpanel",
      "",
    );
    BrowserManagerSidebarPanelWindowUtils.zoomOutPanel(
      window,
      webPanelId,
      true,
    );
  }

  resetZoom() {
    const webPanelId = this.bmsInstance.contextWebpanel.id.replace(
      "webpanel",
      "",
    );
    BrowserManagerSidebarPanelWindowUtils.resetZoomPanel(
      window,
      webPanelId,
      true,
    );
  }

  changeUserAgent() {
    const id = this.bmsInstance.getWebpanelObjectById(
      this.bmsInstance.clickedWebpanel,
    );
    const currentBSD = this.bmsInstance.BROWSER_SIDEBAR_DATA;

    const pref = currentBSD.data[id];
    const currentUserAgentPref = !!pref.userAgent;

    currentBSD.data[id].userAgent = !currentUserAgentPref;

    Services.prefs.setStringPref(
      "floorp.browser.sidebar2.data",
      JSON.stringify(currentBSD),
    );

    // reload webpanel if it is
    // selected. If not, unload it
    if (this.bmsInstance.currentPanel === id) {
      this.bmsInstance.controlFunctions.unloadWebpanel(id);
      this.bmsInstance.currentPanel = id;
      this.bmsInstance.controlFunctions.visibleWebpanel();
    } else {
      this.bmsInstance.controlFunctions.unloadWebpanel(id);
    }
  }

  deleteWebpanel() {
    if (
      document.getElementById("sidebar-splitter2").getAttribute("hidden") !==
        "true" &&
      this.bmsInstance.currentPanel === this.bmsInstance.clickedWebpanel
    ) {
      this.bmsInstance.controlFunctions.changeVisibilityOfWebPanel();
    }

    const currentBSD = this.bmsInstance.BROWSER_SIDEBAR_DATA;
    const index = currentBSD.index.indexOf(
      this.bmsInstance.getWebpanelObjectById(this.bmsInstance.clickedWebpanel),
    );
    currentBSD.index.splice(index, 1);
    delete currentBSD.data[
      this.bmsInstance.getWebpanelObjectById(this.bmsInstance.clickedWebpanel)
    ];
    Services.prefs.setStringPref(
      "floorp.browser.sidebar2.data",
      JSON.stringify(currentBSD),
    );
    this.bmsInstance.contextWebpanel?.remove();
    document.getElementById(this.bmsInstance.clickedWebpanel)?.remove();
  }

  muteWebpanel() {
    if (this.bmsInstance.contextWebpanel.audioMuted) {
      this.bmsInstance.contextWebpanel.unmute();
    } else {
      this.bmsInstance.contextWebpanel.mute();
    }
    this.setMuteIcon();

    if (
      this.bmsInstance.contextWebpanel.src.startsWith(
        "chrome://browser/content/browser.xhtml",
      )
    ) {
      const webPanelId = this.bmsInstance.contextWebpanel.id.replace(
        "webpanel",
        "",
      );
      BrowserManagerSidebarPanelWindowUtils.toggleMutePanel(
        window,
        webPanelId,
        true,
      );
    }
  }
  setMuteIcon() {
    if (this.bmsInstance.contextWebpanel.audioMuted) {
      document
        .getElementById(this.bmsInstance.clickedWebpanel)
        .setAttribute("muted", "true");
    } else {
      document
        .getElementById(this.bmsInstance.clickedWebpanel)
        .removeAttribute("muted");
    }
  }
}
