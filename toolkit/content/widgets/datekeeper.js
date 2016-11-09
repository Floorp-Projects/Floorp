/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * DateKeeper keeps track of the date states.
 *
 * @param {Object} date parts
 *        {
 *          {Number} year
 *          {Number} month
 *          {Number} date
 *        }
 *        {Object} options
 *        {
 *          {Number} firstDayOfWeek [optional]
 *          {Array<Number>} weekends [optional]
 *          {Number} calViewSize [optional]
 *        }
 */
function DateKeeper({ year, month, date }, { firstDayOfWeek = 0, weekends = [0], calViewSize = 42 }) {
  this.state = {
    firstDayOfWeek, weekends, calViewSize,
    dateObj: new Date(0),
    years: [],
    months: [],
    days: []
  };
  this.state.weekHeaders = this._getWeekHeaders(firstDayOfWeek);
  this._update(year, month, date);
}

{
  const DAYS_IN_A_WEEK = 7,
        MONTHS_IN_A_YEAR = 12,
        YEAR_VIEW_SIZE = 200,
        YEAR_BUFFER_SIZE = 10;

  const debug = 0 ? console.log.bind(console, "[datekeeper]") : function() {};

  DateKeeper.prototype = {
    /**
     * Set new date
     * @param {Object} date parts
     *        {
     *          {Number} year [optional]
     *          {Number} month [optional]
     *          {Number} date [optional]
     *        }
     */
    set({ year = this.state.year, month = this.state.month, date = this.state.date }) {
      this._update(year, month, date);
    },

    /**
     * Set date with value
     * @param {Number} value: Date value
     */
    setValue(value) {
      const dateObj = new Date(value);
      this._update(dateObj.getUTCFullYear(), dateObj.getUTCMonth(), dateObj.getUTCDate());
    },

    /**
     * Set month. Makes sure the date is <= the last day of the month
     * @param {Number} month
     */
    setMonth(month) {
      const lastDayOfMonth = this._newUTCDate(this.state.year, month + 1, 0).getUTCDate();
      this._update(this.state.year, month, Math.min(this.state.date, lastDayOfMonth));
    },

    /**
     * Set year. Makes sure the date is <= the last day of the month
     * @param {Number} year
     */
    setYear(year) {
      const lastDayOfMonth = this._newUTCDate(year, this.state.month + 1, 0).getUTCDate();
      this._update(year, this.state.month, Math.min(this.state.date, lastDayOfMonth));
    },

    /**
     * Set month by offset. Makes sure the date is <= the last day of the month
     * @param {Number} offset
     */
    setMonthByOffset(offset) {
      const lastDayOfMonth = this._newUTCDate(this.state.year, this.state.month + offset + 1, 0).getUTCDate();
      this._update(this.state.year, this.state.month + offset, Math.min(this.state.date, lastDayOfMonth));
    },

    /**
     * Update the states.
     * @param  {Number} year  [description]
     * @param  {Number} month [description]
     * @param  {Number} date  [description]
     */
    _update(year, month, date) {
      // Use setUTCFullYear so that year 99 doesn't get parsed as 1999
      this.state.dateObj.setUTCFullYear(year, month, date);
      this.state.year = this.state.dateObj.getUTCFullYear();
      this.state.month = this.state.dateObj.getUTCMonth();
      this.state.date = this.state.dateObj.getUTCDate();
    },

    /**
     * Generate the array of months
     * @return {Array<Object>}
     *         {
     *           {Number} value: Month in int
     *           {Boolean} enabled
     *         }
     */
    getMonths() {
      // TODO: add min/max and step support
      let months = [];

      for (let i = 0; i < MONTHS_IN_A_YEAR; i++) {
        months.push({
          value: i,
          enabled: true
        });
      }

      return months;
    },

    /**
     * Generate the array of years
     * @return {Array<Object>}
     *         {
     *           {Number} value: Year in int
     *           {Boolean} enabled
     *         }
     */
    getYears() {
      // TODO: add min/max and step support
      let years = [];

      const firstItem = this.state.years[0];
      const lastItem = this.state.years[this.state.years.length - 1];
      const currentYear = this.state.dateObj.getUTCFullYear();

      // Generate new years array when the year is outside of the first &
      // last item range. If not, return the cached result.
      if (!firstItem || !lastItem ||
          currentYear <= firstItem.value + YEAR_BUFFER_SIZE ||
          currentYear >= lastItem.value - YEAR_BUFFER_SIZE) {
        // The year is set in the middle with items on both directions
        for (let i = -(YEAR_VIEW_SIZE / 2); i < YEAR_VIEW_SIZE / 2; i++) {
          years.push({
            value: currentYear + i,
            enabled: true
          });
        }
        this.state.years = years;
      }
      return this.state.years;
    },

    /**
     * Get days for calendar
     * @return {Array<Object>}
     *         {
     *           {Number} dateValue
     *           {Number} textContent
     *           {Array<String>} classNames
     *         }
     */
    getDays() {
      // TODO: add min/max and step support
      let firstDayOfMonth = this._getFirstCalendarDate(this.state.dateObj, this.state.firstDayOfWeek);
      let days = [];
      let month = this.state.dateObj.getUTCMonth();

      for (let i = 0; i < this.state.calViewSize; i++) {
        let dateObj = this._newUTCDate(firstDayOfMonth.getUTCFullYear(), firstDayOfMonth.getUTCMonth(), firstDayOfMonth.getUTCDate() + i);
        let classNames = [];
        if (this.state.weekends.includes(dateObj.getUTCDay())) {
          classNames.push("weekend");
        }
        if (month != dateObj.getUTCMonth()) {
          classNames.push("outside");
        }
        days.push({
          dateValue: dateObj.getTime(),
          textContent: dateObj.getUTCDate(),
          classNames
        });
      }
      return days;
    },

    /**
     * Get week headers for calendar
     * @param  {Number} firstDayOfWeek
     * @return {Array<Object>}
     *         {
     *           {Number} textContent
     *           {Array<String>} classNames
     *         }
     */
    _getWeekHeaders(firstDayOfWeek) {
      let headers = [];
      let day = firstDayOfWeek;

      for (let i = 0; i < DAYS_IN_A_WEEK; i++) {
        headers.push({
          textContent: day % DAYS_IN_A_WEEK,
          classNames: this.state.weekends.includes(day % DAYS_IN_A_WEEK) ? ["weekend"] : []
        });
        day++;
      }
      return headers;
    },

    /**
     * Get the first day on a calendar month
     * @param  {Date} dateObj
     * @param  {Number} firstDayOfWeek
     * @return {Date}
     */
    _getFirstCalendarDate(dateObj, firstDayOfWeek) {
      const daysOffset = 1 - DAYS_IN_A_WEEK;
      let firstDayOfMonth = this._newUTCDate(dateObj.getUTCFullYear(), dateObj.getUTCMonth());
      let dayOfWeek = firstDayOfMonth.getUTCDay();

      return this._newUTCDate(
        firstDayOfMonth.getUTCFullYear(),
        firstDayOfMonth.getUTCMonth(),
        // When first calendar date is the same as first day of the week, add
        // another row on top of it.
        firstDayOfWeek == dayOfWeek ? daysOffset : (firstDayOfWeek - dayOfWeek + daysOffset) % DAYS_IN_A_WEEK);
    },

    /**
     * Helper function for creating UTC dates
     * @param  {...[Number]} parts
     * @return {Date}
     */
    _newUTCDate(...parts) {
      return new Date(Date.UTC(...parts));
    }
  };
}
