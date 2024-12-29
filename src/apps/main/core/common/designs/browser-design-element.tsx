/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { createEffect, createMemo, createSignal, For, Match, Switch } from "solid-js";
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

  const getCSS = createMemo(() => {
    return getCSSFromConfig(config())
  });

  createEffect(() => {
    const userjs = getCSS().userjs;
    if (userjs) applyUserJS(userjs);
  });

  let [devStyle,setDevStyle] = createSignal([] as string[]);

  if (import.meta.env.DEV) {
    createEffect(async ()=>{
      let arr = [];
      for (const link of getCSS().styles) {
        arr.push((await import(/* @vite-ignore */`${link}?raw`)).default)
      }
      setDevStyle(arr)
    })
  }

  return (
    <>
      <Switch>
        <Match when={import.meta.env.PROD}>
          <For each={getCSS().styles}>
            {(style) => (
              <link
              class="nora-designs"
              rel="stylesheet"
              href={ `chrome://noraneko${style}`}
            />
            )}
          </For>
        </Match>
        <Match when={import.meta.env.DEV}>
          <For each={devStyle()}>
            {(style)=>(
              <style>
                {style}
              </style>
            )}
          </For>
        </Match>
      </Switch>
      <style>{styleBrowser}</style>
    </>
  );
}
