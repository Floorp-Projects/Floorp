/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

export function VerticalTabbarSplitter() {
  return (
    <xul:splitter
      id="verticaltab-splitter"
      class="chromeclass-extrachrome sidebar-splitter"
      resizebefore="sibling"
      resizeafter="none"
      hidden={true}
    />
  );
}
