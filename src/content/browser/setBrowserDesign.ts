import { applyUserJS } from "./applyUserJS.js";
import { loadStyleSheetWithNsStyleSheetService } from "./stylesheetService.js";

import floorpBaseCSS from "@skin/floorp/css/floorp.css?url";
import fluerialUICSS from "@skin/floorp-fluerial/css/fluerial.css?url";

export async function setBrowserDesign() {
  console.log("setBrowserDesign");
  //remove #browserdesign
  document.getElementById("browserdesign")?.remove();

  const updateNumber = new Date().getTime();
  const themeCSS = {
    LeptonUI: "chrome://noraneko/skin/lepton/css/leptonChrome.css",

    FluerialUI: "chrome://noraneko" + fluerialUICSS,
  };

  //@ts-ignore
  const scrollbox = document
    .querySelector("#tabbrowser-arrowscrollbox")
    .shadowRoot.querySelector<XULElement>("scrollbox");

  if (scrollbox) {
    scrollbox.style.marginTop = "7px";
  }

  const tag = document.createElement("link");
  tag.setAttribute("id", "browserdesign");

  tag.rel = "stylesheet";
  tag.href = themeCSS.FluerialUI;

  const floorpTag = document.createElement("link");
  floorpTag.setAttribute("id", "floorpdesign");
  floorpTag.rel = "stylesheet";
  floorpTag.href = "chrome://noraneko" + floorpBaseCSS;

  document.head?.appendChild(tag);
  document.head?.appendChild(floorpTag);
  await window.gURLBar._updateLayoutBreakoutDimensions();

  console.log("hi!");

  // } else {
  //   unloadStyleSheetWithNsStyleSheetService("chrome://browser/skin/lepton/leptonContent.css");
  //}
}
