import { initContentScripts } from "@content/index.ts";
import { appendSectionBefore } from "./section";

["appcontent", "window-modal-dialog"].forEach((id) => {
  appendSectionBefore(id);
});

initContentScripts();
