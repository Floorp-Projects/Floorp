/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { MegalistViewModel } from "resource://gre/modules/megalist/MegalistViewModel.sys.mjs";

/**
 * MegalistParent integrates MegalistViewModel into Parent/Child model.
 */
export class MegalistParent extends JSWindowActorParent {
  #viewModel;

  actorCreated() {
    this.#viewModel = new MegalistViewModel((...args) =>
      this.sendAsyncMessage(...args)
    );
  }

  didDestroy() {
    this.#viewModel.willDestroy();
    this.#viewModel = null;
  }

  receiveMessage(message) {
    return this.#viewModel?.handleViewMessage(message);
  }
}
