import * as production from "chrome://noraneko/content/assets/js/gestures.js";
const action=production["e"];
if(typeof action!=="function")throw new Error("production action API unavailable");
window.__qa97ExistingProductionAction=action;window.__qa97ActionApiReady=true;
