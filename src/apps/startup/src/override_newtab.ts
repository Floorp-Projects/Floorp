//@ts-ignore

const { AboutNewTab } = ChromeUtils.importESModule(
  "resource:///modules/AboutNewTab.sys.mjs",
);
AboutNewTab.newTabURL = "chrome://noraneko-newtab/content/index.html";
