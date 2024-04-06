import { render } from "../solid-xul/solid-xul";
import { webpanel } from "./webpanel/webpanel";

export function createWebpanel(webpanelId: string, webpanelUrl: string) {
  //const addonsEnabled = Services.prefs.getBoolPref("floorp.browser.sidebar2.addons.enabled");

  const src = webpanelUrl;
  const elem = webpanel(webpanelId, src);
  render(() => elem, document.getElementById("browser"));
}
