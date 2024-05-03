import "@solid-xul/jsx-runtime";
import { render } from "@solid-xul/solid-xul";

const init = async () => {
  render(
    () => (
      <xul:window>
        <xul:browser
          id="tmp"
          contextmenu="contentAreaContextMenu"
          message="true"
          messagemanagergroup="browsers"
          tooltip="aHTMLTooltip"
          type="content"
          maychangeremoteness="true"
          initiallyactive="false"
          // remote="true"
          autocompletepopup="PopupAutoComplete"
          src={"chrome://browser/content/browser.xhtml"}
          flex="1"
          disablefullscreen="true"
          disablehistory="true"
          style="width:100px"
          manualactiveness="true"
          //nodefaultsrc="true"
        />
      </xul:window>
    ),
    document.body,
  );
};

if (document.readyState !== "loading") {
  init();
} else {
  document.addEventListener("DOMContentLoaded", init);
}
