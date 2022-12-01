/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * Initialize the Calendar and generate nodes for week headers and days, and
 * attach event listeners.
 *
 * @param {Object} options
 *        {
 *          {Number} calViewSize: Number of days to appear on a calendar view
 *          {Function} getDayString: Transform day number to string
 *          {Function} getWeekHeaderString: Transform day of week number to string
 *          {Function} setSelection: Set selection for dateKeeper
 *        }
 * @param {Object} context
 *        {
 *          {DOMElement} weekHeader
 *          {DOMElement} daysView
 *        }
 */
function Calendar(options, context) {
  this.context = context;
  this.context.DAYS_IN_A_WEEK = 7;
  this.state = {
    days: [],
    weekHeaders: [],
    setSelection: options.setSelection,
    getDayString: options.getDayString,
    getWeekHeaderString: options.getWeekHeaderString,
  };
  this.elements = {
    weekHeaders: this._generateNodes(
      this.context.DAYS_IN_A_WEEK,
      context.weekHeader
    ),
    daysView: this._generateNodes(options.calViewSize, context.daysView),
  };

  this._attachEventListeners();
}

Calendar.prototype = {
  /**
   * Set new properties and render them.
   *
   * @param {Object} props
   *        {
   *          {Boolean} isVisible: Whether or not the calendar is in view
   *          {Array<Object>} days: Data for days
   *          {
   *            {Date} dateObj
   *            {Number} content
   *            {Array<String>} classNames
   *            {Boolean} enabled
   *          }
   *          {Array<Object>} weekHeaders: Data for weekHeaders
   *          {
   *            {Number} content
   *            {Array<String>} classNames
   *          }
   *        }
   */
  setProps(props) {
    if (props.isVisible) {
      // Transform the days and weekHeaders array for rendering
      const days = props.days.map(
        ({ dateObj, content, classNames, enabled }) => {
          return {
            dateObj,
            textContent: this.state.getDayString(content),
            className: classNames.join(" "),
            enabled,
          };
        }
      );
      const weekHeaders = props.weekHeaders.map(({ content, classNames }) => {
        return {
          textContent: this.state.getWeekHeaderString(content),
          className: classNames.join(" "),
        };
      });
      // Update the DOM nodes states
      this._render({
        elements: this.elements.daysView,
        items: days,
        prevState: this.state.days,
      });
      this._render({
        elements: this.elements.weekHeaders,
        items: weekHeaders,
        prevState: this.state.weekHeaders,
      });
      // Update the state to current
      this.state.days = days;
      this.state.weekHeaders = weekHeaders;
    }
  },

  /**
   * Render the items onto the DOM nodes
   * @param  {Object}
   *         {
   *           {Array<DOMElement>} elements
   *           {Array<Object>} items
   *           {Array<Object>} prevState: state of items from last render
   *         }
   */
  _render({ elements, items, prevState }) {
    let selectedEl;
    let todayEl;
    let firstDayEl;

    for (let i = 0, l = items.length; i < l; i++) {
      let el = elements[i];

      // Check if state from last render has changed, if so, update the elements
      if (!prevState[i] || prevState[i].textContent != items[i].textContent) {
        el.textContent = items[i].textContent;
      }
      if (!prevState[i] || prevState[i].className != items[i].className) {
        el.className = items[i].className;
      }

      if (el.tagName === "td") {
        el.setAttribute("role", "gridcell");

        // Flush states from the previous view
        el.removeAttribute("tabindex");
        el.removeAttribute("aria-disabled");
        el.removeAttribute("aria-selected");
        el.removeAttribute("aria-current");

        // Set new states and properties
        if (el.classList.contains("today")) {
          // Current date/today is communicated to assistive technology
          el.setAttribute("aria-current", "date");
          todayEl = el;
        }
        if (el.classList.contains("selection")) {
          // Selection is included in the focus order, if from the current month
          el.setAttribute("aria-selected", "true");
          if (!el.classList.contains("outside")) {
            selectedEl = el;
          }
        } else if (el.classList.contains("out-of-range")) {
          // Dates that are outside of the range are not selected and cannot be
          el.setAttribute("aria-disabled", "true");
          el.removeAttribute("aria-selected");
        } else {
          // Other dates are not selected, but could be
          el.setAttribute("aria-selected", "false");
        }
        // When no selection or current day/today is present, make the first
        // of the month focusable
        if (el.textContent === "1" && !firstDayEl) {
          let firstDay = new Date(items[i].dateObj);
          firstDay.setUTCDate("1");
          if (this._isSameDay(items[i].dateObj, firstDay)) {
            firstDayEl = el;
          }
        }
      }
    }

    // Selected date is always focusable on init, otherwise make focusable
    // the current day/today or the first day of the month
    if (selectedEl) {
      selectedEl.setAttribute("tabindex", "0");
    } else if (todayEl) {
      todayEl.setAttribute("tabindex", "0");
    } else if (firstDayEl) {
      firstDayEl.setAttribute("tabindex", "0");
    }
  },

  /**
   * Generate DOM nodes with HTML table markup
   *
   * @param  {Number} size: Number of nodes to generate
   * @param  {DOMElement} context: Element to append the nodes to
   * @return {Array<DOMElement>}
   */
  _generateNodes(size, context) {
    let frag = document.createDocumentFragment();
    let refs = [];

    // Create table row to present a week:
    let rowEl = document.createElement("tr");
    for (let i = 0; i < size; i++) {
      // Create table cell for a table header (weekday) or body (date)
      let el;
      if (context.classList.contains("week-header")) {
        el = document.createElement("th");
        el.setAttribute("scope", "col");
        // Explicitly assigning the role as a workaround for the bug 1711273:
        el.setAttribute("role", "columnheader");
      } else {
        el = document.createElement("td");
      }

      el.dataset.id = i;
      refs.push(el);
      rowEl.appendChild(el);

      // Ensure each table row (week) has only
      // seven table cells (days) for a Gregorian calendar
      if ((i + 1) % this.context.DAYS_IN_A_WEEK === 0) {
        frag.appendChild(rowEl);
        rowEl = document.createElement("tr");
      }
    }
    context.appendChild(frag);

    return refs;
  },

  /**
   * Handle events
   * @param  {DOMEvent} event
   */
  handleEvent(event) {
    switch (event.type) {
      case "click": {
        if (event.target.parentNode.parentNode == this.context.daysView) {
          let targetId = event.target.dataset.id;
          let targetObj = this.state.days[targetId];
          if (targetObj.enabled) {
            this.state.setSelection(targetObj.dateObj);
          }
        }
        break;
      }
    }
  },

  /**
   * Comparing two date objects to ensure they produce the same date
   * @param {Date} dateObj1: Date object from the updated state
   * @param {Date} dateObj2: Date object from the previous state
   * @returns
   */
  _isSameDay(dateObj1, dateObj2) {
    return (
      dateObj1.getUTCFullYear() == dateObj2.getUTCFullYear() &&
      dateObj1.getUTCMonth() == dateObj2.getUTCMonth() &&
      dateObj1.getUTCDate() == dateObj2.getUTCDate()
    );
  },

  /**
   * Attach event listener to daysView
   */
  _attachEventListeners() {
    this.context.daysView.addEventListener("click", this);
  },
};
