/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { modalSize } from "../data/data";

export function ModalBrowser() {
  return (
    <xul:browser
      id="modal-child-browser"
      type="content"
      remote="true"
      maychangeremoteness="true"
      messagemanagergroup="browsers"
      src={import.meta.env.DEV
        ? "http://localhost:5185/"
        : "chrome://noraneko-modal-child/content/index.html"}
      flex="1"
      style={{
        "width": modalSize().width ? `${modalSize().width}px` : "800px",
        "height": modalSize().height ? `${modalSize().height}px` : "600px",
        "max-width": modalSize().maxWidth
          ? `${modalSize().maxWidth}px`
          : "none",
        "max-height": modalSize().maxHeight
          ? `${modalSize().maxHeight}px`
          : "none",
      }}
    />
  );
}
