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
 *        }
 * @param {Object} context
 *        {
 *          {DOMElement} weekHeader
 *          {DOMElement} daysView
 *        }
 */
function Calendar(options, context) {
  const DAYS_IN_A_WEEK = 7;

  this.context = context;
  this.state = {
    days: [],
    weekHeaders: []
  };
  this.props = {};
  this.elements = {
    weekHeaders: this._generateNodes(DAYS_IN_A_WEEK, context.weekHeader),
    daysView: this._generateNodes(options.calViewSize, context.daysView)
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
   *            {Number} dateValue: Date in milliseconds
   *            {Number} textContent
   *            {Array<String>} classNames
   *          }
   *          {Array<Object>} weekHeaders: Data for weekHeaders
   *          {
   *            {Number} textContent
   *            {Array<String>} classNames
   *            {Boolean} enabled
   *          }
   *          {Function} getDayString: Transform day number to string
   *          {Function} getWeekHeaderString: Transform day of week number to string
   *          {Function} setSelection: Set selection for dateKeeper
   *        }
   */
  setProps(props) {
    if (props.isVisible) {
      // Transform the days and weekHeaders array for rendering
      const days = props.days.map(({ dateObj, classNames, enabled }) => {
        return {
          textContent: props.getDayString(dateObj.getUTCDate()),
          className: classNames.join(" "),
          enabled
        };
      });
      const weekHeaders = props.weekHeaders.map(({ textContent, classNames }) => {
        return {
          textContent: props.getWeekHeaderString(textContent),
          className: classNames.join(" ")
        };
      });
      // Update the DOM nodes states
      this._render({
        elements: this.elements.daysView,
        items: days,
        prevState: this.state.days
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

    this.props = Object.assign(this.props, props);
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
    for (let i = 0, l = items.length; i < l; i++) {
      let el = elements[i];

      // Check if state from last render has changed, if so, update the elements
      if (!prevState[i] || prevState[i].textContent != items[i].textContent) {
        el.textContent = items[i].textContent;
      }
      if (!prevState[i] || prevState[i].className != items[i].className) {
        el.className = items[i].className;
      }
    }
  },

  /**
   * Generate DOM nodes
   *
   * @param  {Number} size: Number of nodes to generate
   * @param  {DOMElement} context: Element to append the nodes to
   * @return {Array<DOMElement>}
   */
  _generateNodes(size, context) {
    let frag = document.createDocumentFragment();
    let refs = [];

    for (let i = 0; i < size; i++) {
      let el = document.createElement("div");
      el.dataset.id = i;
      refs.push(el);
      frag.appendChild(el);
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
        if (event.target.parentNode == this.context.daysView) {
          let targetId = event.target.dataset.id;
          let targetObj = this.props.days[targetId];
          if (targetObj.enabled) {
            this.props.setSelection(targetObj.dateObj);
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
  }
};
