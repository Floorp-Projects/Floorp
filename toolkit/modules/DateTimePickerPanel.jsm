/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["DateTimePickerPanel"];

var DateTimePickerPanel = class {
  constructor(element) {
    this.element = element;

    this.TIME_PICKER_WIDTH = "12em";
    this.TIME_PICKER_HEIGHT = "21em";
    this.DATE_PICKER_WIDTH = "23.1em";
    this.DATE_PICKER_HEIGHT = "20.7em";
  }

  get dateTimePopupFrame() {
    let frame = this.element.querySelector("#dateTimePopupFrame");
    if (!frame) {
      frame = this.element.ownerDocument.createXULElement("iframe");
      frame.id = "dateTimePopupFrame";
      this.element.appendChild(frame);
    }
    return frame;
  }

  openPicker(type, rect, detail) {
    if (type == "datetime-local") {
      type = "date";
    }
    this.type = type;
    this.pickerState = {};
    // TODO: Resize picker according to content zoom level
    this.element.style.fontSize = "10px";
    switch (type) {
      case "time": {
        this.detail = detail;
        this.dateTimePopupFrame.addEventListener("load", this, true);
        this.dateTimePopupFrame.setAttribute(
          "src",
          "chrome://global/content/timepicker.xhtml"
        );
        this.dateTimePopupFrame.style.width = this.TIME_PICKER_WIDTH;
        this.dateTimePopupFrame.style.height = this.TIME_PICKER_HEIGHT;
        break;
      }
      case "date": {
        this.detail = detail;
        this.dateTimePopupFrame.addEventListener("load", this, true);
        this.dateTimePopupFrame.setAttribute(
          "src",
          "chrome://global/content/datepicker.xhtml"
        );
        this.dateTimePopupFrame.style.width = this.DATE_PICKER_WIDTH;
        this.dateTimePopupFrame.style.height = this.DATE_PICKER_HEIGHT;
        break;
      }
    }
    this.element.openPopupAtScreenRect(
      "after_start",
      rect.left,
      rect.top,
      rect.width,
      rect.height,
      false,
      false
    );
  }

  closePicker() {
    this.setInputBoxValue(true);
    this.pickerState = {};
    this.type = undefined;
    this.dateTimePopupFrame.removeEventListener("load", this, true);
    this.dateTimePopupFrame.contentDocument.removeEventListener(
      "message",
      this
    );
    this.dateTimePopupFrame.setAttribute("src", "");
    this.element.hidePopup();
  }

  setPopupValue(data) {
    switch (this.type) {
      case "time": {
        this.postMessageToPicker({
          name: "PickerSetValue",
          detail: data.value,
        });
        break;
      }
      case "date": {
        const { year, month, day } = data.value;
        this.postMessageToPicker({
          name: "PickerSetValue",
          detail: {
            year,
            // Month value from input box starts from 1 instead of 0
            month: month == undefined ? undefined : month - 1,
            day,
          },
        });
        break;
      }
    }
  }

  initPicker(detail) {
    let locale = new Services.intl.Locale(
      Services.locale.webExposedLocales[0],
      {
        calendar: "gregory",
      }
    ).toString();

    // Workaround for bug 1418061, while we wait for resolution of
    // http://bugs.icu-project.org/trac/ticket/13592: drop the PT region code,
    // because it results in "abbreviated" day names that are too long;
    // the region-less "pt" locale has shorter forms that are better here.
    locale = locale.replace(/^pt-PT/i, "pt");

    const dir = Services.locale.isAppLocaleRTL ? "rtl" : "ltr";

    switch (this.type) {
      case "time": {
        const { hour, minute } = detail.value;
        const format = detail.format || "12";

        this.postMessageToPicker({
          name: "PickerInit",
          detail: {
            hour,
            minute,
            format,
            locale,
            min: detail.min,
            max: detail.max,
            step: detail.step,
          },
        });
        break;
      }
      case "date": {
        const { year, month, day } = detail.value;
        const { firstDayOfWeek, weekends } = this.getCalendarInfo(locale);

        const monthDisplayNames = new Services.intl.DisplayNames(locale, {
          type: "month",
          style: "short",
          calendar: "gregory",
        });
        const monthStrings = [
          1,
          2,
          3,
          4,
          5,
          6,
          7,
          8,
          9,
          10,
          11,
          12,
        ].map(month => monthDisplayNames.of(month));

        const weekdayDisplayNames = new Services.intl.DisplayNames(locale, {
          type: "weekday",
          style: "abbreviated",
          calendar: "gregory",
        });
        const weekdayStrings = [
          // Weekdays starting Sunday (7) to Saturday (6).
          7,
          1,
          2,
          3,
          4,
          5,
          6,
        ].map(weekday => weekdayDisplayNames.of(weekday));

        this.postMessageToPicker({
          name: "PickerInit",
          detail: {
            year,
            // Month value from input box starts from 1 instead of 0
            month: month == undefined ? undefined : month - 1,
            day,
            firstDayOfWeek,
            weekends,
            monthStrings,
            weekdayStrings,
            locale,
            dir,
            min: detail.min,
            max: detail.max,
            step: detail.step,
            stepBase: detail.stepBase,
          },
        });
        break;
      }
    }
  }

  /**
   * @param {Boolean} passAllValues: Pass spinner values regardless if they've been set/changed or not
   */
  setInputBoxValue(passAllValues) {
    switch (this.type) {
      case "time": {
        const {
          hour,
          minute,
          isHourSet,
          isMinuteSet,
          isDayPeriodSet,
        } = this.pickerState;
        const isAnyValueSet = isHourSet || isMinuteSet || isDayPeriodSet;
        if (passAllValues && isAnyValueSet) {
          this.sendPickerValueChanged({ hour, minute });
        } else {
          this.sendPickerValueChanged({
            hour: isHourSet || isDayPeriodSet ? hour : undefined,
            minute: isMinuteSet ? minute : undefined,
          });
        }
        break;
      }
      case "date": {
        this.sendPickerValueChanged(this.pickerState);
        break;
      }
    }
  }

  sendPickerValueChanged(value) {
    switch (this.type) {
      case "time": {
        this.element.dispatchEvent(
          new CustomEvent("DateTimePickerValueChanged", {
            detail: {
              hour: value.hour,
              minute: value.minute,
            },
          })
        );
        break;
      }
      case "date": {
        this.element.dispatchEvent(
          new CustomEvent("DateTimePickerValueChanged", {
            detail: {
              year: value.year,
              // Month value from input box starts from 1 instead of 0
              month: value.month == undefined ? undefined : value.month + 1,
              day: value.day,
            },
          })
        );
        break;
      }
    }
  }

  getCalendarInfo(locale) {
    const calendarInfo = Services.intl.getCalendarInfo(locale);

    // Day of week from calendarInfo starts from 1 as Monday to 7 as Sunday,
    // so they need to be mapped to JavaScript convention with 0 as Sunday
    // and 6 as Saturday
    function toDateWeekday(day) {
      return day === 7 ? 0 : day;
    }

    let firstDayOfWeek = toDateWeekday(calendarInfo.firstDayOfWeek),
      weekend = calendarInfo.weekend;

    let weekends = weekend.map(toDateWeekday);

    return {
      firstDayOfWeek,
      weekends,
    };
  }

  handleEvent(aEvent) {
    switch (aEvent.type) {
      case "load": {
        this.initPicker(this.detail);
        this.dateTimePopupFrame.contentWindow.addEventListener("message", this);
        break;
      }
      case "message": {
        this.handleMessage(aEvent);
        break;
      }
    }
  }

  handleMessage(aEvent) {
    if (
      !this.dateTimePopupFrame.contentDocument.nodePrincipal.isSystemPrincipal
    ) {
      return;
    }

    switch (aEvent.data.name) {
      case "PickerPopupChanged": {
        this.pickerState = aEvent.data.detail;
        this.setInputBoxValue();
        break;
      }
      case "ClosePopup": {
        this.closePicker();
        break;
      }
    }
  }

  postMessageToPicker(data) {
    if (
      this.dateTimePopupFrame.contentDocument.nodePrincipal.isSystemPrincipal
    ) {
      this.dateTimePopupFrame.contentWindow.postMessage(data, "*");
    }
  }
};
