import { render } from "@nora/solid-xul";
import { QRCodePageActionButton } from "./qr-code-button";
import { QRCodePanel } from "./qr-code-panel";
import { QRCodeManager } from "./qr-code-manager";
import { noraComponent, NoraComponentBase } from "@core/utils/base";

export let manager: QRCodeManager;

@noraComponent(import.meta.hot)
export default class QRCodeGenerator extends NoraComponentBase {
  init() {
    manager = new QRCodeManager();
    const mainPopupSet = document?.getElementById("mainPopupSet");
    if (mainPopupSet) {
      render(QRCodePanel, mainPopupSet, {
        hotCtx: import.meta.hot,
      });
    }

    window.SessionStore.promiseInitialized.then(() => {
      const starButtonBox = document?.getElementById("star-button-box");
      if (starButtonBox && starButtonBox.parentElement) {
        render(QRCodePageActionButton, starButtonBox.parentElement, {
          marker: starButtonBox,
          hotCtx: import.meta.hot,
        });
      }
    });

    manager.init();
  }
}
