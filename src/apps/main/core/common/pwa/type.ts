/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { z } from "zod";

export type Manifest = {
  id: string;
  name: string;
  short_name?: string;
  start_url: string;
  icon: string;
  scope?: string;
};

export type Icon = {
  base64?: string;
  iconURL?: string;
  type?: string;
  src: string;
  purpose?: string[];
  sizes: string[];
};

export type IconData = {
  iconURL: string;
};

export type Browser = {
  currentURI: nsIURI;
  manifest: Manifest;
  contentTitle: string;
  icons: Icon[];
  browsingContext: Window;
  scope: string;
};

export type LegacyPWAEntry = {
  id: string;
  name: string;
  startURI: string;
  manifest: {
    dir: string;
    start_url: string;
    display: string;
    name: string;
    short_name: string;
    theme_color: string;
    background_color: string;
    scope: string;
    id: string;
    icons: {
      src: { src: string; sizes: string[] }[];
    };
  };
  scope: string;
  config: {
    needsUpdate: boolean;
    persisted: boolean;
  };
};

export const zPwaConfig = z.object({
  showToolbar: z.boolean(),
});

export type TPwaConfig = z.infer<typeof zPwaConfig>;
