/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { createEffect, createMemo, For, Show } from "solid-js";
import { applyUserJS } from "./utils/userjs-parser";
import styleBrowser from "./browser.css?inline";
import { config } from "./configs";
import { getCSSFromConfig } from "./utils/css";



export function BrowserDesignElement() {
  [100, 500].forEach((time) => {
    setTimeout(() => {
      window.gURLBar.updateLayoutBreakout();
    }, time);
  });

  const getCSS = createMemo(() => getCSSFromConfig(config()));

  createEffect(() => {
    const userjs = getCSS().userjs;
    if (userjs) applyUserJS(userjs);
  });

  return (
    <>
      <Show when={getCSS()}>
        <For each={getCSS().styles}>
          {(style) => (
            <link
              class="nora-designs"
              rel="stylesheet"
              href={`chrome://noraneko${style}`}
            />
          )}
        </For>
      </Show>
      <style>{styleBrowser}</style>
    </>
  );
}
