/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { createEffect, For, Show } from "solid-js";
import type { z } from "zod";
import type { zFloorpDesignConfigs } from "../../../../../apps/common/scripts/global-types/type";
import { applyUserJS } from "./userjs-parser";
import { config } from "./configs";
import leptonChromeStyles from "@nora/skin/lepton/css/leptonChrome.css?url";
import leptonTabStyles from "@nora/skin/lepton/css/leptonContent.css?url";
import leptonUserJs from "@nora/skin/lepton/userjs/lepton.js?raw";
import photonUserJs from "@nora/skin/lepton/userjs/photon.js?raw";
import protonfixUserJs from "@nora/skin/lepton/userjs/protonfix.js?raw";
import fluerialStyles from "@nora/skin/fluerial/css/fluerial.css?url";
import styleBrowser from "./browser.css?inline";

interface FCSS {
  styles: string[] | null;
  userjs: string | null;
}

function getCSSFromConfig(pref: z.infer<typeof zFloorpDesignConfigs>): FCSS {
  switch (pref.globalConfigs.userInterface) {
    case "fluerial": {
      return { styles: [fluerialStyles], userjs: null };
    }
    case "lepton": {
      return {
        styles: [leptonChromeStyles, leptonTabStyles],
        userjs: leptonUserJs,
      };
    }
    case "photon": {
      return {
        styles: [leptonChromeStyles, leptonTabStyles],
        userjs: photonUserJs,
      };
    }
    case "protonfix": {
      return {
        styles: [leptonChromeStyles, leptonTabStyles],
        userjs: protonfixUserJs,
      };
    }
    case "proton": {
      return {
        styles: null,
        userjs: null,
      };
    }
    default: {
      pref.globalConfigs.userInterface satisfies never;
      return {
        styles:null,
        userjs:null
      }
    }
  }
}

export function BrowserDesignElement() {
  [100, 500].forEach((time) => {
    setTimeout(() => {
      window.gURLBar.updateLayoutBreakout();
    }, time);
  });

  const getCSS = () => getCSSFromConfig(config());

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
