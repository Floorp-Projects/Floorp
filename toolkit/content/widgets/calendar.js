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
 *          {Function} setMonthByOffset: Update the month shown by the dateView
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
    setMonthByOffset: options.setMonthByOffset,
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
        if (this.context.daysView.contains(event.target)) {
          let targetId = event.target.dataset.id;
          let targetObj = this.state.days[targetId];
          if (targetObj.enabled) {
            this.state.setSelection(targetObj.dateObj);
          }
        }
        break;
      }

      case "keydown": {
        // Providing keyboard navigation support in accordance with
        // the ARIA Grid and Dialog design patterns
        if (this.context.daysView.contains(event.target)) {
          // If RTL, the offset direction for Right/Left needs to be reversed
          const direction = Services.locale.isAppLocaleRTL ? -1 : 1;

          switch (event.key) {
            case "Enter":
            case " ": {
              let targetId = event.target.dataset.id;
              let targetObj = this.state.days[targetId];
              if (targetObj.enabled) {
                this.state.setSelection(targetObj.dateObj);
              }
              break;
            }

            case "ArrowRight": {
              // Moves focus to the next day. If the next day is
              // out-of-range, update the view to show the next month
              this._handleKeydownEvent(event.target, 1 * direction);
              break;
            }
            case "ArrowLeft": {
              // Moves focus to the previous day. If the next day is
              // out-of-range, update the view to show the previous month
              this._handleKeydownEvent(event.target, -1 * direction);
              break;
            }
            case "ArrowUp": {
              // Moves focus to the same day of the previous week. If the next
              // day is out-of-range, update the view to show the previous month
              this._handleKeydownEvent(
                event.target,
                -1,
                this.context.DAYS_IN_A_WEEK
              );
              break;
            }
            case "ArrowDown": {
              // Moves focus to the same day of the next week. If the next
              // day is out-of-range, update the view to show the previous month
              this._handleKeydownEvent(
                event.target,
                1,
                this.context.DAYS_IN_A_WEEK
              );
              break;
            }
            case "Home": {
              // Moves focus to the first day (ie. Sunday) of the current week
              let nextId;
              if (event.ctrlKey) {
                // Moves focus to the first day of the current month
                for (let i = 0; i < this.state.days.length; i++) {
                  if (this.state.days[i].dateObj.getUTCDate() == 1) {
                    nextId = i;
                    break;
                  }
                }
              } else {
                nextId =
                  Number(event.target.dataset.id) -
                  (Number(event.target.dataset.id) %
                    this.context.DAYS_IN_A_WEEK);
                nextId = this._updateViewIfOutside(nextId, -1);
              }
              this._updateKeyboardFocus(event.target, nextId);
              break;
            }
            case "End": {
              // Moves focus to the last day (ie. Saturday) of the current week
              let nextId;
              if (event.ctrlKey) {
                // Moves focus to the last day of the current month
                for (let i = this.state.days.length - 1; i >= 0; i--) {
                  if (this.state.days[i].dateObj.getUTCDate() == 1) {
                    nextId = i - 1;
                    break;
                  }
                }
              } else {
                nextId =
                  Number(event.target.dataset.id) +
                  (this.context.DAYS_IN_A_WEEK - 1) -
                  (Number(event.target.dataset.id) %
                    this.context.DAYS_IN_A_WEEK);
                nextId = this._updateViewIfOutside(nextId, 1);
              }
              this._updateKeyboardFocus(event.target, nextId);
              break;
            }
            case "PageUp": {
              // Changes the view to the previous month/year
              // and sets focus on the same day.
              // If that day does not exist, then moves focus
              // to the same day of the same week.
              let targetId = event.target.dataset.id;
              let nextDate = this.state.days[targetId].dateObj;
              if (event.shiftKey) {
                // Previous year
                this.state.setMonthByOffset(-12);
                nextDate.setYear(nextDate.getFullYear() - 1);
              } else {
                // Previous month
                this.state.setMonthByOffset(-1);
                nextDate.setMonth(nextDate.getMonth() - 1);
              }
              let nextId = this._calculateNextId(nextDate);
              // Outside dates for the previous month (ie. when moving from
              // the March 30th back to February where 30th does not exist)
              // occur at the end of the month, thus month offset is 1
              nextId = this._updateViewIfOutside(nextId, 1);
              this._updateKeyboardFocus(event.target, nextId);
              break;
            }
            case "PageDown": {
              // Changes the view to the next month/year
              // and sets focus on the same day.
              // If that day does not exist, then moves focus
              // to the same day of the same week.
              let targetId = event.target.dataset.id;
              let nextDate = this.state.days[targetId].dateObj;
              if (event.shiftKey) {
                // Next year
                this.state.setMonthByOffset(12);
                nextDate.setYear(nextDate.getFullYear() + 1);
              } else {
                // Next month
                this.state.setMonthByOffset(1);
                nextDate.setMonth(nextDate.getMonth() + 1);
              }
              let nextId = this._calculateNextId(nextDate);
              nextId = this._updateViewIfOutside(nextId, 1);
              this._updateKeyboardFocus(event.target, nextId);
              break;
            }
          }
        }
        break;
      }
    }
  },

  /**
   * Attach event listener to daysView
   */
  _attachEventListeners() {
    this.context.daysView.addEventListener("click", this);
    this.context.daysView.addEventListener("keydown", this);
  },

  /**
   * Find Data-id of the next element to focus on the daysView grid
   * @param {Object} nextDate: Data object of the next element to focus
   */
  _calculateNextId(nextDate) {
    for (let i = 0; i < this.state.days.length; i++) {
      if (this._isSameDay(this.state.days[i].dateObj, nextDate)) {
        return i;
      }
    }
    return null;
  },

  /**
   * Comparing two date objects to ensure they produce the same date
   * @param  {Date} dateObj1: Date object from the updated state
   * @param  {Date} dateObj2: Date object from the previous state
   * @return {Boolean} If two date objects are the same day
   */
  _isSameDay(dateObj1, dateObj2) {
    return (
      dateObj1.getUTCFullYear() == dateObj2.getUTCFullYear() &&
      dateObj1.getUTCMonth() == dateObj2.getUTCMonth() &&
      dateObj1.getUTCDate() == dateObj2.getUTCDate()
    );
  },

  /**
   * Manage focus for the keyboard navigation for the daysView grid
   * @param  {DOMElement} eTarget: The event.target day element
   * @param  {Number} offsetDir: The direction where the focus should move,
   *                             where a negative number (-1) moves backwards
   * @param  {Number} offsetSize: The number of days to move the focus by.
   */
  _handleKeydownEvent(eTarget, offsetDir, offsetSize = 1) {
    let offset = offsetDir * offsetSize;
    let nextId = Number(eTarget.dataset.id) + offset;
    if (!this.state.days[nextId]) {
      nextId = this._updateViewIfUndefined(nextId, offset, eTarget.dataset.id);
    }
    nextId = this._updateViewIfOutside(nextId, offsetDir);
    this._updateKeyboardFocus(eTarget, nextId);
  },

  /**
   * Add gridcell attributes and move focus to the next dayView element
   * @param {DOMElement} targetEl: Day element as an event.target
   * @param {Number} nextId: The data-id of the next HTML day element to focus
   */
  _updateKeyboardFocus(targetEl, nextId) {
    const nextEl = this.elements.daysView[nextId];

    targetEl.removeAttribute("tabindex");
    nextEl.setAttribute("tabindex", "0");

    nextEl.focus();
  },

  /**
   * Find Data-id of the next element to focus on the daysView grid if
   * the next element has "outside" class and belongs to another month
   * @param  {Number} nextId: The data-id of the next HTML day element to focus
   * @param  {Number} offset: The direction for the month view offset
   * @return {Number} The data-id of the next HTML day element to focus
   */
  _updateViewIfOutside(nextId, offset) {
    if (this.elements.daysView[nextId].classList.contains("outside")) {
      let nextDate = this.state.days[nextId].dateObj;
      this.state.setMonthByOffset(offset);
      nextId = this._calculateNextId(nextDate);
    }
    return nextId;
  },

  /**
   * Find Data-id of the next element to focus on the daysView grid if
   * the next element is outside of the current daysView calendar
   * @param  {Number} nextId: The data-id of the next HTML day element to focus
   * @param  {Number} offset: The number of days to move by,
   *                          where a negative number moves backwards.
   * @param  {Number} targetId: The data-id for the event target day element
   * @return {Number} The data-id of the next HTML day element to focus
   */
  _updateViewIfUndefined(nextId, offset, targetId) {
    let targetDate = this.state.days[targetId].dateObj;
    let nextDate = targetDate;
    // Get the date that needs to be focused next:
    nextDate.setDate(targetDate.getDate() + offset);
    // Update the month view to include this date:
    this.state.setMonthByOffset(Math.sign(offset));
    // Find the "data-id" of the date element:
    nextId = this._calculateNextId(nextDate);
    return nextId;
  },

  /**
   * Place keyboard focus on the calendar grid, when the datepicker is initiated.
   * The selected day is what gets focused, if such a day exists. If it does not,
   * today's date will be focused.
   */
  focus() {
    const focus = this.context.daysView.querySelector('[tabindex="0"]');
    if (focus) {
      focus.focus();
    }
  },
};
