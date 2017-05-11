/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * DateKeeper keeps track of the date states.
 */
function DateKeeper(props) {
  this.init(props);
}

{
  const DAYS_IN_A_WEEK = 7,
        MONTHS_IN_A_YEAR = 12,
        YEAR_VIEW_SIZE = 200,
        YEAR_BUFFER_SIZE = 10,
        // The min and max values are derived from the ECMAScript spec:
        // http://ecma-international.org/ecma-262/5.1/#sec-15.9.1.1
        MIN_DATE = -8640000000000000,
        MAX_DATE = 8640000000000000;

  DateKeeper.prototype = {
    get year() {
      return this.state.dateObj.getUTCFullYear();
    },

    get month() {
      return this.state.dateObj.getUTCMonth();
    },

    get day() {
      return this.state.dateObj.getUTCDate();
    },

    get selection() {
      return this.state.selection;
    },

    /**
     * Initialize DateKeeper
     * @param  {Number} year
     * @param  {Number} month
     * @param  {Number} day
     * @param  {String} min
     * @param  {String} max
     * @param  {Number} step
     * @param  {Number} stepBase
     * @param  {Number} firstDayOfWeek
     * @param  {Array<Number>} weekends
     * @param  {Number} calViewSize
     */
    init({ year, month, day, min, max, step, stepBase, firstDayOfWeek = 0, weekends = [0], calViewSize = 42 }) {
      const today = new Date();
      const isDateSet = year != undefined && month != undefined && day != undefined;

      this.state = {
        step, firstDayOfWeek, weekends, calViewSize,
        min: new Date(min != undefined ? min : MIN_DATE),
        max: new Date(max != undefined ? max : MAX_DATE),
        stepBase: new Date(stepBase),
        today: this._newUTCDate(today.getFullYear(), today.getMonth(), today.getDate()),
        weekHeaders: this._getWeekHeaders(firstDayOfWeek, weekends),
        years: [],
        months: [],
        days: [],
        selection: { year, month, day },
      };

      this.state.dateObj = isDateSet ?
                           this._newUTCDate(year, month, day) :
                           new Date(this.state.today);
    },
    /**
     * Set new date. The year is always treated as full year, so the short-form
     * is not supported.
     * @param {Object} date parts
     *        {
     *          {Number} year [optional]
     *          {Number} month [optional]
     *          {Number} date [optional]
     *        }
     */
    set({ year = this.year, month = this.month, day = this.day }) {
      // Use setUTCFullYear so that year 99 doesn't get parsed as 1999
      this.state.dateObj.setUTCFullYear(year, month, day);
    },

    /**
     * Set selection date
     * @param {Number} year
     * @param {Number} month
     * @param {Number} day
     */
    setSelection({ year, month, day }) {
      this.state.selection.year = year;
      this.state.selection.month = month;
      this.state.selection.day = day;
    },

    /**
     * Set month. Makes sure the day is <= the last day of the month
     * @param {Number} month
     */
    setMonth(month) {
      const lastDayOfMonth = this._newUTCDate(this.year, month + 1, 0).getUTCDate();
      this.set({ year: this.year,
                 month,
                 day: Math.min(this.day, lastDayOfMonth) });
    },

    /**
     * Set year. Makes sure the day is <= the last day of the month
     * @param {Number} year
     */
    setYear(year) {
      const lastDayOfMonth = this._newUTCDate(year, this.month + 1, 0).getUTCDate();
      this.set({ year,
                 month: this.month,
                 day: Math.min(this.day, lastDayOfMonth) });
    },

    /**
     * Set month by offset. Makes sure the day is <= the last day of the month
     * @param {Number} offset
     */
    setMonthByOffset(offset) {
      const lastDayOfMonth = this._newUTCDate(this.year, this.month + offset + 1, 0).getUTCDate();
      this.set({ year: this.year,
                 month: this.month + offset,
                 day: Math.min(this.day, lastDayOfMonth) });
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
      let years = [];

      const firstItem = this.state.years[0];
      const lastItem = this.state.years[this.state.years.length - 1];
      const currentYear = this.year;

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
     *           {Date} dateObj
     *           {Array<String>} classNames
     *           {Boolean} enabled
     *         }
     */
    getDays() {
      const firstDayOfMonth = this._getFirstCalendarDate(this.state.dateObj, this.state.firstDayOfWeek);
      const month = this.month;
      let days = [];

      for (let i = 0; i < this.state.calViewSize; i++) {
        const dateObj = this._newUTCDate(firstDayOfMonth.getUTCFullYear(),
                                         firstDayOfMonth.getUTCMonth(),
                                         firstDayOfMonth.getUTCDate() + i);
        let classNames = [];
        let enabled = true;

        const isWeekend = this.state.weekends.includes(dateObj.getUTCDay());
        const isCurrentMonth = month == dateObj.getUTCMonth();
        const isSelection = this.state.selection.year == dateObj.getUTCFullYear() &&
                            this.state.selection.month == dateObj.getUTCMonth() &&
                            this.state.selection.day == dateObj.getUTCDate();
        const isOutOfRange = dateObj.getTime() < this.state.min.getTime() ||
                             dateObj.getTime() > this.state.max.getTime();
        const isToday = this.state.today.getTime() == dateObj.getTime();
        const isOffStep = this._checkIsOffStep(dateObj,
                                               this._newUTCDate(dateObj.getUTCFullYear(),
                                                                dateObj.getUTCMonth(),
                                                                dateObj.getUTCDate() + 1));

        if (isWeekend) {
          classNames.push("weekend");
        }
        if (!isCurrentMonth) {
          classNames.push("outside");
        }
        if (isSelection && !isOutOfRange && !isOffStep) {
          classNames.push("selection");
        }
        if (isOutOfRange) {
          classNames.push("out-of-range");
          enabled = false;
        }
        if (isToday) {
          classNames.push("today");
        }
        if (isOffStep) {
          classNames.push("off-step");
          enabled = false;
        }
        days.push({
          dateObj,
          classNames,
          enabled,
        });
      }
      return days;
    },

    /**
     * Check if a date is off step given a starting point and the next increment
     * @param  {Date} start
     * @param  {Date} next
     * @return {Boolean}
     */
    _checkIsOffStep(start, next) {
      // If the increment is larger or equal to the step, it must not be off-step.
      if (next - start >= this.state.step) {
        return false;
      }
      // Calculate the last valid date
      const lastValidStep = Math.floor((next - 1 - this.state.stepBase) / this.state.step);
      const lastValidTimeInMs = lastValidStep * this.state.step + this.state.stepBase.getTime();
      // The date is off-step if the last valid date is smaller than the start date
      return lastValidTimeInMs < start.getTime();
    },

    /**
     * Get week headers for calendar
     * @param  {Number} firstDayOfWeek
     * @param  {Array<Number>} weekends
     * @return {Array<Object>}
     *         {
     *           {Number} textContent
     *           {Array<String>} classNames
     *         }
     */
    _getWeekHeaders(firstDayOfWeek, weekends) {
      let headers = [];
      let dayOfWeek = firstDayOfWeek;

      for (let i = 0; i < DAYS_IN_A_WEEK; i++) {
        headers.push({
          textContent: dayOfWeek % DAYS_IN_A_WEEK,
          classNames: weekends.includes(dayOfWeek % DAYS_IN_A_WEEK) ? ["weekend"] : []
        });
        dayOfWeek++;
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
