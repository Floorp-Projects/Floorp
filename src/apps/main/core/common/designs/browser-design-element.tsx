/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import {
  createEffect,
  createMemo,
  createSignal,
  For,
  Match,
  Switch,
} from "solid-js";
import { applyUserJS } from "./utils/userjs-parser.ts";
import styleBrowser from "./browser.css?inline";
import { config } from "./configs.ts";
import { getCSSFromConfig } from "./utils/css.ts";

export function BrowserDesignElement() {
  [100, 500].forEach((time) => {
    setTimeout(() => {
      window.gURLBar.updateLayoutBreakout();
    }, time);
  });

  const getCSS = createMemo(() => {
    return getCSSFromConfig(config());
  });

  createEffect(() => {
    const userjs = getCSS().userjs;
    if (userjs) applyUserJS(userjs);
  });

  const [devStyle, setDevStyle] = createSignal([] as string[]);

  if (import.meta.env.DEV) {
    createEffect(async () => {
      const arr: string[] = [];
      const styles = getCSS().styles;
      if (!styles.length) return;

      const rawPath = styles[0].replace("css/leptonChrome.css", "icons");
      const iconsDirPath = rawPath.startsWith("/@fs/")
        ? `file://${rawPath.slice(4)}`
        : `/src/${rawPath}`;
      console.log(iconsDirPath);
      for (const link of styles) {
        arr.push(
          (await import(/* @vite-ignore */ `${link}?raw`)).default.replaceAll(
            "../icons",
            iconsDirPath,
          ),
        );
      }

      setDevStyle(arr);
    });
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
                href={`${style}`}
              />
            )}
          </For>
        </Match>
        <Match when={import.meta.env.DEV}>
          <For each={devStyle()}>
            {(style) => (
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
