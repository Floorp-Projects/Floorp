/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { z } from "zod";
import { type GestureActionRegistration } from "../mouse-gesture/utils/gestures.ts";

export const zModifiers = z.object({
  alt: z.boolean().default(false),
  ctrl: z.boolean().default(false),
  meta: z.boolean().default(false),
  shift: z.boolean().default(false),
});

export const zShortcutConfig = z.object({
  modifiers: zModifiers,
  key: z.string(),
  action: z.string(),
});

export const zKeyboardShortcutConfig = z.object({
  enabled: z.boolean().default(true),
  shortcuts: z.record(z.string(), zShortcutConfig).default({}),
});

export type Modifiers = z.infer<typeof zModifiers>;
export type ShortcutConfig = z.infer<typeof zShortcutConfig>;
export type KeyboardShortcutConfig = z.infer<typeof zKeyboardShortcutConfig>;
export type { GestureActionRegistration };
