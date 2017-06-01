/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/*
 * The spinner is responsible for displaying the items, and does
 * not care what the values represent. The setValue function is called
 * when it detects a change in value triggered by scroll event.
 * Supports scrolling, clicking on up or down, clicking on item, and
 * dragging.
 */

function Spinner(props, context) {
  this.context = context;
  this._init(props);
}

{

  const ITEM_HEIGHT = 2.5,
        VIEWPORT_SIZE = 7,
        VIEWPORT_COUNT = 5,
        SCROLL_TIMEOUT = 100;

  Spinner.prototype = {
    /**
     * Initializes a spinner. Set the default states and properties, cache
     * element references, create the HTML markup, and add event listeners.
     *
     * @param  {Object} props [Properties passed in from parent]
     *         {
     *           {Function} setValue: Takes a value and set the state to
     *             the parent component.
     *           {Function} getDisplayString: Takes a value, and output it
     *             as localized strings.
     *           {Number} viewportSize [optional]: Number of items in a
     *             viewport.
     *           {Boolean} hideButtons [optional]: Hide up & down buttons
     *           {Number} rootFontSize [optional]: Used to support zoom in/out
     *         }
     */
    _init(props) {
      const { id, setValue, getDisplayString, hideButtons, rootFontSize = 10 } = props;

      const spinnerTemplate = document.getElementById("spinner-template");
      const spinnerElement = document.importNode(spinnerTemplate.content, true);

      // Make sure viewportSize is an odd number because we want to have the selected
      // item in the center. If it's an even number, use the default size instead.
      const viewportSize = props.viewportSize % 2 ? props.viewportSize : VIEWPORT_SIZE;

      this.state = {
        items: [],
        isScrolling: false
      };
      this.props = {
        setValue, getDisplayString, viewportSize, rootFontSize,
        // We can assume that the viewportSize is an odd number. Calculate how many
        // items we need to insert on top of the spinner so that the selected is at
        // the center. Ex: if viewportSize is 5, we need 2 items on top.
        viewportTopOffset: (viewportSize - 1) / 2
      };
      this.elements = {
        container: spinnerElement.querySelector(".spinner-container"),
        spinner: spinnerElement.querySelector(".spinner"),
        up: spinnerElement.querySelector(".up"),
        down: spinnerElement.querySelector(".down"),
        itemsViewElements: []
      };

      this.elements.spinner.style.height = (ITEM_HEIGHT * viewportSize) + "rem";

      if (id) {
        this.elements.container.id = id;
      }
      if (hideButtons) {
        this.elements.container.classList.add("hide-buttons");
      }

      this.context.appendChild(spinnerElement);
      this._attachEventListeners();
    },

    /**
     * Only the parent component calls setState on the spinner.
     * It checks if the items have changed and updates the spinner.
     * If only the value has changed, smooth scrolls to the new value.
     *
     * @param {Object} newState [The new spinner state]
     *        {
     *          {Number/String} value: The centered value
     *          {Array} items: The list of items for display
     *          {Boolean} isInfiniteScroll: Whether or not the spinner should
     *            have infinite scroll capability
     *          {Boolean} isValueSet: true if user has selected a value
     *        }
     */
    setState(newState) {
      const { value, items } = this.state;
      const { value: newValue, items: newItems, isValueSet, isInvalid, smoothScroll = true } = newState;

      if (this._isArrayDiff(newItems, items)) {
        this.state = Object.assign(this.state, newState);
        this._updateItems();
        this._scrollTo(newValue, true);
      } else if (newValue != value) {
        this.state = Object.assign(this.state, newState);
        if (smoothScroll) {
          this._smoothScrollTo(newValue, true);
        } else {
          this._scrollTo(newValue, true);
        }
      }

      if (isValueSet && !isInvalid) {
        this._updateSelection();
      } else {
        this._removeSelection();
      }
    },

    /**
     * Whenever scroll event is detected:
     * - Update the index state
     * - If a smooth scroll has reached its destination, set [isScrolling] state
     *   to false
     * - If the value has changed, update the [value] state and call [setValue]
     * - If infinite scrolling is on, reset the scrolling position if necessary
     */
    _onScroll() {
      const { items, itemsView, isInfiniteScroll } = this.state;
      const { viewportSize, viewportTopOffset } = this.props;
      const { spinner } = this.elements;

      this.state.index = this._getIndexByOffset(spinner.scrollTop);

      const value = itemsView[this.state.index + viewportTopOffset].value;

      // Check if smooth scrolling has reached its destination.
      // This prevents input box jump when input box changes values.
      if (this.state.value == value && this.state.isScrolling) {
        this.state.isScrolling = false;
      }

      // Call setValue if value has changed, and is not smooth scrolling
      if (this.state.value != value && !this.state.isScrolling) {
        this.state.value = value;
        this.props.setValue(value);
      }

      // Do infinite scroll when items length is bigger or equal to viewport
      // and isInfiniteScroll is not false.
      if (items.length >= viewportSize && isInfiniteScroll) {
        // If the scroll position is near the top or bottom, jump back to the middle
        // so user can keep scrolling up or down.
        if (this.state.index < viewportSize ||
            this.state.index > itemsView.length - viewportSize) {
          this._scrollTo(this.state.value, true);
        }
      }

      // Use a timer to detect if a scroll event has not fired within some time
      // (defined in SCROLL_TIMEOUT). This is required because we need to hide
      // highlight and hover state when user is scrolling.
      clearTimeout(this.state.scrollTimer);
      this.elements.spinner.classList.add("scrolling");
      this.state.scrollTimer = setTimeout(() => {
        this.elements.spinner.classList.remove("scrolling");
        this.elements.spinner.dispatchEvent(new CustomEvent("ScrollStop"));
      }, SCROLL_TIMEOUT);
    },

    /**
     * Updates the spinner items to the current states.
     */
    _updateItems() {
      const { viewportSize, viewportTopOffset } = this.props;
      const { items, isInfiniteScroll } = this.state;

      // Prepends null elements so the selected value is centered in spinner
      let itemsView = new Array(viewportTopOffset).fill({}).concat(items);

      if (items.length >= viewportSize && isInfiniteScroll) {
        // To achieve infinite scroll, we move the scroll position back to the
        // center when it is near the top or bottom. The scroll momentum could
        // be lost in the process, so to minimize that, we need at least 2 sets
        // of items to act as buffer: one for the top and one for the bottom.
        // But if the number of items is small ( < viewportSize * viewport count)
        // we should add more sets.
        let count = Math.ceil(viewportSize * VIEWPORT_COUNT / items.length) * 2;
        for (let i = 0; i < count; i += 1) {
          itemsView.push(...items);
        }
      }

      // Reuse existing DOM nodes when possible. Create or remove
      // nodes based on how big itemsView is.
      this._prepareNodes(itemsView.length, this.elements.spinner);
      // Once DOM nodes are ready, set display strings using textContent
      this._setDisplayStringAndClass(itemsView, this.elements.itemsViewElements);

      this.state.itemsView = itemsView;
    },

    /**
     * Make sure the number or child elements is the same as length
     * and keep the elements' references for updating textContent
     *
     * @param {Number} length [The number of child elements]
     * @param {DOMElement} parent [The parent element reference]
     */
    _prepareNodes(length, parent) {
      const diff = length - parent.childElementCount;

      if (!diff) {
        return;
      }

      if (diff > 0) {
        // Add more elements if length is greater than current
        let frag = document.createDocumentFragment();

        // Remove margin bottom on the last element before appending
        if (parent.lastChild) {
          parent.lastChild.style.marginBottom = "";
        }

        for (let i = 0; i < diff; i++) {
          let el = document.createElement("div");
          frag.appendChild(el);
          this.elements.itemsViewElements.push(el);
        }
        parent.appendChild(frag);
      } else if (diff < 0) {
        // Remove elements if length is less than current
        for (let i = 0; i < Math.abs(diff); i++) {
          parent.removeChild(parent.lastChild);
        }
        this.elements.itemsViewElements.splice(diff);
      }

      parent.lastChild.style.marginBottom =
        (ITEM_HEIGHT * this.props.viewportTopOffset) + "rem";
    },

    /**
     * Set the display string and class name to the elements.
     *
     * @param {Array<Object>} items
     *        [{
     *          {Number/String} value: The value in its original form
     *          {Boolean} enabled: Whether or not the item is enabled
     *        }]
     * @param {Array<DOMElement>} elements
     */
    _setDisplayStringAndClass(items, elements) {
      const { getDisplayString } = this.props;

      items.forEach((item, index) => {
        elements[index].textContent =
          item.value != undefined ? getDisplayString(item.value) : "";
        elements[index].className = item.enabled ? "" : "disabled";
      });
    },

    /**
     * Attach event listeners to the spinner and buttons.
     */
    _attachEventListeners() {
      const { spinner, container } = this.elements;

      spinner.addEventListener("scroll", this, { passive: true });
      container.addEventListener("mouseup", this, { passive: true });
      container.addEventListener("mousedown", this, { passive: true });
    },

    /**
     * Handle events
     * @param  {DOMEvent} event
     */
    handleEvent(event) {
      const { mouseState = {}, index, itemsView } = this.state;
      const { viewportTopOffset, setValue } = this.props;
      const { spinner, up, down } = this.elements;

      switch (event.type) {
        case "scroll": {
          this._onScroll();
          break;
        }
        case "mousedown": {
          this.state.mouseState = {
            down: true,
            layerX: event.layerX,
            layerY: event.layerY
          };
          if (event.target == up) {
            // An "active" class is needed to simulate :active pseudo-class
            // because element is not focused.
            event.target.classList.add("active");
            this._smoothScrollToIndex(index - 1);
          }
          if (event.target == down) {
            event.target.classList.add("active");
            this._smoothScrollToIndex(index + 1);
          }
          if (event.target.parentNode == spinner) {
            // Listen to dragging events
            spinner.addEventListener("mousemove", this, { passive: true });
            spinner.addEventListener("mouseleave", this, { passive: true });
          }
          break;
        }
        case "mouseup": {
          this.state.mouseState.down = false;
          if (event.target == up || event.target == down) {
            event.target.classList.remove("active");
          }
          if (event.target.parentNode == spinner) {
            // Check if user clicks or drags, scroll to the item if clicked,
            // otherwise get the current index and smooth scroll there.
            if (event.layerX == mouseState.layerX && event.layerY == mouseState.layerY) {
              const newIndex = this._getIndexByOffset(event.target.offsetTop) - viewportTopOffset;
              if (index == newIndex) {
                // Set value manually if the clicked element is already centered.
                // This happens when the picker first opens, and user pick the
                // default value.
                setValue(itemsView[index + viewportTopOffset].value);
              } else {
                this._smoothScrollToIndex(newIndex);
              }
            } else {
              this._smoothScrollToIndex(this._getIndexByOffset(spinner.scrollTop));
            }
            // Stop listening to dragging
            spinner.removeEventListener("mousemove", this, { passive: true });
            spinner.removeEventListener("mouseleave", this, { passive: true });
          }
          break;
        }
        case "mouseleave": {
          if (event.target == spinner) {
            // Stop listening to drag event if mouse is out of the spinner
            this._smoothScrollToIndex(this._getIndexByOffset(spinner.scrollTop));
            spinner.removeEventListener("mousemove", this, { passive: true });
            spinner.removeEventListener("mouseleave", this, { passive: true });
          }
          break;
        }
        case "mousemove": {
          // Change spinner position on drag
          spinner.scrollTop -= event.movementY;
          break;
        }
      }
    },

    /**
     * Find the index by offset
     * @param {Number} offset: Offset value in pixel.
     * @return {Number}  Index number
     */
    _getIndexByOffset(offset) {
      return Math.round(offset / (ITEM_HEIGHT * this.props.rootFontSize));
    },

    /**
     * Find the index of a value that is the closest to the current position.
     * If centering is true, find the index closest to the center.
     *
     * @param {Number/String} value: The value to find
     * @param {Boolean} centering: Whether or not to find the value closest to center
     * @return {Number} index of the value, returns -1 if value is not found
     */
    _getScrollIndex(value, centering) {
      const { itemsView } = this.state;
      const { viewportTopOffset } = this.props;

      // If index doesn't exist, or centering is true, start from the middle point
      let currentIndex = centering || (this.state.index == undefined) ?
                         Math.round((itemsView.length - viewportTopOffset) / 2) :
                         this.state.index;
      let closestIndex = itemsView.length;
      let indexes = [];
      let diff = closestIndex;
      let isValueFound = false;

      // Find indexes of items match the value
      itemsView.forEach((item, index) => {
        if (item.value == value) {
          indexes.push(index);
        }
      });

      // Find the index closest to currentIndex
      indexes.forEach(index => {
        let d = Math.abs(index - currentIndex);
        if (d < diff) {
          diff = d;
          closestIndex = index;
          isValueFound = true;
        }
      });

      return isValueFound ? (closestIndex - viewportTopOffset) : -1;
    },

    /**
     * Scroll to a value.
     *
     * @param  {Number/String} value: Value to scroll to
     * @param  {Boolean} centering: Whether or not to scroll to center location
     */
    _scrollTo(value, centering) {
      const index = this._getScrollIndex(value, centering);
      // Do nothing if the value is not found
      if (index > -1) {
        this.state.index = index;
        this.elements.spinner.scrollTop = this.state.index * ITEM_HEIGHT * this.props.rootFontSize;
      }
    },

    /**
     * Smooth scroll to a value.
     *
     * @param  {Number/String} value: Value to scroll to
     */
    _smoothScrollTo(value) {
      const index = this._getScrollIndex(value);
      // Do nothing if the value is not found
      if (index > -1) {
        this.state.index = index;
        this._smoothScrollToIndex(this.state.index);
      }
    },

    /**
     * Smooth scroll to a value based on the index
     *
     * @param  {Number} index: Index number
     */
    _smoothScrollToIndex(index) {
      const element = this.elements.spinner.children[index];
      if (element) {
        // Set the isScrolling flag before smooth scrolling begins
        // and remove it when it has reached the destination.
        // This prevents input box jump when input box changes values
        this.state.isScrolling = true;
        element.scrollIntoView({
          behavior: "smooth", block: "start"
        });
      }
    },

    /**
     * Update the selection state.
     */
    _updateSelection() {
      const { itemsViewElements, selected } = this.elements;
      const { itemsView, index } = this.state;
      const { viewportTopOffset } = this.props;
      const currentItemIndex = index + viewportTopOffset;

      if (selected && selected != itemsViewElements[currentItemIndex]) {
        this._removeSelection();
      }

      this.elements.selected = itemsViewElements[currentItemIndex];
      if (itemsView[currentItemIndex] && itemsView[currentItemIndex].enabled) {
        this.elements.selected.classList.add("selection");
      }
    },

    /**
     * Remove selection if selected exists and different from current
     */
    _removeSelection() {
      const { selected } = this.elements;
      if (selected) {
        selected.classList.remove("selection");
      }
    },

    /**
     * Compares arrays of objects. It assumes the structure is an array of
     * objects, and objects in a and b have the same number of properties.
     *
     * @param  {Array<Object>} a
     * @param  {Array<Object>} b
     * @return {Boolean}  Returns true if a and b are different
     */
    _isArrayDiff(a, b) {
      // Check reference first, exit early if reference is the same.
      if (a == b) {
        return false;
      }

      if (a.length != b.length) {
        return true;
      }

      for (let i = 0; i < a.length; i++) {
        for (let prop in a[i]) {
          if (a[i][prop] != b[i][prop]) {
            return true;
          }
        }
      }
      return false;
    }
  };
}
