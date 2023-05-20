/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* import-globals-from datekeeper.js */
/* import-globals-from calendar.js */
/* import-globals-from spinner.js */

"use strict";

function DatePicker(context) {
  this.context = context;
  this._attachEventListeners();
}

{
  const CAL_VIEW_SIZE = 42;

  DatePicker.prototype = {
    /**
     * Initializes the date picker. Set the default states and properties.
     * @param  {Object} props
     *         {
     *           {Number} year [optional]
     *           {Number} month [optional]
     *           {Number} date [optional]
     *           {Number} min
     *           {Number} max
     *           {Number} step
     *           {Number} stepBase
     *           {Number} firstDayOfWeek
     *           {Array<Number>} weekends
     *           {Array<String>} monthStrings
     *           {Array<String>} weekdayStrings
     *           {String} locale [optional]: User preferred locale
     *         }
     */
    init(props = {}) {
      this.props = props;
      this._setDefaultState();
      this._createComponents();
      this._update();
      this.components.calendar.focusDay();
      // TODO(bug 1828721): This is a bit sad.
      window.PICKER_READY = true;
      document.dispatchEvent(new CustomEvent("PickerReady"));
    },

    /*
     * Set initial date picker states.
     */
    _setDefaultState() {
      const {
        year,
        month,
        day,
        min,
        max,
        step,
        stepBase,
        firstDayOfWeek,
        weekends,
        monthStrings,
        weekdayStrings,
        locale,
        dir,
      } = this.props;
      const dateKeeper = new DateKeeper({
        year,
        month,
        day,
        min,
        max,
        step,
        stepBase,
        firstDayOfWeek,
        weekends,
        calViewSize: CAL_VIEW_SIZE,
      });

      document.dir = dir;

      this.state = {
        dateKeeper,
        locale,
        isMonthPickerVisible: false,
        datetimeOrders: new Intl.DateTimeFormat(locale)
          .formatToParts(new Date(0))
          .map(part => part.type),
        getDayString: day =>
          day ? new Intl.NumberFormat(locale).format(day) : "",
        getWeekHeaderString: weekday => weekdayStrings[weekday],
        getMonthString: month => monthStrings[month],
        setSelection: date => {
          dateKeeper.setSelection({
            year: date.getUTCFullYear(),
            month: date.getUTCMonth(),
            day: date.getUTCDate(),
          });
          this._update();
          this._dispatchState();
          this._closePopup();
        },
        setMonthByOffset: offset => {
          dateKeeper.setMonthByOffset(offset);
          this._update();
        },
        setYear: year => {
          dateKeeper.setYear(year);
          dateKeeper.setSelection({
            year,
            month: dateKeeper.selection.month,
            day: dateKeeper.selection.day,
          });
          this._update();
          this._dispatchState();
        },
        setMonth: month => {
          dateKeeper.setMonth(month);
          dateKeeper.setSelection({
            year: dateKeeper.selection.year,
            month,
            day: dateKeeper.selection.day,
          });
          this._update();
          this._dispatchState();
        },
        toggleMonthPicker: () => {
          this.state.isMonthPickerVisible = !this.state.isMonthPickerVisible;
          this._update();
        },
      };
    },

    /**
     * Initalize the date picker components.
     */
    _createComponents() {
      this.components = {
        calendar: new Calendar(
          {
            calViewSize: CAL_VIEW_SIZE,
            locale: this.state.locale,
            setSelection: this.state.setSelection,
            // Year and month could be changed without changing a selection
            setCalendarMonth: (year, month) => {
              this.state.dateKeeper.setCalendarMonth({
                year,
                month,
              });
              this._update();
            },
            getDayString: this.state.getDayString,
            getWeekHeaderString: this.state.getWeekHeaderString,
          },
          {
            weekHeader: this.context.weekHeader,
            daysView: this.context.daysView,
          }
        ),
        monthYear: new MonthYear(
          {
            setYear: this.state.setYear,
            setMonth: this.state.setMonth,
            getMonthString: this.state.getMonthString,
            datetimeOrders: this.state.datetimeOrders,
            locale: this.state.locale,
          },
          {
            monthYear: this.context.monthYear,
            monthYearView: this.context.monthYearView,
          }
        ),
      };
    },

    /**
     * Update date picker and its components.
     */
    _update(options = {}) {
      const { dateKeeper, isMonthPickerVisible } = this.state;

      const calendarEls = [
        this.context.buttonPrev,
        this.context.buttonNext,
        this.context.weekHeader.parentNode,
        this.context.buttonClear,
      ];
      // Update MonthYear state and toggle visibility for sighted users
      // and for assistive technology:
      this.context.monthYearView.hidden = !isMonthPickerVisible;
      for (let el of calendarEls) {
        el.hidden = isMonthPickerVisible;
      }
      this.context.monthYearNav.toggleAttribute(
        "monthPickerVisible",
        isMonthPickerVisible
      );
      if (isMonthPickerVisible) {
        this.state.months = dateKeeper.getMonths();
        this.state.years = dateKeeper.getYears();
      } else {
        this.state.days = dateKeeper.getDays();
      }

      this.components.monthYear.setProps({
        isVisible: isMonthPickerVisible,
        dateObj: dateKeeper.state.dateObj,
        months: this.state.months,
        years: this.state.years,
        toggleMonthPicker: this.state.toggleMonthPicker,
        noSmoothScroll: options.noSmoothScroll,
      });
      this.components.calendar.setProps({
        isVisible: !isMonthPickerVisible,
        days: this.state.days,
        weekHeaders: dateKeeper.state.weekHeaders,
      });
    },

    /**
     * Use postMessage to close the picker.
     */
    _closePopup(clear = false) {
      window.postMessage(
        {
          name: "ClosePopup",
          detail: clear,
        },
        "*"
      );
    },

    /**
     * Use postMessage to pass the state of picker to the panel.
     */
    _dispatchState() {
      const { year, month, day } = this.state.dateKeeper.selection;

      // The panel is listening to window for postMessage event, so we
      // do postMessage to itself to send data to input boxes.
      window.postMessage(
        {
          name: "PickerPopupChanged",
          detail: {
            year,
            month,
            day,
          },
        },
        "*"
      );
    },

    /**
     * Attach event listeners
     */
    _attachEventListeners() {
      window.addEventListener("message", this);
      document.addEventListener("mouseup", this, { passive: true });
      document.addEventListener("mousedown", this);
      document.addEventListener("keydown", this);
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
        case "keydown": {
          switch (event.key) {
            case "Enter":
            case " ":
            case "Escape": {
              // If the target is a toggle or a spinner on the month-year panel
              const isOnMonthPicker =
                this.context.monthYearView.parentNode.contains(event.target);

              if (this.state.isMonthPickerVisible && isOnMonthPicker) {
                // While a control on the month-year picker panel is focused,
                // keep the spinner's selection and close the month-year dialog
                event.stopPropagation();
                event.preventDefault();
                this.state.toggleMonthPicker();
                this.components.calendar.focusDay();
                break;
              }
              if (event.key == "Escape") {
                // Close the date picker on Escape from within the picker
                this._closePopup();
                break;
              }
              if (event.target == this.context.buttonPrev) {
                event.target.classList.add("active");
                this.state.setMonthByOffset(-1);
                this.context.buttonPrev.focus();
              } else if (event.target == this.context.buttonNext) {
                event.target.classList.add("active");
                this.state.setMonthByOffset(1);
                this.context.buttonNext.focus();
              } else if (event.target == this.context.buttonClear) {
                event.target.classList.add("active");
                this._closePopup(/* clear = */ true);
              }
              break;
            }
            case "Tab": {
              // Manage tab order of a daysView to prevent keyboard trap
              if (event.target.tagName === "td") {
                if (event.shiftKey) {
                  this.context.buttonNext.focus();
                } else if (!event.shiftKey) {
                  this.context.buttonClear.focus();
                }
                event.stopPropagation();
                event.preventDefault();
              }
              break;
            }
          }
          break;
        }
        case "mousedown": {
          // Use preventDefault to keep focus on input boxes
          event.preventDefault();
          event.target.setPointerCapture(event.pointerId);

          if (event.target == this.context.buttonClear) {
            event.target.classList.add("active");
            this._closePopup(/* clear = */ true);
          } else if (event.target == this.context.buttonPrev) {
            event.target.classList.add("active");
            this.state.dateKeeper.setMonthByOffset(-1);
            this._update();
          } else if (event.target == this.context.buttonNext) {
            event.target.classList.add("active");
            this.state.dateKeeper.setMonthByOffset(1);
            this._update();
          }
          break;
        }
        case "mouseup": {
          event.target.releasePointerCapture(event.pointerId);

          if (
            event.target == this.context.buttonPrev ||
            event.target == this.context.buttonNext
          ) {
            event.target.classList.remove("active");
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
        case "PickerSetValue": {
          this.set(event.data.detail);
          break;
        }
        case "PickerInit": {
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
    set({ year, month, day }) {
      if (!this.state) {
        return;
      }

      const { dateKeeper } = this.state;

      dateKeeper.setCalendarMonth({
        year,
        month,
      });
      dateKeeper.setSelection({
        year,
        month,
        day,
      });
      this._update({ noSmoothScroll: true });
    },
  };

  /**
   * MonthYear is a component that handles the month & year spinners
   *
   * @param {Object} options
   *        {
   *          {String} locale
   *          {Function} setYear
   *          {Function} setMonth
   *          {Function} getMonthString
   *          {Array<String>} datetimeOrders
   *        }
   * @param {DOMElement} context
   */
  function MonthYear(options, context) {
    const spinnerSize = 5;
    const yearFormat = new Intl.DateTimeFormat(options.locale, {
      year: "numeric",
      timeZone: "UTC",
    }).format;
    const dateFormat = new Intl.DateTimeFormat(options.locale, {
      year: "numeric",
      month: "long",
      timeZone: "UTC",
    }).format;
    const spinnerOrder =
      options.datetimeOrders.indexOf("month") <
      options.datetimeOrders.indexOf("year")
        ? "order-month-year"
        : "order-year-month";

    context.monthYearView.classList.add(spinnerOrder);

    this.context = context;
    this.state = { dateFormat };
    this.props = {};
    this.components = {
      month: new Spinner(
        {
          id: "spinner-month",
          setValue: month => {
            this.state.isMonthSet = true;
            options.setMonth(month);
          },
          getDisplayString: options.getMonthString,
          viewportSize: spinnerSize,
        },
        context.monthYearView
      ),
      year: new Spinner(
        {
          id: "spinner-year",
          setValue: year => {
            this.state.isYearSet = true;
            options.setYear(year);
          },
          getDisplayString: year =>
            yearFormat(new Date(new Date(0).setUTCFullYear(year))),
          viewportSize: spinnerSize,
        },
        context.monthYearView
      ),
    };

    this._updateButtonLabels();
    this._attachEventListeners();
  }

  MonthYear.prototype = {
    /**
     * Set new properties and pass them to components
     *
     * @param {Object} props
     *        {
     *          {Boolean} isVisible
     *          {Date} dateObj
     *          {Array<Object>} months
     *          {Array<Object>} years
     *          {Function} toggleMonthPicker
     *         }
     */
    setProps(props) {
      this.context.monthYear.textContent = this.state.dateFormat(props.dateObj);
      const spinnerDialog = this.context.monthYearView.parentNode;

      if (props.isVisible) {
        this.context.monthYear.classList.add("active");
        this.context.monthYear.setAttribute("aria-expanded", "true");
        // To prevent redundancy, as spinners will announce their value on change
        this.context.monthYear.setAttribute("aria-live", "off");
        this.components.month.setState({
          value: props.dateObj.getUTCMonth(),
          items: props.months,
          isInfiniteScroll: true,
          isValueSet: this.state.isMonthSet,
          smoothScroll: !(this.state.firstOpened || props.noSmoothScroll),
        });
        this.components.year.setState({
          value: props.dateObj.getUTCFullYear(),
          items: props.years,
          isInfiniteScroll: false,
          isValueSet: this.state.isYearSet,
          smoothScroll: !(this.state.firstOpened || props.noSmoothScroll),
        });
        this.state.firstOpened = false;

        // Set up spinner dialog container properties for assistive technology:
        spinnerDialog.setAttribute("role", "dialog");
        spinnerDialog.setAttribute("aria-modal", "true");
      } else {
        this.context.monthYear.classList.remove("active");
        this.context.monthYear.setAttribute("aria-expanded", "false");
        // To ensure calendar month's changes are announced:
        this.context.monthYear.setAttribute("aria-live", "polite");
        // Remove spinner dialog container properties to ensure this hidden
        // modal will be ignored by assistive technology, because even though
        // the dialog is hidden, the toggle button is a visible descendant,
        // so we must not treat its container as a dialog:
        spinnerDialog.removeAttribute("role");
        spinnerDialog.removeAttribute("aria-modal");
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
        case "keydown": {
          if (event.key === "Enter" || event.key === " ") {
            event.stopPropagation();
            event.preventDefault();
            this.props.toggleMonthPicker();
          }
          break;
        }
      }
    },

    /**
     * Update localizable IDs of the spinner and its Prev/Next buttons
     */
    _updateButtonLabels() {
      document.l10n.setAttributes(
        this.components.month.elements.spinner,
        "date-spinner-month"
      );
      document.l10n.setAttributes(
        this.components.year.elements.spinner,
        "date-spinner-year"
      );
      document.l10n.setAttributes(
        this.components.month.elements.up,
        "date-spinner-month-previous"
      );
      document.l10n.setAttributes(
        this.components.month.elements.down,
        "date-spinner-month-next"
      );
      document.l10n.setAttributes(
        this.components.year.elements.up,
        "date-spinner-year-previous"
      );
      document.l10n.setAttributes(
        this.components.year.elements.down,
        "date-spinner-year-next"
      );
      document.l10n.translateRoots();
    },

    /**
     * Attach event listener to monthYear button
     */
    _attachEventListeners() {
      this.context.monthYear.addEventListener("click", this);
      this.context.monthYear.addEventListener("keydown", this);
    },
  };
}
