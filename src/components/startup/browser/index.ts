import initScripts from "@content/browser";
import { appendSectionBefore } from "./section";

["appcontent", "window-modal-dialog"].forEach((id) => {
  appendSectionBefore(id);
});

initScripts();
