//@ts-ignore

const { AboutNewTab } = ChromeUtils.importESModule(
  "resource:///modules/AboutNewTab.sys.mjs",
);

if (Services.prefs.getBoolPref("floorp.isnewtab.floorpstart", true)) {
  (() => {
    const response = fetch("chrome://noraneko-newtab/content/index.html");
    response.then(() => {
      AboutNewTab.newTabURL = "chrome://noraneko-newtab/content/index.html";
    }).catch(() => {
      AboutNewTab.newTabURL = "http://localhost:5186/";
    });
  })();
}
