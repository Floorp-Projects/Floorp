import { setBrowserDesign } from "./setBrowserDesign";

//console.log("hi!");
//@ts-expect-error ii
SessionStore.promiseInitialized.then(() => {
  setBrowserDesign();
});

//Services.obs.addObserver(setBrowserDesign, "browser-window-before-show");
