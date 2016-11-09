/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

function DatePicker(context) {
  this.context = context;
  this._attachEventListeners();
}

{
  const CAL_VIEW_SIZE = 42;
  const debug = 0 ? console.log.bind(console, "[datepicker]") : function() {};

  DatePicker.prototype = {
    /**
     * Initializes the date picker. Set the default states and properties.
     * @param  {Object} props
     *         {
     *           {Number} year [optional]
     *           {Number} month [optional]
     *           {Number} date [optional]
     *           {String} locale [optional]: User preferred locale
     *         }
     */
    init(props = {}) {
      this.props = props;
      this._setDefaultState();
      this._createComponents();
      this._update();
    },

    /*
     * Set initial date picker states.
     */
    _setDefaultState() {
      const now = new Date();
      const { year = now.getFullYear(),
              month = now.getMonth(),
              date = now.getDate(),
              locale } = this.props;

      // TODO: Use calendar info API to get first day of week & weekends
      //       (Bug 1287503)
      const dateKeeper = new DateKeeper({
        year, month, date
      }, {
        calViewSize: CAL_VIEW_SIZE,
        firstDayOfWeek: 0,
        weekends: [0]
      });

      this.state = {
        dateKeeper,
        locale,
        isMonthPickerVisible: false,
        isYearSet: false,
        isMonthSet: false,
        isDateSet: false,
        getDayString: new Intl.NumberFormat(locale).format,
        // TODO: use calendar terms when available (Bug 1287677)
        getWeekHeaderString: weekday => ["Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"][weekday],
        setValue: ({ dateValue, selectionValue }) => {
          dateKeeper.setValue(dateValue);
          this.state.selectionValue = selectionValue;
          this.state.isYearSet = true;
          this.state.isMonthSet = true;
          this.state.isDateSet = true;
          this._update();
          this._dispatchState();
        },
        setYear: year => {
          dateKeeper.setYear(year);
          this.state.isYearSet = true;
          this._update();
          this._dispatchState();
        },
        setMonth: month => {
          dateKeeper.setMonth(month);
          this.state.isMonthSet = true;
          this._update();
          this._dispatchState();
        },
        toggleMonthPicker: () => {
          this.state.isMonthPickerVisible = !this.state.isMonthPickerVisible;
          this._update();
        }
      };
    },

    /**
     * Initalize the date picker components.
     */
    _createComponents() {
      this.components = {
        calendar: new Calendar({
          calViewSize: CAL_VIEW_SIZE,
          locale: this.state.locale
        }, {
          weekHeader: this.context.weekHeader,
          daysView: this.context.daysView
        }),
        monthYear: new MonthYear({
          setYear: this.state.setYear,
          setMonth: this.state.setMonth,
          locale: this.state.locale
        }, {
          monthYear: this.context.monthYear,
          monthYearView: this.context.monthYearView
        })
      };
    },

    /**
     * Update date picker and its components.
     */
    _update() {
      const { dateKeeper, selectionValue, isYearSet, isMonthSet, isMonthPickerVisible } = this.state;

      if (isMonthPickerVisible) {
        this.state.months = dateKeeper.getMonths();
        this.state.years = dateKeeper.getYears();
      } else {
        this.state.days = dateKeeper.getDays();
      }

      this.components.monthYear.setProps({
        isVisible: isMonthPickerVisible,
        dateObj: dateKeeper.state.dateObj,
        month: dateKeeper.state.month,
        months: this.state.months,
        year: dateKeeper.state.year,
        years: this.state.years,
        toggleMonthPicker: this.state.toggleMonthPicker
      });
      this.components.calendar.setProps({
        isVisible: !isMonthPickerVisible,
        days: this.state.days,
        weekHeaders: dateKeeper.state.weekHeaders,
        setValue: this.state.setValue,
        getDayString: this.state.getDayString,
        getWeekHeaderString: this.state.getWeekHeaderString,
        selectionValue
      });

      isMonthPickerVisible ?
        this.context.monthYearView.classList.remove("hidden") :
        this.context.monthYearView.classList.add("hidden");
    },

    /**
     * Use postMessage to pass the state of picker to the panel.
     */
    _dispatchState() {
      const { year, month, date } = this.state.dateKeeper.state;
      const { isYearSet, isMonthSet, isDateSet } = this.state;
      // The panel is listening to window for postMessage event, so we
      // do postMessage to itself to send data to input boxes.
      window.postMessage({
        name: "DatePickerPopupChanged",
        detail: {
          year,
          month,
          date,
          isYearSet,
          isMonthSet,
          isDateSet
        }
      }, "*");
    },

    /**
     * Attach event listeners
     */
    _attachEventListeners() {
      window.addEventListener("message", this);
      document.addEventListener("click", this);
    },

    /**
     * Handle events.
     *
     * @param  {Event} event
     */
    handleEvent(event) {
      switch (event.type) {
        case "message": {
          this.handleMessage(event);
          break;
        }
        case "click": {
          if (event.target == this.context.buttonLeft) {
            this.state.dateKeeper.setMonthByOffset(-1);
            this._update();
          } else if (event.target == this.context.buttonRight) {
            this.state.dateKeeper.setMonthByOffset(1);
            this._update();
          }
          break;
        }
      }
    },

    /**
     * Handle postMessage events.
     *
     * @param {Event} event
     */
    handleMessage(event) {
      switch (event.data.name) {
        case "DatePickerSetValue": {
          this.set(event.data.detail);
          break;
        }
        case "DatePickerInit": {
          this.init(event.data.detail);
          break;
        }
      }
    },

    /**
     * Set the date state and update the components with the new state.
     *
     * @param {Object} dateState
     *        {
     *          {Number} year [optional]
     *          {Number} month [optional]
     *          {Number} date [optional]
     *        }
     */
    set(dateState) {
      if (dateState.year != undefined) {
        this.state.isYearSet = true;
      }
      if (dateState.month != undefined) {
        this.state.isMonthSet = true;
      }
      if (dateState.date != undefined) {
        this.state.isDateSet = true;
      }

      this.state.dateKeeper.set(dateState);
      this._update();
    }
  };

  /**
   * MonthYear is a component that handles the month & year spinners
   *
   * @param {Object} options
   *        {
   *          {String} locale
   *          {Function} setYear
   *          {Function} setMonth
   *        }
   * @param {DOMElement} context
   */
  function MonthYear(options, context) {
    const spinnerSize = 5;
    const monthFormat = new Intl.DateTimeFormat(options.locale, { month: "short" }).format;
    const yearFormat = new Intl.DateTimeFormat(options.locale, { year: "numeric" }).format;
    const dateFormat = new Intl.DateTimeFormat(options.locale, { year: "numeric", month: "long" }).format;

    this.context = context;
    this.state = { dateFormat };
    this.props = {};
    this.components = {
      month: new Spinner({
        setValue: month => {
          this.state.isMonthSet = true;
          options.setMonth(month);
        },
        getDisplayString: month => monthFormat(new Date(0, month)),
        viewportSize: spinnerSize
      }, context.monthYearView),
      year: new Spinner({
        setValue: year => {
          this.state.isYearSet = true;
          options.setYear(year);
        },
        getDisplayString: year => yearFormat(new Date(new Date(0).setFullYear(year))),
        viewportSize: spinnerSize
      }, context.monthYearView)
    };

    this._attachEventListeners();
  };

  MonthYear.prototype = {

    /**
     * Set new properties and pass them to components
     *
     * @param {Object} props
     *        {
     *          {Boolean} isVisible
     *          {Date} dateObj
     *          {Number} month
     *          {Number} year
     *          {Array<Object>} months
     *          {Array<Object>} years
     *          {Function} toggleMonthPicker
     *         }
     */
    setProps(props) {
      this.context.monthYear.textContent = this.state.dateFormat(props.dateObj);

      if (props.isVisible) {
        this.components.month.setState({
          value: props.month,
          items: props.months,
          isInfiniteScroll: true,
          isValueSet: this.state.isMonthSet,
          smoothScroll: !this.state.firstOpened
        });
        this.components.year.setState({
          value: props.year,
          items: props.years,
          isInfiniteScroll: false,
          isValueSet: this.state.isYearSet,
          smoothScroll: !this.state.firstOpened
        });
        this.state.firstOpened = false;
      } else {
        this.state.isMonthSet = false;
        this.state.isYearSet = false;
        this.state.firstOpened = true;
      }

      this.props = Object.assign(this.props, props);
    },

    /**
     * Handle events
     * @param  {DOMEvent} event
     */
    handleEvent(event) {
      switch (event.type) {
        case "click": {
          this.props.toggleMonthPicker();
          break;
        }
      }
    },

    /**
     * Attach event listener to monthYear button
     */
    _attachEventListeners() {
      this.context.monthYear.addEventListener("click", this);
    }
  };
}
