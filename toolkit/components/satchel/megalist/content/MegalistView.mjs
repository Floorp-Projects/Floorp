/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { html } from "chrome://global/content/vendor/lit.all.mjs";
import { MozLitElement } from "chrome://global/content/lit-utils.mjs";

// eslint-disable-next-line import/no-unassigned-import
import "chrome://global/content/megalist/VirtualizedList.mjs";

// eslint-disable-next-line import/no-unassigned-import
import "chrome://global/content/megalist/search-input.mjs";

/**
 * Map with limit on how many entries it can have.
 * When over limit entries are added, oldest one are removed.
 */
class MostRecentMap {
  constructor(maxSize) {
    this.#maxSize = maxSize;
  }

  get(id) {
    const data = this.#map.get(id);
    if (data) {
      this.#keepAlive(id, data);
    }
    return data;
  }

  has(id) {
    this.#map.has(id);
  }

  set(id, data) {
    this.#keepAlive(id, data);
    this.#enforceLimits();
  }

  clear() {
    this.#map.clear();
  }

  #maxSize;
  #map = new Map();

  #keepAlive(id, data) {
    // Re-insert data to the map so it will be  less likely to be evicted
    this.#map.delete(id);
    this.#map.set(id, data);
  }

  #enforceLimits() {
    // Maps preserve order in which data was inserted,
    // we use that fact to remove oldest data from it.
    while (this.#map.size > this.#maxSize) {
      this.#map.delete(this.#map.keys().next().value);
    }
  }
}

/**
 * MegalistView presents data pushed to it by the MegalistViewModel and
 * notify MegalistViewModel of user commands.
 */
export class MegalistView extends MozLitElement {
  static keyToMessage = {
    ArrowUp: "SelectPreviousSnapshot",
    ArrowDown: "SelectNextSnapshot",
    PageUp: "SelectPreviousGroup",
    PageDown: "SelectNextGroup",
    Escape: "UpdateFilter",
  };
  static LINE_HEIGHT = 64;

  constructor() {
    super();
    this.selectedIndex = 0;
    this.searchText = "";

    window.addEventListener("MessageFromViewModel", ev =>
      this.#onMessageFromViewModel(ev)
    );
  }

  static get properties() {
    return {
      listLength: { type: Number },
      selectedIndex: { type: Number },
      searchText: { type: String },
    };
  }

  /**
   * View shows list of snapshots of lines stored in the View Model.
   * View Model provides the first snapshot id in the list and list length.
   * It's safe to combine firstSnapshotId+index to identify specific snapshot
   * in the list. When the list changes, View Model will provide a new
   * list with new first snapshot id (even if the content is the same).
   */
  #firstSnapshotId = 0;

  /**
   * Cache 120 most recently used lines.
   * View lives in child and View Model in parent processes.
   * By caching a few lines we reduce the need to send data between processes.
   * This improves performance in nearby scrolling scenarios.
   * 7680 is 8K vertical screen resolution.
   * Typical line is under 1/4KB long, making around 30KB cache requirement.
   */
  #snapshotById = new MostRecentMap(7680 / MegalistView.LINE_HEIGHT);

  #templates = {};

