/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { noraComponent, NoraComponentBase } from "@core/utils/base.ts";
import { ModalManager } from "./modalManager.ts";
import { ModalElement } from "./modalElement.tsx";
import type { ModalSize } from "./data/data.ts";

@noraComponent(import.meta.hot)
export default class ModalParent extends NoraComponentBase {
  private static instance: ModalParent;
  private modalManager: ModalManager;

  public static getInstance(): ModalParent {
    if (!ModalParent.instance) {
      ModalParent.instance = new ModalParent();
    }
    return ModalParent.instance;
  }

  constructor() {
    super();
    this.modalManager = new ModalManager();
  }

  init(): void {
    new ModalElement();
    globalThis.showNoraModal = this.show.bind(this);
    globalThis.hideNoraModal = this.hide.bind(this);
  }

  public show(): void {
    this.modalManager.show();
  }

  public hide(): void {
    this.modalManager.hide();
  }

  public setModalSize(newSize: ModalSize): void {
    this.modalManager.setModalSize(newSize);
  }
}
