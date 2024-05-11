import { setBrowserDesign } from "./setBrowserDesign";
import { CBrowserManagerSidebar } from "./browser-manager-sidebar";
import { initSidebar } from "./browser-sidebar/index";

export function initContentScripts() {
  //@ts-expect-error ii
  SessionStore.promiseInitialized.then(() => {
    setBrowserDesign();
    //createWebpanel("tmp", "https://manatoki332.net/");
    //console.log(document.getElementById("tmp"));
    //window.gBrowserManagerSidebar = CBrowserManagerSidebar.getInstance();
    import("./testButton");
    initSidebar();
  });
}
