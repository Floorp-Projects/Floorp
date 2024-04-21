import type { CBrowserManagerSidebar } from "@content/browser-manager-sidebar";

//@ts-expect-error ChromeUtils exists
const { LoginHelper } = ChromeUtils.importESModule(
  "resource://gre/modules/LoginHelper.sys.mjs",
);

export function sidebarSelectBox(bms: CBrowserManagerSidebar) {
  return (
    <xul:vbox
      id="sidebar-select-box"
      style="overflow: hidden auto;"
      class="webpanel-box chromeclass-extrachrome"
    >
      <xul:vbox id="panelBox">
        <xul:toolbarbutton
          class="sidepanel-browser-icon"
          data-l10n-id="sidebar-add-button"
          onClick={() => {
            bms.utils.addPanel();
          }}
          onDragover={bms.mouseEvent.dragOver}
          onDragleave={bms.mouseEvent.dragLeave}
          onDrop={bms.mouseEvent.drop}
          id="add-button"
        />
      </xul:vbox>
      <xul:spacer flex="1" />
      <xul:vbox id="bottomButtonBox">
        <xul:toolbarbutton
          class="sidepanel-browser-icon"
          data-l10n-id="sidebar2-hide-sidebar"
          onClick={() => {
            Services.prefs.setBoolPref("floorp.browser.sidebar.enable", false);
          }}
          id="sidebar-hide-icon"
        />
        <xul:toolbarbutton
          class="sidepanel-browser-icon"
          data-l10n-id="sidebar-addons-button"
          onClick={() => {
            //https://searchfox.org/mozilla-central/rev/56f7d50bd9dcf8b93a9c65d31c4286a735224cc9/browser/base/content/browser-addons.js#1144
            //TODO: argument required
            window.BrowserAddonUI.openAddonsMgr();
          }}
          id="addons-icon"
        />
        <xul:toolbarbutton
          class="sidepanel-browser-icon"
          data-l10n-id="sidebar-passwords-button"
          onClick={() => {
            LoginHelper.openPasswordManager(window, {
              entrypoint: "mainmenu",
            });
          }}
          id="passwords-icon"
        />
        <xul:toolbarbutton
          class="sidepanel-browser-icon"
          data-l10n-id="sidebar-preferences-button"
          onClick={() => {
            window.openPreferences();
          }}
          id="preferences-icon"
        />
      </xul:vbox>
    </xul:vbox>
  );
}
