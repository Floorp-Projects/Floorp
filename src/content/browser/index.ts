import { setBrowserDesign } from "./setBrowserDesign";
import { initSidebar } from "./browser-sidebar/index";
import { CustomShortcutKey } from "./custom-shortcut-key";

export function initContentScripts() {
  //@ts-expect-error ii
  SessionStore.promiseInitialized.then(() => {
    setBrowserDesign();
    //createWebpanel("tmp", "https://manatoki332.net/");
    //console.log(document.getElementById("tmp"));
    //window.gBrowserManagerSidebar = CBrowserManagerSidebar.getInstance();
    import("./testButton");
    console.log("csk getinstance");
    CustomShortcutKey.getInstance();
    initSidebar();
  });
}
