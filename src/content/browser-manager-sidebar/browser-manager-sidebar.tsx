import "@solid-xul/jsx-runtime";

export function sidebar() {
  return (
    <>
      <xul:vbox
        id="sidebar2-box"
        style="min-width: 25em;"
        class="browser-sidebar2 chromeclass-extrachrome"
      >
        <xul:box id="sidebar2-header" style="min-height: 2.5em" align="center">
          <xul:toolbarbutton
            id="sidebar2-back"
            class="sidebar2-icon"
            style="margin-left: 0.5em;"
            data-l10n-id="sidebar-back-button"
            oncommand="gBrowserManagerSidebar.sidebarButtons(0);"
          />
          <xul:toolbarbutton
            id="sidebar2-forward"
            class="sidebar2-icon"
            style="margin-left: 1em;"
            data-l10n-id="sidebar-forward-button"
            oncommand="gBrowserManagerSidebar.sidebarButtons(1);"
          />
          <xul:toolbarbutton
            id="sidebar2-reload"
            class="sidebar2-icon"
            style="margin-left: 1em;"
            data-l10n-id="sidebar-reload-button"
            oncommand="gBrowserManagerSidebar.sidebarButtons(2);"
          />
          <xul:toolbarbutton
            id="sidebar2-go-index"
            class="sidebar2-icon"
            style="margin-left: 1em;"
            data-l10n-id="sidebar-go-index-button"
            oncommand="gBrowserManagerSidebar.sidebarButtons(3);"
          />
          <xul:spacer flex="1" />
          <xul:toolbarbutton
            id="sidebar2-keeppanelwidth"
            context="width-size-context"
            class="sidebar2-icon"
            style="margin-right: 0.5em;"
            data-l10n-id="sidebar-keepWidth-button"
            oncommand="gBrowserManagerSidebar.keepWebPanelWidth();"
          />
          <xul:toolbarbutton
            id="sidebar2-close"
            class="sidebar2-icon"
            style="margin-right: 0.5em;"
            data-l10n-id="sidebar2-close-button"
            oncommand="gBrowserManagerSidebar.controllFunctions.changeVisibilityOfWebPanel();"
          />
        </xul:box>
      </xul:vbox>
      <xul:splitter
        id="sidebar-splitter2"
        class="browser-sidebar2 chromeclass-extrachrome"
        hidden="false"
      />
    </>
  );
}
