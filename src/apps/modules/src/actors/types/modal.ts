/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

export type ModalOptions = {
  width?: number;
  height?: number;
  maxWidth?: number;
  maxHeight?: number;
};

export type ModalResponse = {
  success: boolean;
  data?: Record<string, unknown>;
  error?: string;
};