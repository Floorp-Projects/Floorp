//@ts-ignore

const { AboutNewTab } = ChromeUtils.importESModule(
  "resource:///modules/AboutNewTab.sys.mjs",
);

(() => {
  const response = fetch("chrome://noraneko-newtab/content/index.html");
  response.then(() => {
    AboutNewTab.newTabURL = "chrome://noraneko-newtab/content/index.html";
  }).catch(() => {
    AboutNewTab.newTabURL = "http://localhost:5186/";
  });
})();
