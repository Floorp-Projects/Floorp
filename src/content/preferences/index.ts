import { insert, render } from "@solid-xul/solid-xul";
import { csk } from "./csk";
import { category } from "./csk/category";
import { initHashChange } from "./hashchange";

export default function initScripts() {
  const init = () => {
    insert(
      document.querySelector("#categories"),
      category(),
      //document.getElementById("category-Downloads"),
      document.getElementById("category-more-from-mozilla"),
    );
    render(csk, document.querySelector(".pane-container"));
    initHashChange();
    switch (location.hash) {
      case "#csk": {
        document.getElementById("category-csk")?.click();
        break;
      }
    }
  };

  if (document.readyState !== "loading") {
    init();
  } else {
    document.addEventListener("DOMContentLoaded", init);
  }
}

// const gCSKPane = {
//   init() {

//   },
// };
// window.gCSKPane = gCSKPane;
