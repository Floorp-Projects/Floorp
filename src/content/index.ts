import { render } from "../solid-xul/solid-xul";
import { createWebpanel } from "./browser-manager-sidebar";
import { setBrowserDesign } from "./setBrowserDesign";
import { webpanel2base } from "./webpanel/webpanel";
import { loadPanel } from "./webpanel/webpanels-ext";
import "./browser-manager-sidebar";

//console.log("hi!");
//@ts-expect-error ii
SessionStore.promiseInitialized.then(() => {
  setBrowserDesign();

  //createWebpanel("tmp", "https://manatoki332.net/");
  //console.log(document.getElementById("tmp"));
});

//Services.obs.addObserver(setBrowserDesign, "browser-window-before-show");
