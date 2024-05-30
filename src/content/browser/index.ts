import { setBrowserDesign } from "./setBrowserDesign";
import { initSidebar } from "./browser-sidebar";
import { CustomShortcutKey } from "@modules/custom-shortcut-key";
import { initStatusbar } from "./statusbar";
import { initBrowserContextMenu } from "./browser-context-menu";

export default function initScripts() {
  //@ts-expect-error ii
  SessionStore.promiseInitialized.then(() => {
    initBrowserContextMenu();
    setBrowserDesign();

    //createWebpanel("tmp", "https://manatoki332.net/");
    //console.log(document.getElementById("tmp"));
    //window.gBrowserManagerSidebar = CBrowserManagerSidebar.getInstance();
    import("./testButton");
    initStatusbar();
    console.log("csk getinstance");
    CustomShortcutKey.getInstance();
    initSidebar();
  });
}
