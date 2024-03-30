import { applyUserJS } from "./applyUserJS";
import { loadStyleSheetWithNsStyleSheetService } from "./stylesheetService";

export async function setBrowserDesign() {
  console.log("setBrowserDesign");
  //remove #browserdesign
  document.getElementById("browserdesign")?.remove();

  //const floorpInterfaceNum = Services.prefs.getIntPref("floorp.browser.user.interface");
  const updateNumber = new Date().getTime();
  const themeCSS = {
    LeptonUI: "chrome://noraneko/skin/lepton/css/leptonChrome.css",
    // fluentUI: `@import url(chrome://browser/skin/fluentUI/fluentUI.css);`,
    // gnomeUI: `@import url(chrome://browser/skin/gnomeUI/gnomeUI.css);`,
    FluerialUI: "chrome://noraneko/skin/floorp-fluerial/css/fluerial.css",
    // FluerialUIMultitab: `@import url(chrome://browser/skin/floorplegacy/test_legacy.css?${updateNumber}); @import url(chrome://browser/skin/floorplegacy/test_legacy_multitab.css);`,

    // // Vertical Tabs CSS Injection
    // LeptonVerticalTabs: `@import url(chrome://browser/skin/lepton/leptonVerticalTabs.css);`,
    // fluentVerticalTabs: `@import url(chrome://browser/skin/fluentUI/fluentVerticalTabs.css);`,
    // gnomeVerticalTabs: `@import url(chrome://browser/skin/gnomeUI/gnomeVerticalTabs.css);`,
    // FluerialVerticalTabs: `@import url(chrome://browser/skin/floorplegacy/test_legacy_verticalTabs.css?${updateNumber});`,
  };

  const tag = document.createElement("link");
  tag.setAttribute("id", "browserdesign");

  tag.rel = "stylesheet";
  tag.href = themeCSS.FluerialUI;
  //const enableMultitab = Services.prefs.getIntPref("floorp.tabbar.style") == 1;
  //const enableVerticalTabs = Services.prefs.getIntPref("floorp.browser.tabbar.settings") == 2;

  // switch (floorpInterfaceNum) {
  //   case 1:
  //     break;
  //   case 3:
  //     tag.innerText = enableVerticalTabs ? themeCSS.LeptonUI + themeCSS.LeptonVerticalTabs : themeCSS.LeptonUI;
  //     break;
  //   case 5:
  //     if (AppConstants.platform !== "linux") {
  //       tag.innerText = enableVerticalTabs ? themeCSS.fluentUI + themeCSS.fluentVerticalTabs : themeCSS.fluentUI;
  //     }
  //     break;
  //   case 6:
  //     if (AppConstants.platform == "linux") {
  //       tag.innerText = enableVerticalTabs ? themeCSS.gnomeUI + themeCSS.gnomeVerticalTabs : themeCSS.gnomeUI;
  //     }
  //     break;
  //   case 8:
  //     tag.innerText = enableMultitab ? themeCSS.FluerialUIMultitab : enableVerticalTabs ? themeCSS.FluerialUI + themeCSS.FluerialVerticalTabs : themeCSS.FluerialUI;
  //     break;
  // }

  document.head.appendChild(tag);

  // recalculate sidebar width
  //setTimeout(() => {

  //@ts-expect-error gURLBar is Firefox-specific

  await window.gURLBar._updateLayoutBreakoutDimensions();
  //}, 100);

  // setTimeout(() => {
  //   gURLBar._updateLayoutBreakoutDimensions();
  // }, 500);

  //setTimeout(() => {
  //setMultirowTabMaxHeight();
  //}, 1000);

  //if (floorpInterfaceNum == 3) {

  //?
  // loadStyleSheetWithNsStyleSheetService(
  //   "chrome://noraneko/skin/lepton/css/leptonContent.css",
  // );
  //await applyUserJS();

  console.log("hi!");

  // } else {
  //   unloadStyleSheetWithNsStyleSheetService("chrome://browser/skin/lepton/leptonContent.css");
  //}
}