  connectedCallback() {
    super.connectedCallback();
    this.ownerDocument.addEventListener("keydown", e => this.#handleKeydown(e));
    for (const template of this.ownerDocument.getElementsByTagName(
      "template"
    )) {
      this.#templates[template.id] = template.content.firstElementChild;
    }
    this.#messageToViewModel("Refresh");
  }

  createLineElement(index) {
    if (index < 0 || index >= this.listLength) {
      return null;
    }

    const snapshotId = this.#firstSnapshotId + index;
    const lineElement = this.#templates.lineElement.cloneNode(true);
    lineElement.dataset.id = snapshotId;
    lineElement.addEventListener("dblclick", e => {
      this.#messageToViewModel("Command");
      e.preventDefault();
    });

    const data = this.#snapshotById.get(snapshotId);
    if (data !== "Loading") {
      if (data) {
        this.#applyData(snapshotId, data, lineElement);
      } else {
        // Put placeholder for this snapshot data to avoid requesting it again
        this.#snapshotById.set(snapshotId, "Loading");

        // Ask for snapshot data from the View Model.
        // Note: we could have optimized it further by asking for a range of
        //       indices because any scroll in virtualized list can only add
        //       a continuous range at the top or bottom of the visible area.
        //       However, this optimization is not necessary at the moment as
        //       we typically will request under a 100 of lines at a time.
        //       If we feel like making this improvement, we need to enhance
        //       VirtualizedList to request a range of new elements instead.
        this.#messageToViewModel("RequestSnapshot", { snapshotId });
      }
    }

    return lineElement;
  }

  /**
   * Find snapshot element on screen and populate it with data
   */
  receiveSnapshot({ snapshotId, snapshot }) {
    this.#snapshotById.set(snapshotId, snapshot);

    const lineElement = this.shadowRoot.querySelector(
      `.line[data-id="${snapshotId}"]`
    );
    if (lineElement) {
      this.#applyData(snapshotId, snapshot, lineElement);
    }
  }

  #applyData(snapshotId, snapshotData, lineElement) {
    let elementToFocus;
    const template =
      this.#templates[snapshotData.template] ?? this.#templates.lineTemplate;

    const lineContent = template.cloneNode(true);
    lineContent.querySelector(".label").textContent = snapshotData.label;

    const valueElement = lineContent.querySelector(".value");
    if (valueElement) {
      const valueText = lineContent.querySelector("span");
      if (valueText) {
        valueText.textContent = snapshotData.value;
      } else {
        const valueInput = lineContent.querySelector("input");
        if (valueInput) {
          valueInput.value = snapshotData.value;
          valueInput.addEventListener("keydown", e => {
            switch (e.code) {
              case "Enter":
                this.#messageToViewModel("Command", {
                  snapshotId,
                  commandId: "Save",
                  value: valueInput.value,
                });
                break;
              case "Escape":
                this.#messageToViewModel("Command", {
                  snapshotId,
                  commandId: "Cancel",
                });
                break;
              default:
                return;
            }
            e.preventDefault();
            e.stopPropagation();
          });
          valueInput.addEventListener("input", () => {
            // Update local cache so we don't override editing value
            // while user scrolls up or down a little.
            const snapshotDataInChild = this.#snapshotById.get(snapshotId);
            if (snapshotDataInChild) {
              snapshotDataInChild.value = valueInput.value;
            }
            this.#messageToViewModel("Command", {
              snapshotId,
              commandId: "EditInProgress",
              value: valueInput.value,
            });
          });
          elementToFocus = valueInput;
        } else {
          valueElement.textContent = snapshotData.value;
        }
      }

      if (snapshotData.valueIcon) {
        const valueIcon = valueElement.querySelector(".icon");
        if (valueIcon) {
          valueIcon.src = snapshotData.valueIcon;
        }
      }

      if (snapshotData.href) {
        const linkElement = this.ownerDocument.createElement("a");
        linkElement.className = valueElement.className;
        linkElement.href = snapshotData.href;
        linkElement.replaceChildren(...valueElement.children);
        valueElement.replaceWith(linkElement);
      }

      if (snapshotData.stickers?.length) {
        const stickersElement = lineContent.querySelector(".stickers");
        for (const sticker of snapshotData.stickers) {
          const stickerElement = this.ownerDocument.createElement("span");
          stickerElement.textContent = sticker.label;
          stickerElement.className = sticker.type;
          stickersElement.appendChild(stickerElement);
        }
      }
    }

    lineElement.querySelector(".content").replaceWith(lineContent);
    lineElement.classList.toggle("start", !!snapshotData.start);
    lineElement.classList.toggle("end", !!snapshotData.end);
    elementToFocus?.focus();
  }

  #messageToViewModel(messageName, data) {
    window.windowGlobalChild
      .getActor("Megalist")
      .sendAsyncMessage(messageName, data);
  }

  #onMessageFromViewModel({ detail }) {
    const functionName = `receive${detail.name}`;
    if (!(functionName in this)) {
      throw new Error(`Received unknown message "${detail.name}"`);
    }
    this[functionName](detail.data);
  }

  receiveUpdateSelection({ selectedIndex }) {
    this.selectedIndex = selectedIndex;
  }

  receiveShowSnapshots({ firstSnapshotId, count }) {
    this.#firstSnapshotId = firstSnapshotId;
    this.listLength = count;

    // Each new display list starts with the new first snapshot id
    // so we can forget previously known data.
    this.#snapshotById.clear();
    this.shadowRoot.querySelector("virtualized-list").requestRefresh();
    this.requestUpdate();
  }

  receiveMegalistUpdateFilter({ searchText }) {
    this.searchText = searchText;
    this.requestUpdate();
  }

  #handleInputChange(e) {
    const searchText = e.target.value;
    this.#messageToViewModel("UpdateFilter", { searchText });
  }

  #handleKeydown(e) {
    const message = MegalistView.keyToMessage[e.code];
    if (message) {
      this.#messageToViewModel(message);
      e.preventDefault();
    } else if (e.code == "Enter") {
      // Do not handle Enter at the virtualized list level when line menu is open
      if (
        this.shadowRoot.querySelector(
          ".line.selected > .menuButton > .menuPopup"
        )
      ) {
        return;
      }

      if (e.altKey) {
        // Execute default command1
        this.#messageToViewModel("Command");
      } else {
        // Show line level menu
        this.shadowRoot
          .querySelector(".line.selected > .menuButton > button")
          ?.click();
      }
      e.preventDefault();
    } else if (e.ctrlKey && e.key == "c" && !this.searchText.length) {
      this.#messageToViewModel("Command", { commandId: "Copy" });
      e.preventDefault();
    }
  }

  #handleClick(e) {
    const lineElement = e.composedTarget.closest(".line");
    if (!lineElement) {
      return;
    }

    const snapshotId = Number(lineElement.dataset.id);
    const snapshotData = this.#snapshotById.get(snapshotId);
    if (!snapshotData) {
      return;
    }

    this.#messageToViewModel("SelectSnapshot", { snapshotId });
    const menuButton = e.composedTarget.closest(".menuButton");
    if (menuButton) {
      this.#handleMenuButtonClick(menuButton, snapshotId, snapshotData);
    }

    e.preventDefault();
  }

  #handleMenuButtonClick(menuButton, snapshotId, snapshotData) {
    if (!snapshotData.commands?.length) {
      return;
    }

    const popup = this.ownerDocument.createElement("div");
    popup.className = "menuPopup";
    popup.addEventListener(
      "keydown",
      e => {
        function focusInternal(next, wrapSelector) {
          let element = e.composedTarget;
          do {
            element = element[next];
          } while (element && element.tagName != "BUTTON");

          // If we can't find next/prev button, focus the first/last one
          element ??=
            e.composedTarget.parentElement.querySelector(wrapSelector);
          element?.focus();
        }

        function focusNext() {
          focusInternal("nextElementSibling", "button");
        }

        function focusPrev() {
          focusInternal("previousElementSibling", "button:last-of-type");
        }

        switch (e.code) {
          case "Escape":
            popup.remove();
            break;
          case "Tab":
            if (e.shiftKey) {
              focusPrev();
            } else {
              focusNext();
            }
            break;
          case "ArrowUp":
            focusPrev();
            break;
          case "ArrowDown":
            focusNext();
            break;
          default:
            return;
        }

        e.preventDefault();
        e.stopPropagation();
      },
      { capture: true }
    );
    popup.addEventListener(
      "blur",
      e => {
        if (
          e.composedTarget?.closest(".menuPopup") !=
          e.relatedTarget?.closest(".menuPopup")
        ) {
          // TODO: this triggers on macOS before "click" event. Due to this,
          // we are not receiving the command.
          popup.remove();
        }
      },
      { capture: true }
    );

    for (const command of snapshotData.commands) {
      if (command == "-") {
        const separator = this.ownerDocument.createElement("div");
        separator.className = "separator";
        popup.appendChild(separator);
        continue;
      }

      const menuItem = this.ownerDocument.createElement("button");
      menuItem.textContent = command.label;
      menuItem.addEventListener("click", e => {
        this.#messageToViewModel("Command", {
          snapshotId,
          commandId: command.id,
        });
        popup.remove();
        e.preventDefault();
      });
      popup.appendChild(menuItem);
    }

    menuButton.querySelector("button").after(popup);
    popup.querySelector("button")?.focus();
  }

  render() {
    return html`
      <link
        rel="stylesheet"
        href="chrome://global/content/megalist/megalist.css"
      />
      <div class="container">
        <search-input
          .value=${this.searchText}
          .change=${e => this.#handleInputChange(e)}
        >
        </search-input>
        <virtualized-list
          .lineCount=${this.listLength}
          .lineHeight=${MegalistView.LINE_HEIGHT}
          .selectedIndex=${this.selectedIndex}
          .createLineElement=${index => this.createLineElement(index)}
          @click=${e => this.#handleClick(e)}
        >
        </virtualized-list>
      </div>
    `;
  }
}

customElements.define("megalist-view", MegalistView);
