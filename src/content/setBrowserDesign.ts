import { applyUserJS } from "./applyUserJS.js";
import { loadStyleSheetWithNsStyleSheetService } from "./stylesheetService.js";

export async function setBrowserDesign() {
  console.log("setBrowserDesign");
  //remove #browserdesign
  document.getElementById("browserdesign")?.remove();

  const updateNumber = new Date().getTime();
  const themeCSS = {
    LeptonUI: "chrome://noraneko/skin/lepton/css/leptonChrome.css",

    FluerialUI: "chrome://noraneko/skin/floorp-fluerial/css/fluerial.css",
  };

  const tag = document.createElement("link");
  tag.setAttribute("id", "browserdesign");

  tag.rel = "stylesheet";
  tag.href = themeCSS.FluerialUI;

  const floorpTag = document.createElement("link");
  floorpTag.setAttribute("id", "floorpdesign");
  floorpTag.rel = "stylesheet";
  floorpTag.href = "chrome://noraneko/skin/floorp/css/floorp.css";

  document.head?.appendChild(tag);
  document.head?.appendChild(floorpTag);
  await window.gURLBar._updateLayoutBreakoutDimensions();

  console.log("hi!");

  // } else {
  //   unloadStyleSheetWithNsStyleSheetService("chrome://browser/skin/lepton/leptonContent.css");
  //}
}
