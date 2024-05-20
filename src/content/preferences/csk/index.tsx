import type {} from "solid-styled-jsx";
import { CustomShortcutKeyPage } from "./page";

import { showCSK } from "../hashchange";
import { initSetKey } from "./setkey";
import { Show } from "solid-js";

export function csk() {
  initSetKey();
  return (
    <section id="nora-csk-entry">
      <Show when={showCSK()}>
        <CustomShortcutKeyPage />
      </Show>
    </section>
  );
}
