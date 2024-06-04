/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { createSignal } from "solid-js";
import { Show } from "solid-js";
import shareModeStyle from "./share-mode.pcss?inline";

export const [shareModeEnabled, setShareModeEnabled] = createSignal(false);
export function ShareModeElement() {
  return (
    <>
      <xul:menuitem
        data-l10n-id="sharemode-menuitem"
        label="Toggle Share Mode"
        type="checkbox"
        id="toggle_sharemode"
        checked={shareModeEnabled()}
        onCommand={() => setShareModeEnabled((prev) => !prev)}
        accesskey="S"
      />

      <Show when={shareModeEnabled()}>
        <style>{shareModeStyle}</style>
      </Show>
    </>
  );
}
