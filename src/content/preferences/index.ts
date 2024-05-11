import { render } from "@solid-xul/solid-xul";
import { menu } from "./menuButton";

export function initContentScripts() {
  render(menu, document.getElementById("categories"));
}
