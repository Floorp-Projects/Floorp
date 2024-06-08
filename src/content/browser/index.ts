import { setBrowserDesign } from "./setBrowserDesign";
import { initSidebar } from "./browser-sidebar";
import { CustomShortcutKey } from "@modules/custom-shortcut-key";
import { initStatusbar } from "./statusbar";
import { initBrowserContextMenu } from "./browser-context-menu";
import { initShareMode } from "./browser-share-mode";
import { initProfileManager } from "./profile-manager";

export default function initScripts() {
  //@ts-expect-error ii
  SessionStore.promiseInitialized.then(() => {
    initBrowserContextMenu();
    setBrowserDesign();
    initShareMode();
    initProfileManager();

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
