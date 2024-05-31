/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Virtualized List can efficiently show billions of lines provided
 * that all of them have the same height.
 *
 * Caller is responsible for setting createLineElement(index) function to
 * create elements as they are scrolled into the view.
 */
class VirtualizedList extends HTMLElement {
  lineHeight = 64;
  #lineCount = 0;
  #oldScrollTop = 0;

  get lineCount() {
    return this.#lineCount;
  }

  set lineCount(value) {
    this.#lineCount = value;
    this.#rebuildVisibleLines();
  }

  #selectedIndex = 0;

  get selectedIndex() {
    return this.#selectedIndex;
  }

  set selectedIndex(value) {
    this.#selectedIndex = value;
    if (this.#container) {
      this.updateLineSelection(true);
    }
  }

  #linesContainerWrapper;
  #container;

  connectedCallback() {
    this.#linesContainerWrapper = this.ownerDocument.createElement("div");
    this.#linesContainerWrapper.role = "list";
    this.#container = this.ownerDocument.createElement("span");
    this.#container.classList.add("lines-container");
    this.#container.role = "none";
    this.#linesContainerWrapper.appendChild(this.#container);
    this.appendChild(this.#linesContainerWrapper);

    this.#rebuildVisibleLines();
    this.addEventListener("scroll", () => this.#rebuildVisibleLines());
  }

  requestRefresh() {
    this.#container.replaceChildren();
    this.#rebuildVisibleLines();
  }

  updateLineSelection(scrollIntoView) {
    const lineElements = this.#container.querySelectorAll(".line");
    let selectedElement;

    for (let lineElement of lineElements) {
      let isSelected = Number(lineElement.dataset.index) === this.selectedIndex;
      if (isSelected) {
        selectedElement = lineElement;
      }
      lineElement.classList.toggle("selected", isSelected);
    }

    if (scrollIntoView) {
      if (selectedElement) {
        selectedElement.scrollIntoView({ block: "nearest" });
      } else {
        let selectedTop = this.selectedIndex * this.lineHeight;
        if (this.scrollTop > selectedTop) {
          this.scrollTop = selectedTop;
        } else {
          this.scrollTop = selectedTop - this.clientHeight + this.lineHeight;
        }
      }
    }
  }

  #rebuildVisibleLines() {
    if (!this.isConnected || !this.createLineElement) {
      return;
    }

    this.#linesContainerWrapper.style.height = `${
      this.lineHeight * this.lineCount
    }px`;

    let firstLineIndex = Math.floor(this.scrollTop / this.lineHeight);
    let visibleLineCount = Math.ceil(this.clientHeight / this.lineHeight);
    let lastLineIndex = firstLineIndex + visibleLineCount;
    let extraLines = Math.ceil(visibleLineCount / 2); // They are present in DOM, but not visible

    firstLineIndex = Math.max(0, firstLineIndex - extraLines);
    lastLineIndex = Math.min(this.lineCount, lastLineIndex + extraLines);

    let previousChild = null;
    let visibleLines = new Map();

    for (let child of Array.from(this.#container.children)) {
      let index = Number(child.dataset.index);
      if (index < firstLineIndex || index > lastLineIndex) {
        child.remove();
      } else {
        visibleLines.set(index, child);
      }
    }

    for (let index = firstLineIndex; index <= lastLineIndex; index++) {
      let child = visibleLines.get(index);
      if (!child) {
        child = this.createLineElement(index);

        if (!child) {
          // Friday fix :-)
          //todo: figure out what was on that Friday and how can we fix it
          continue;
        }

        child.role = "listitem";
        child.style.top = `${index * this.lineHeight}px`;
        child.dataset.index = index;

        if (previousChild) {
          previousChild.after(child);
        } else if (this.#container.firstElementChild?.offsetTop > top) {
          this.#container.firstElementChild.before(child);
        } else if (
          this.#container.firstElementChild &&
          this.#oldScrollTop > this.scrollTop
        ) {
          this.#container.firstElementChild.before(child);
        } else {
          this.#container.appendChild(child);
        }
      }
      previousChild = child;
    }

    this.#oldScrollTop = this.scrollTop;
    this.updateLineSelection(false);
  }
}

customElements.define("virtualized-list", VirtualizedList);
