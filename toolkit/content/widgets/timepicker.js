/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

function TimePicker(context) {
  this.context = context;
  this._attachEventListeners();
}

{
  const debug = 0 ? console.log.bind(console, "[timepicker]") : function() {};

  const DAY_PERIOD_IN_HOURS = 12,
        SECOND_IN_MS = 1000,
        MINUTE_IN_MS = 60000,
        DAY_IN_MS = 86400000;

  TimePicker.prototype = {
    /**
     * Initializes the time picker. Set the default states and properties.
     * @param  {Object} props
     *         {
     *           {Number} hour [optional]: Hour in 24 hours format (0~23), default is current hour
     *           {Number} minute [optional]: Minute (0~59), default is current minute
     *           {String} min [optional]: Minimum time, in 24 hours format. ex: "05:45"
     *           {String} max [optional]: Maximum time, in 24 hours format. ex: "23:00"
     *           {Number} step [optional]: Step size in minutes. Default is 60.
     *           {String} format [optional]: "12" for 12 hours, "24" for 24 hours format
     *           {String} locale [optional]: User preferred locale
     *         }
     */
    init(props) {
      this.props = props || {};
      this._setDefaultState();
      this._createComponents();
      this._setComponentStates();
    },

    /*
     * Set initial time states. If there's no hour & minute, it will
     * use the current time. The Time module keeps track of the time states,
     * and calculates the valid options given the time, min, max, step,
     * and format (12 or 24).
     */
    _setDefaultState() {
      const { hour, minute, min, max, step, format } = this.props;
      const now = new Date();

      let timerHour = hour == undefined ? now.getHours() : hour;
      let timerMinute = minute == undefined ? now.getMinutes() : minute;

      // The spec defines 1 step == 1 second, need to convert to ms for timekeeper
      let timeKeeper = new TimeKeeper({
        min: this._parseTimeString(min) || new Date(0),
        max: this._parseTimeString(max) || new Date(DAY_IN_MS - 1),
        stepInMs: step ? step * SECOND_IN_MS : MINUTE_IN_MS,
        format: format || "12"
      });
      timeKeeper.setState({ hour: timerHour, minute: timerMinute });

      this.state = { timeKeeper };
    },

    /**
     * Convert a time string from DOM attribute to a date object.
     *
     * @param  {String} timeString: (ex. "10:30", "23:55", "12:34:56.789")
     * @return {Date/Boolean} Date object or false if date is invalid.
     */
    _parseTimeString(timeString) {
      let time = new Date("1970-01-01T" + timeString + "Z");
      return time.toString() == "Invalid Date" ? false : time;
    },

    /**
     * Initalize the spinner components.
     */
    _createComponents() {
      const { locale, step, format } = this.props;
      const { timeKeeper } = this.state;

      const wrapSetValueFn = (setTimeFunction) => {
        return (value) => {
          setTimeFunction(value);
          this._setComponentStates();
          this._dispatchState();
        };
      };
      const numberFormat = new Intl.NumberFormat(locale).format;

      this.components = {
        hour: new Spinner({
          setValue: wrapSetValueFn(value => {
            timeKeeper.setHour(value);
            this.state.isHourSet = true;
          }),
          getDisplayString: hour => {
            if (format == "24") {
              return numberFormat(hour);
            }
            // Hour 0 in 12 hour format is displayed as 12.
            const hourIn12 = hour % DAY_PERIOD_IN_HOURS;
            return hourIn12 == 0 ? numberFormat(12)
              : numberFormat(hourIn12);
          }
        }, this.context),
        minute: new Spinner({
          setValue: wrapSetValueFn(value => {
            timeKeeper.setMinute(value);
            this.state.isMinuteSet = true;
          }),
          getDisplayString: minute => numberFormat(minute)
        }, this.context)
      };

      this._insertLayoutElement({
        tag: "div",
        textContent: ":",
        className: "colon",
        insertBefore: this.components.minute.elements.container
      });

      // The AM/PM spinner is only available in 12hr mode
      // TODO: Replace AM & PM string with localized string
      if (format == "12") {
        this.components.dayPeriod = new Spinner({
          setValue: wrapSetValueFn(value => {
            timeKeeper.setDayPeriod(value);
            this.state.isDayPeriodSet = true;
          }),
          getDisplayString: dayPeriod => dayPeriod == 0 ? "AM" : "PM",
          hideButtons: true
        }, this.context);

        this._insertLayoutElement({
          tag: "div",
          className: "spacer",
          insertBefore: this.components.dayPeriod.elements.container
        });
      }
    },

    /**
     * Insert element for layout purposes.
     *
     * @param {Object}
     *        {
     *          {String} tag: The tag to create
     *          {DOMElement} insertBefore: The DOM node to insert before
     *          {String} className [optional]: Class name
     *          {String} textContent [optional]: Text content
     *        }
     */
    _insertLayoutElement({ tag, insertBefore, className, textContent }) {
      let el = document.createElement(tag);
      el.textContent = textContent;
      el.className = className;
      this.context.insertBefore(el, insertBefore);
    },

    /**
     * Set component states.
     */
    _setComponentStates() {
      const { timeKeeper, isHourSet, isMinuteSet, isDayPeriodSet } = this.state;
      const isInvalid = timeKeeper.state.isInvalid;
      // Value is set to min if it's first opened and time state is invalid
      const setToMinValue = !isHourSet && !isMinuteSet && !isDayPeriodSet && isInvalid;

      this.components.hour.setState({
        value: setToMinValue ? timeKeeper.ranges.hours[0].value : timeKeeper.hour,
        items: timeKeeper.ranges.hours,
        isInfiniteScroll: true,
        isValueSet: isHourSet,
        isInvalid
      });

      this.components.minute.setState({
        value: setToMinValue ? timeKeeper.ranges.minutes[0].value : timeKeeper.minute,
        items: timeKeeper.ranges.minutes,
        isInfiniteScroll: true,
        isValueSet: isMinuteSet,
        isInvalid
      });

      // The AM/PM spinner is only available in 12hr mode
      if (this.props.format == "12") {
        this.components.dayPeriod.setState({
          value: setToMinValue ? timeKeeper.ranges.dayPeriod[0].value : timeKeeper.dayPeriod,
          items: timeKeeper.ranges.dayPeriod,
          isInfiniteScroll: false,
          isValueSet: isDayPeriodSet,
          isInvalid
        });
      }
    },

    /**
     * Dispatch CustomEvent to pass the state of picker to the panel.
     */
    _dispatchState() {
      const { hour, minute } = this.state.timeKeeper;
      const { isHourSet, isMinuteSet, isDayPeriodSet } = this.state;
      // The panel is listening to window for postMessage event, so we
      // do postMessage to itself to send data to input boxes.
      window.postMessage({
        name: "TimePickerPopupChanged",
        detail: {
          hour,
          minute,
          isHourSet,
          isMinuteSet,
          isDayPeriodSet
        }
      }, "*");
    },
    _attachEventListeners() {
      window.addEventListener("message", this);
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
      }
    },

    /**
     * Handle postMessage events.
     *
     * @param {Event} event
     */
    handleMessage(event) {
      switch (event.data.name) {
        case "TimePickerSetValue": {
          this.set(event.data.detail);
          break;
        }
        case "TimePickerInit": {
          this.init(event.data.detail);
          break;
        }
      }
    },

    /**
     * Set the time state and update the components with the new state.
     *
     * @param {Object} timeState
     *        {
     *          {Number} hour [optional]
     *          {Number} minute [optional]
     *          {Number} second [optional]
     *          {Number} millisecond [optional]
     *        }
     */
    set(timeState) {
      if (timeState.hour != undefined) {
        this.state.isHourSet = true;
      }
      if (timeState.minute != undefined) {
        this.state.isMinuteSet = true;
      }
      this.state.timeKeeper.setState(timeState);
      this._setComponentStates();
    }
  };
}
