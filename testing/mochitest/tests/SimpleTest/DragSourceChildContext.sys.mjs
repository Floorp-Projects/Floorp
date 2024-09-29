/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { DragChildContextBase } from "./DragChildContextBase.sys.mjs";

export class DragSourceChildContext extends DragChildContextBase {
  // "editing-host" element (if any) at start of the drag
  editingHost = null;

  constructor(aDragWindow, aParams) {
    super("dragSource", aDragWindow, aParams);

    // Store current "editing-host" element so we can determine if the dragend
    // went to the right place.
    if (!this.dragElement.matches(":read-write")) {
      return;
    }
    let lastEditableElement = this.dragElement;
    for (
      let inclusiveAncestor = this.getInclusiveFlattenedTreeParentElement(
        this.dragElement
      );
      inclusiveAncestor;
      inclusiveAncestor = this.getInclusiveFlattenedTreeParentElement(
        inclusiveAncestor.flattenedTreeParentNode
      )
    ) {
      if (inclusiveAncestor.matches(":read-write")) {
        lastEditableElement = inclusiveAncestor;
        if (lastEditableElement == this.dragElement.ownerDocument.body) {
          break;
        }
      }
    }
    this.editingHost = lastEditableElement;
  }

  async checkMouseDown() {
    if (!this.expectNoDragEvents) {
      this.ok(!!this.events.mousedown[0], "mousedown existed");
      if (this.events.mousedown[0]) {
        this.ok(
          this.nodeIsFlattenedTreeDescendantOf(
            this.events.mousedown[0].composedTarget,
            this.dragElement
          ),
          "event target of mousedown is srcElement or a descendant"
        );
      }
    }
    await this.checkExpected();
  }

  async checkDragStart() {
    // Check that content still has drag session since it could have been
    // canceled (by the caller's dragstart).
    if (this.expectNoDragEvents || this.expectCancelDragStart) {
      this.info(`Drag was pre-existing: ${this.alreadyHadSession}`);
      this.ok(
        !this.dragService.getCurrentSession(this.dragWindow) ||
          this.alreadyHadSession,
        `no drag session in src process unless one was pre-existing`
      );
    } else {
      this.ok(
        !!this.dragService.getCurrentSession(this.dragWindow),
        `drag session was started in src process`
      );
      this.ok(!!this.events.dragstart[0], "dragstart existed");
      if (this.events.dragstart[0]) {
        this.ok(
          this.nodeIsFlattenedTreeDescendantOf(
            this.events.dragstart[0].composedTarget,
            this.dragElement
          ),
          "event target of dragstart is srcElement or a descendant"
        );
      }
      await this.checkExpected();
    }
  }

  async checkDragEnd() {
    if (!this.expectNoDragEvents) {
      this.ok(!!this.events.dragend[0], "dragend existed");
      if (this.events.dragend[0]) {
        this.ok(
          this.nodeIsFlattenedTreeDescendantOf(
            this.events.dragend[0].composedTarget,
            this.dragElement
          ),
          "event target of dragend is srcElement or a descendant"
        );
        if (this.editingHost) {
          this.ok(
            this.events.dragend[0].composedTarget == this.editingHost,
            `event target of dragend was editingHost`
          );
        }
      }
    }
    await this.checkExpected();
  }
}

export function createDragSourceChildContext(
  aDragWindow,
  aParams,
  aOk,
  aIs,
  aInfo
) {
  aDragWindow.dragSource = new DragSourceChildContext(aDragWindow, {
    ...aParams,
    ok: aOk,
    is: aIs,
    info: aInfo,
  });
}
