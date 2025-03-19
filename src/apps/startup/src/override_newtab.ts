//@ts-ignore

const { AboutNewTab } = ChromeUtils.importESModule(
  "resource:///modules/AboutNewTab.sys.mjs",
);
AboutNewTab.newTabURL = import.meta.env.MODE === "dev"
  ? "http://localhost:5186/"
  : "chrome://noraneko-newtab/content/index.html";
