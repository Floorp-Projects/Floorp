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
 *          {Function} setCalendarMonth: Update the month shown by the dateView
 *                     to a specific month of a specific year
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
    setCalendarMonth: options.setCalendarMonth,
    getDayString: options.getDayString,
    getWeekHeaderString: options.getWeekHeaderString,
    focusedDate: null,
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
      // Update the state to current and place keyboard focus
      this.state.days = days;
      this.state.weekHeaders = weekHeaders;
      this.focusDay();
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
    let selected = {};
    let today = {};
    let sameDay = {};
    let firstDay = {};

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
        if (
          this.state.focusedDate &&
          this._isSameDayOfMonth(items[i].dateObj, this.state.focusedDate) &&
          !el.classList.contains("outside")
        ) {
          // When any other date was focused previously, send the focus
          // to the same day of month, but only within the current month
          sameDay.el = el;
          sameDay.dateObj = items[i].dateObj;
        }
        if (el.classList.contains("today")) {
          // Current date/today is communicated to assistive technology
          el.setAttribute("aria-current", "date");
          if (!el.classList.contains("outside")) {
            today.el = el;
            today.dateObj = items[i].dateObj;
          }
        }
        if (el.classList.contains("selection")) {
          // Selection is communicated to assistive technology
          // and may be included in the focus order when from the current month
          el.setAttribute("aria-selected", "true");

          if (!el.classList.contains("outside")) {
            selected.el = el;
            selected.dateObj = items[i].dateObj;
          }
        } else if (el.classList.contains("out-of-range")) {
          // Dates that are outside of the range are not selected and cannot be
          el.setAttribute("aria-disabled", "true");
          el.removeAttribute("aria-selected");
        } else {
          // Other dates are not selected, but could be
          el.setAttribute("aria-selected", "false");
        }
        if (el.textContent === "1" && !firstDay.el) {
          // When no previous day, no selection, or no current day/today
          // is present, make the first of the month focusable
          firstDay.dateObj = items[i].dateObj;
          firstDay.dateObj.setUTCDate("1");

          if (this._isSameDay(items[i].dateObj, firstDay.dateObj)) {
            firstDay.el = el;
            firstDay.dateObj = items[i].dateObj;
          }
        }
      }
    }

    // The previously focused date (if the picker is updated and the grid still
    // contains the date) is always focusable. The selected date on init is also
    // always focusable. If neither exist, we make the current day or the first
    // day of the month focusable.
    if (sameDay.el) {
      sameDay.el.setAttribute("tabindex", "0");
      this.state.focusedDate = new Date(sameDay.dateObj);
    } else if (selected.el) {
      selected.el.setAttribute("tabindex", "0");
      this.state.focusedDate = new Date(selected.dateObj);
    } else if (today.el) {
      today.el.setAttribute("tabindex", "0");
      this.state.focusedDate = new Date(today.dateObj);
    } else if (firstDay.el) {
      firstDay.el.setAttribute("tabindex", "0");
      this.state.focusedDate = new Date(firstDay.dateObj);
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
              this._handleKeydownEvent(1 * direction);
              break;
            }
            case "ArrowLeft": {
              // Moves focus to the previous day. If the next day is
              // out-of-range, update the view to show the previous month
              this._handleKeydownEvent(-1 * direction);
              break;
            }
            case "ArrowUp": {
              // Moves focus to the same day of the previous week. If the next
              // day is out-of-range, update the view to show the previous month
              this._handleKeydownEvent(-1 * this.context.DAYS_IN_A_WEEK);
              break;
            }
            case "ArrowDown": {
              // Moves focus to the same day of the next week. If the next
              // day is out-of-range, update the view to show the previous month
              this._handleKeydownEvent(1 * this.context.DAYS_IN_A_WEEK);
              break;
            }
            case "Home": {
              // Moves focus to the first day (ie. Sunday) of the current week
              if (event.ctrlKey) {
                // Moves focus to the first day of the current month
                this.state.focusedDate.setUTCDate(1);
                this._updateKeyboardFocus();
              } else {
                this._handleKeydownEvent(
                  this.state.focusedDate.getUTCDay() * -1
                );
              }
              break;
            }
            case "End": {
              // Moves focus to the last day (ie. Saturday) of the current week
              if (event.ctrlKey) {
                // Moves focus to the last day of the current month
                let lastDateOfMonth = new Date(
                  this.state.focusedDate.getUTCFullYear(),
                  this.state.focusedDate.getUTCMonth() + 1,
                  0
                );
                this.state.focusedDate = lastDateOfMonth;
                this._updateKeyboardFocus();
              } else {
                this._handleKeydownEvent(
                  this.context.DAYS_IN_A_WEEK -
                    1 -
                    this.state.focusedDate.getUTCDay()
                );
              }
              break;
            }
            case "PageUp": {
              // Changes the view to the previous month/year
              // and sets focus on the same day.
              // If that day does not exist, then moves focus
              // to the same day of the same week.
              if (event.shiftKey) {
                // Previous year
                let prevYear = this.state.focusedDate.getUTCFullYear() - 1;
                this.state.focusedDate.setUTCFullYear(prevYear);
              } else {
                // Previous month
                let prevMonth = this.state.focusedDate.getUTCMonth() - 1;
                this.state.focusedDate.setUTCMonth(prevMonth);
              }
              this.state.setCalendarMonth(
                this.state.focusedDate.getUTCFullYear(),
                this.state.focusedDate.getUTCMonth()
              );
              this._updateKeyboardFocus();
              break;
            }
            case "PageDown": {
              // Changes the view to the next month/year
              // and sets focus on the same day.
              // If that day does not exist, then moves focus
              // to the same day of the same week.
              if (event.shiftKey) {
                // Next year
                let nextYear = this.state.focusedDate.getUTCFullYear() + 1;
                this.state.focusedDate.setUTCFullYear(nextYear);
              } else {
                // Next month
                let nextMonth = this.state.focusedDate.getUTCMonth() + 1;
                this.state.focusedDate.setUTCMonth(nextMonth);
              }
              this.state.setCalendarMonth(
                this.state.focusedDate.getUTCFullYear(),
                this.state.focusedDate.getUTCMonth()
              );
              this._updateKeyboardFocus();
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
   * Comparing two date objects to ensure they produce the same day of the month,
   * while being on different months
   * @param  {Date} dateObj1: Date object from the updated state
   * @param  {Date} dateObj2: Date object from the previous state
   * @return {Boolean} If two date objects are the same day of the month
   */
  _isSameDayOfMonth(dateObj1, dateObj2) {
    return dateObj1.getUTCDate() == dateObj2.getUTCDate();
  },

  /**
   * Manage focus for the keyboard navigation for the daysView grid
   * @param  {Number} offsetDays: The direction and the number of days to move
   *                            the focus by, where a negative number (i.e. -1)
   *                            moves the focus to the previous day
   */
  _handleKeydownEvent(offsetDays) {
    let newFocusedDay = this.state.focusedDate.getUTCDate() + offsetDays;
    let newFocusedDate = new Date(this.state.focusedDate);
    newFocusedDate.setUTCDate(newFocusedDay);

    // Update the month, if the next focused element is outside
    if (newFocusedDate.getUTCMonth() !== this.state.focusedDate.getUTCMonth()) {
      this.state.setCalendarMonth(
        newFocusedDate.getUTCFullYear(),
        newFocusedDate.getUTCMonth()
      );
    }
    this.state.focusedDate.setUTCDate(newFocusedDate.getUTCDate());
    this._updateKeyboardFocus();
  },

  /**
   * Update the daysView grid and send focus to the next day
   * based on the current state fo the Calendar
   */
  _updateKeyboardFocus() {
    this._render({
      elements: this.elements.daysView,
      items: this.state.days,
      prevState: this.state.days,
    });
    this.focusDay();
  },

  /**
   * Place keyboard focus on the calendar grid, when the datepicker is initiated or updated.
   * A "tabindex" attribute is provided to only one date within the grid
   * by the "render()" method and this focusable element will be focused.
   */
  focusDay() {
    const focusable = this.context.daysView.querySelector('[tabindex="0"]');
    if (focusable) {
      focusable.focus();
    }
  },
};
