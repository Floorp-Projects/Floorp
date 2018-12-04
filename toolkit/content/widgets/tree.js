/* This Source Code Form is subject to the terms of the Mozilla Public
  * License, v. 2.0. If a copy of the MPL was not distributed with this
  * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

  "use strict";

  // This is loaded into all XUL windows. Wrap in a block to prevent
  // leaking to window scope.
  {
  class MozTreeChildren extends MozElements.BaseControl {
    constructor() {
      super();

      /**
       * If there is no modifier key, we select on mousedown, not
       * click, so that drags work correctly.
       */
      this.addEventListener("mousedown", (event) => {
        if (this.parentNode.disabled)
          return;
        if (((!event.getModifierState("Accel") ||
              !this.parentNode.pageUpOrDownMovesSelection) &&
            !event.shiftKey && !event.metaKey) ||
          this.parentNode.view.selection.single) {
          var b = this.parentNode;
          var cell = b.getCellAt(event.clientX, event.clientY);
          var view = this.parentNode.view;

          // save off the last selected row
          this._lastSelectedRow = cell.row;

          if (cell.row == -1)
            return;

          if (cell.childElt == "twisty")
            return;

          if (cell.col && event.button == 0) {
            if (cell.col.cycler) {
              view.cycleCell(cell.row, cell.col);
              return;
            } else if (cell.col.type == window.TreeColumn.TYPE_CHECKBOX) {
              if (this.parentNode.editable && cell.col.editable &&
                view.isEditable(cell.row, cell.col)) {
                var value = view.getCellValue(cell.row, cell.col);
                value = value == "true" ? "false" : "true";
                view.setCellValue(cell.row, cell.col, value);
                return;
              }
            }
          }

          if (!view.selection.isSelected(cell.row)) {
            view.selection.select(cell.row);
            b.ensureRowIsVisible(cell.row);
          }
        }
      });

      /**
       * On a click (up+down on the same item), deselect everything
       * except this item.
       */
      this.addEventListener("click", (event) => {
        if (event.button != 0) { return; }
        if (this.parentNode.disabled)
          return;
        var b = this.parentNode;
        var cell = b.getCellAt(event.clientX, event.clientY);
        var view = this.parentNode.view;

        if (cell.row == -1)
          return;

        if (cell.childElt == "twisty") {
          if (view.selection.currentIndex >= 0 &&
            view.isContainerOpen(cell.row)) {
            var parentIndex = view.getParentIndex(view.selection.currentIndex);
            while (parentIndex >= 0 && parentIndex != cell.row)
              parentIndex = view.getParentIndex(parentIndex);
            if (parentIndex == cell.row) {
              var parentSelectable = true;
              if (parentSelectable)
                view.selection.select(parentIndex);
            }
          }
          this.parentNode.changeOpenState(cell.row);
          return;
        }

        if (!view.selection.single) {
          var augment = event.getModifierState("Accel");
          if (event.shiftKey) {
            view.selection.rangedSelect(-1, cell.row, augment);
            b.ensureRowIsVisible(cell.row);
            return;
          }
          if (augment) {
            view.selection.toggleSelect(cell.row);
            b.ensureRowIsVisible(cell.row);
            view.selection.currentIndex = cell.row;
            return;
          }
        }

        /* We want to deselect all the selected items except what was
          clicked, UNLESS it was a right-click.  We have to do this
          in click rather than mousedown so that you can drag a
          selected group of items */

        if (!cell.col) return;

        // if the last row has changed in between the time we
        // mousedown and the time we click, don't fire the select handler.
        // see bug #92366
        if (!cell.col.cycler && this._lastSelectedRow == cell.row &&
          cell.col.type != window.TreeColumn.TYPE_CHECKBOX) {
          view.selection.select(cell.row);
          b.ensureRowIsVisible(cell.row);
        }
      });

      /**
       * double-click
       */
      this.addEventListener("dblclick", (event) => {
        if (this.parentNode.disabled)
          return;
        var tree = this.parentNode;
        var view = this.parentNode.view;
        var row = view.selection.currentIndex;

        if (row == -1)
          return;

        var cell = tree.getCellAt(event.clientX, event.clientY);

        if (cell.childElt != "twisty") {
          this.parentNode.startEditing(row, cell.col);
        }

        if (this.parentNode._editingColumn || !view.isContainer(row))
          return;

        // Cyclers and twisties respond to single clicks, not double clicks
        if (cell.col && !cell.col.cycler && cell.childElt != "twisty")
          this.parentNode.changeOpenState(row);
      });

    }

    connectedCallback() {
      if (this.delayConnectedCallback()) {
        return;
      }

      this._lastSelectedRow = -1;

      if ("_ensureColumnOrder" in this.parentNode)
        this.parentNode._ensureColumnOrder();

    }
  }

  customElements.define("treechildren", MozTreeChildren);

  class MozTreecol extends MozElements.BaseControl {
    static get observedAttributes() {
      return [
        "label",
        "sortdirection",
        "hideheader",
        "crop",
      ];
    }

    get content() {
      return MozXULElement.parseXULToFragment(`
        <label class="treecol-text" flex="1" crop="right"></label>
        <image class="treecol-sortdirection"></image>
    `);
    }

    constructor() {
      super();

      this.addEventListener("mousedown", (event) => {
        if (event.button != 0) { return; }
        if (this.parentNode.parentNode.enableColumnDrag) {
          var xulns = "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul";
          var cols = this.parentNode.getElementsByTagNameNS(xulns, "treecol");

          // only start column drag operation if there are at least 2 visible columns
          var visible = 0;
          for (var i = 0; i < cols.length; ++i)
            if (cols[i].boxObject.width > 0) ++visible;

          if (visible > 1) {
            window.addEventListener("mousemove", this._onDragMouseMove, true);
            window.addEventListener("mouseup", this._onDragMouseUp, true);
            document.treecolDragging = this;
            this.mDragGesturing = true;
            this.mStartDragX = event.clientX;
            this.mStartDragY = event.clientY;
          }
        }
      });

      this.addEventListener("click", (event) => {
        if (event.button != 0) { return; }
        if (event.target != event.originalTarget)
          return;

        // On Windows multiple clicking on tree columns only cycles one time
        // every 2 clicks.
        if (/Win/.test(navigator.platform) && event.detail % 2 == 0)
          return;

        var tree = this.parentNode.parentNode;
        if (tree.columns) {
          tree.view.cycleHeader(tree.columns.getColumnFor(this));
        }
      });

    }

    markTreeDirty() {
      this.parentNode.parentNode._columnsDirty = true;
    }

    connectedCallback() {
      if (this.delayConnectedCallback()) {
        return;
      }
      if (!this.isRunningDelayedConnectedCallback) {
        this.markTreeDirty();
      }

      this.textContent = "";
      this.appendChild(this.content);

      this._updateAttributes();
    }

    attributeChangedCallback() {
      if (this.isConnectedAndReady) {
        this._updateAttributes();
      }
    }

    _updateAttributes() {
      let image = this.querySelector(".treecol-sortdirection");
      let label = this.querySelector(".treecol-text");

      this.inheritAttribute(image, "sortdirection");
      this.inheritAttribute(image, "hidden=hideheader");
      this.inheritAttribute(label, "value=label");

      // Don't remove the attribute on the child if it's los on the host.
      if (this.hasAttribute("crop")) {
        this.inheritAttribute(label, "crop");
      }
    }

    set ordinal(val) {
      this.setAttribute("ordinal", val);
      return val;
    }

    get ordinal() {
      var val = this.getAttribute("ordinal");
      if (val == "")
        return "1";

      return "" + (val == "0" ? 0 : parseInt(val));
    }

    get _previousVisibleColumn() {
      var sib = this.boxObject.previousSibling;
      while (sib) {
        if (sib.localName == "treecol" && sib.boxObject.width > 0 && sib.parentNode == this.parentNode)
          return sib;
        sib = sib.boxObject.previousSibling;
      }
      return null;
    }

    _onDragMouseMove(aEvent) {
      var col = document.treecolDragging;
      if (!col) return;

      // determine if we have moved the mouse far enough
      // to initiate a drag
      if (col.mDragGesturing) {
        if (Math.abs(aEvent.clientX - col.mStartDragX) < 5 &&
          Math.abs(aEvent.clientY - col.mStartDragY) < 5) {
          return;
        }
        col.mDragGesturing = false;
        col.setAttribute("dragging", "true");
        window.addEventListener("click", col._onDragMouseClick, true);
      }

      var pos = {};
      var targetCol = col.parentNode.parentNode._getColumnAtX(aEvent.clientX, 0.5, pos);

      // bail if we haven't mousemoved to a different column
      if (col.mTargetCol == targetCol && col.mTargetDir == pos.value)
        return;

      var tree = col.parentNode.parentNode;
      var sib;
      var column;
      if (col.mTargetCol) {
        // remove previous insertbefore/after attributes
        col.mTargetCol.removeAttribute("insertbefore");
        col.mTargetCol.removeAttribute("insertafter");
        column = tree.columns.getColumnFor(col.mTargetCol);
        tree.invalidateColumn(column);
        sib = col.mTargetCol._previousVisibleColumn;
        if (sib) {
          sib.removeAttribute("insertafter");
          column = tree.columns.getColumnFor(sib);
          tree.invalidateColumn(column);
        }
        col.mTargetCol = null;
        col.mTargetDir = null;
      }

      if (targetCol) {
        // set insertbefore/after attributes
        if (pos.value == "after") {
          targetCol.setAttribute("insertafter", "true");
        } else {
          targetCol.setAttribute("insertbefore", "true");
          sib = targetCol._previousVisibleColumn;
          if (sib) {
            sib.setAttribute("insertafter", "true");
            column = tree.columns.getColumnFor(sib);
            tree.invalidateColumn(column);
          }
        }
        column = tree.columns.getColumnFor(targetCol);
        tree.invalidateColumn(column);
        col.mTargetCol = targetCol;
        col.mTargetDir = pos.value;
      }
    }

    _onDragMouseUp(aEvent) {
      var col = document.treecolDragging;
      if (!col) return;

      if (!col.mDragGesturing) {
        if (col.mTargetCol) {
          // remove insertbefore/after attributes
          var before = col.mTargetCol.hasAttribute("insertbefore");
          col.mTargetCol.removeAttribute(before ? "insertbefore" : "insertafter");

          var sib = col.mTargetCol._previousVisibleColumn;
          if (before && sib) {
            sib.removeAttribute("insertafter");
          }

          // Move the column only if it will result in a different column
          // ordering
          var move = true;

          // If this is a before move and the previous visible column is
          // the same as the column we're moving, don't move
          if (before && col == sib) {
            move = false;
          } else if (!before && col == col.mTargetCol) {
            // If this is an after move and the column we're moving is
            // the same as the target column, don't move.
            move = false;
          }

          if (move) {
            col.parentNode.parentNode._reorderColumn(col, col.mTargetCol, before);
          }

          // repaint to remove lines
          col.parentNode.parentNode.invalidate();

          col.mTargetCol = null;
        }
      } else
        col.mDragGesturing = false;

      document.treecolDragging = null;
      col.removeAttribute("dragging");

      window.removeEventListener("mousemove", col._onDragMouseMove, true);
      window.removeEventListener("mouseup", col._onDragMouseUp, true);
      // we have to wait for the click event to fire before removing
      // cancelling handler
      var clickHandler = function(handler) {
        window.removeEventListener("click", handler, true);
      };
      window.setTimeout(clickHandler, 0, col._onDragMouseClick);
    }

    _onDragMouseClick(aEvent) {
      // prevent click event from firing after column drag and drop
      aEvent.stopPropagation();
      aEvent.preventDefault();
    }
  }

  customElements.define("treecol", MozTreecol);

  class MozTreecols extends MozElements.BaseControl {
    connectedCallback() {
      if (this.delayConnectedCallback()) {
        return;
      }

      if (!this.querySelector("treecolpicker")) {
        this.appendChild(MozXULElement.parseXULToFragment(`
          <treecolpicker class="treecol-image" fixed="true"></treecolpicker>
        `));
      }

      let treecolpicker = this.querySelector("treecolpicker");
      this.inheritAttribute(treecolpicker, "tooltiptext=pickertooltiptext");

      // Set resizeafter="farthest" on the splitters if nothing else has been
      // specified.
      Array.forEach(this.getElementsByTagName("splitter"), function(splitter) {
        if (!splitter.hasAttribute("resizeafter"))
          splitter.setAttribute("resizeafter", "farthest");
      });
    }
  }

  customElements.define("treecols", MozTreecols);

}
