import { render } from "@nora/solid-xul";
import { NoraSettingsLink } from "./nora-settings-link";

document?.addEventListener("DOMContentLoaded", () => {
  const nav_root = document.querySelector("#categories");
  render(NoraSettingsLink, nav_root);
});
