/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// This is a UA widget. It runs in per-origin UA widget scope,
// to be loaded by UAWidgetsChild.jsm.

/*
 * This is the class of entry. It will construct the actual implementation
 * according to the value of the "type" property.
 */
this.DateTimeBoxWidget = class {
  constructor(shadowRoot) {
    this.shadowRoot = shadowRoot;
    this.element = shadowRoot.host;
    this.document = this.element.ownerDocument;
    this.window = this.document.defaultView;
  }

  /*
   * Callback called by UAWidgets right after constructor.
   */
  onsetup() {
    this.onchange(/* aDestroy = */ false);
  }

  /*
   * Callback called by UAWidgets when the "type" property changes.
   */
  onchange(aDestroy = true) {
    let newType = this.element.type;
    if (this.type == newType) {
      return;
    }

    if (aDestroy) {
      this.destructor();
    }
    this.type = newType;
    this.setup();
  }

  shouldShowTime() {
    return this.type == "time" || this.type == "datetime-local";
  }

  shouldShowDate() {
    return this.type == "date" || this.type == "datetime-local";
  }

  destructor() {
    this.mResetButton.addEventListener("mousedown", this, {
      mozSystemGroup: true,
    });

    this.mInputElement.removeEventListener("keypress", this, {
      capture: true,
      mozSystemGroup: true,
    });
    this.mInputElement.removeEventListener("click", this, {
      mozSystemGroup: true,
    });

    this.CONTROL_EVENTS.forEach(eventName => {
      this.mDateTimeBoxElement.removeEventListener(eventName, this);
    });

    this.removeEditFields();

    this.mInputElement = null;

    this.shadowRoot.firstChild.remove();
  }

  removeEditFields() {
    this.removeEventListenersToField(this.mYearField);
    this.removeEventListenersToField(this.mMonthField);
    this.removeEventListenersToField(this.mDayField);
    this.removeEventListenersToField(this.mHourField);
    this.removeEventListenersToField(this.mMinuteField);
    this.removeEventListenersToField(this.mSecondField);
    this.removeEventListenersToField(this.mMillisecField);
    this.removeEventListenersToField(this.mDayPeriodField);

    this.mYearField = null;
    this.mMonthField = null;
    this.mDayField = null;
    this.mHourField = null;
    this.mMinuteField = null;
    this.mSecondField = null;
    this.mMillisecField = null;
    this.mDayPeriodField = null;

    let root = this.shadowRoot.getElementById("edit-wrapper");
    while (root.firstChild) {
      root.firstChild.remove();
    }
  }

  rebuildEditFieldsIfNeeded() {
    if (
      this.shouldShowSecondField() == !!this.mSecondField &&
      this.shouldShowMillisecField() == !!this.mMillisecField
    ) {
      return;
    }

    let focused = this.mInputElement.matches(":focus");

    this.removeEditFields();
    this.buildEditFields();

    if (focused) {
      this.mInputElement.focus();
    }
  }

  setup() {
    this.DEBUG = false;

    this.generateContent();

    this.mDateTimeBoxElement = this.shadowRoot.firstChild;
    this.mInputElement = this.element;
    this.mLocales = this.window.getWebExposedLocales();

    this.mIsRTL = false;
    let intlUtils = this.window.intlUtils;
    if (intlUtils) {
      this.mIsRTL = intlUtils.isAppLocaleRTL();
    }

    if (this.mIsRTL) {
      let inputBoxWrapper = this.shadowRoot.getElementById("input-box-wrapper");
      inputBoxWrapper.dir = "rtl";
    }

    this.mIsPickerOpen = false;

    this.mMinMonth = 1;
    this.mMaxMonth = 12;
    this.mMinDay = 1;
    this.mMaxDay = 31;
    this.mMinYear = 1;
    // Maximum year limited by ECMAScript date object range, year <= 275760.
    this.mMaxYear = 275760;
    this.mMonthDayLength = 2;
    this.mYearLength = 4;
    this.mMonthPageUpDownInterval = 3;
    this.mDayPageUpDownInterval = 7;
    this.mYearPageUpDownInterval = 10;

    const kDefaultAMString = "AM";
    const kDefaultPMString = "PM";

    let { amString, pmString } = this.getStringsForLocale(this.mLocales);

    this.mAMIndicator = amString || kDefaultAMString;
    this.mPMIndicator = pmString || kDefaultPMString;

    this.mHour12 = this.is12HourTime(this.mLocales);
    this.mMillisecSeparatorText = ".";
    this.mMaxLength = 2;
    this.mMillisecMaxLength = 3;
    this.mDefaultStep = 60 * 1000; // in milliseconds

    this.mMinHour = this.mHour12 ? 1 : 0;
    this.mMaxHour = this.mHour12 ? 12 : 23;
    this.mMinMinute = 0;
    this.mMaxMinute = 59;
    this.mMinSecond = 0;
    this.mMaxSecond = 59;
    this.mMinMillisecond = 0;
    this.mMaxMillisecond = 999;

    this.mHourPageUpDownInterval = 3;
    this.mMinSecPageUpDownInterval = 10;

    this.mResetButton = this.shadowRoot.getElementById("reset-button");
    this.mResetButton.style.visibility = "hidden";
    this.mResetButton.addEventListener("mousedown", this, {
      mozSystemGroup: true,
    });

    this.mInputElement.addEventListener(
      "keypress",
      this,
      {
        capture: true,
        mozSystemGroup: true,
      },
      false
    );
    // This is to open the picker when input element is clicked (this
    // includes padding area).
    this.mInputElement.addEventListener(
      "click",
      this,
      { mozSystemGroup: true },
      false
    );

    // Those events are dispatched to <div class="datetimebox"> with bubble set
    // to false. They are trapped inside UA Widget Shadow DOM and are not
    // dispatched to the document.
    this.CONTROL_EVENTS.forEach(eventName => {
      this.mDateTimeBoxElement.addEventListener(eventName, this, {}, false);
    });

    this.buildEditFields();
    this.updateEditAttributes();

    if (this.mInputElement.value) {
      this.setFieldsFromInputValue();
    }
  }

  generateContent() {
    /*
     * Pass the markup through XML parser purely for the reason of loading the localization DTD.
     * Remove it when migrate to Fluent (bug 1504363).
     */
    const parser = new this.window.DOMParser();
    parser.forceEnableDTD();
    let parserDoc = parser.parseFromString(
      `<!DOCTYPE bindings [
      <!ENTITY % datetimeboxDTD SYSTEM "chrome://global/locale/datetimebox.dtd">
      %datetimeboxDTD;
      ]>
      <div class="datetimebox" xmlns="http://www.w3.org/1999/xhtml" role="none">
        <link rel="stylesheet" type="text/css" href="chrome://global/content/bindings/datetimebox.css" />
        <div class="datetime-input-box-wrapper" id="input-box-wrapper" role="presentation">
          <span class="datetime-input-edit-wrapper"
                     id="edit-wrapper">
            <!-- Each of the date/time input types will append their input child
               - elements here -->
          </span>

          <button class="datetime-reset-button" id="reset-button" tabindex="-1" aria-label="&datetime.reset.label;">
            <svg xmlns="http://www.w3.org/2000/svg" class="datetime-reset-button-svg" width="12" height="12" viewBox="0 0 12 12">
              <path d="M 3.9,3 3,3.9 5.1,6 3,8.1 3.9,9 6,6.9 8.1,9 9,8.1 6.9,6 9,3.9 8.1,3 6,5.1 Z M 12,6 A 6,6 0 0 1 6,12 6,6 0 0 1 0,6 6,6 0 0 1 6,0 6,6 0 0 1 12,6 Z"/>
            </svg>
          </button>
        </div>
        <div id="strings"
          data-m-year-place-holder="&date.year.placeholder;"
          data-m-year-label="&date.year.label;"
          data-m-month-place-holder="&date.month.placeholder;"
          data-m-month-label="&date.month.label;"
          data-m-day-place-holder="&date.day.placeholder;"
          data-m-day-label="&date.day.label;"

          data-m-hour-place-holder="&time.hour.placeholder;"
          data-m-hour-label="&time.hour.label;"
          data-m-minute-place-holder="&time.minute.placeholder;"
          data-m-minute-label="&time.minute.label;"
          data-m-second-place-holder="&time.second.placeholder;"
          data-m-second-label="&time.second.label;"
          data-m-millisecond-place-holder="&time.millisecond.placeholder;"
          data-m-millisecond-label="&time.millisecond.label;"
          data-m-day-period-place-holder="&time.dayperiod.placeholder;"
          data-m-day-period-label="&time.dayperiod.label;"
        ></div>
      </div>`,
      "application/xml"
    );

    /*
     * The <div id="strings"> is also parsed in the document so that there is no
     * need to create another XML document just to get the strings.
     */
    let stringsElement = parserDoc.getElementById("strings");
    stringsElement.remove();
    for (let key in stringsElement.dataset) {
      // key will be camelCase version of the attribute key above,
      // like mYearPlaceHolder.
      this[key] = stringsElement.dataset[key];
    }

    this.shadowRoot.importNodeAndAppendChildAt(
      this.shadowRoot,
      parserDoc.documentElement,
      true
    );
  }

  get FIELD_EVENTS() {
    return ["focus", "blur", "copy", "cut", "paste"];
  }

  get CONTROL_EVENTS() {
    return [
      "MozDateTimeValueChanged",
      "MozNotifyMinMaxStepAttrChanged",
      "MozDateTimeAttributeChanged",
      "MozPickerValueChanged",
      "MozSetDateTimePickerState",
    ];
  }

  addEventListenersToField(aElement) {
    // These events don't bubble out of the Shadow DOM, so we'll have to add
    // event listeners specifically on each of the fields, not just
    // on the <input>
    this.FIELD_EVENTS.forEach(eventName => {
      aElement.addEventListener(
        eventName,
        this,
        { mozSystemGroup: true },
        false
      );
    });
  }

  removeEventListenersToField(aElement) {
    if (!aElement) {
      return;
    }

    this.FIELD_EVENTS.forEach(eventName => {
      aElement.removeEventListener(eventName, this, { mozSystemGroup: true });
    });
  }

  log(aMsg) {
    if (this.DEBUG) {
      this.window.dump("[DateTimeBox] " + aMsg + "\n");
    }
  }

  createEditFieldAndAppend(
    aPlaceHolder,
    aLabel,
    aIsNumeric,
    aMinDigits,
    aMaxLength,
    aMinValue,
    aMaxValue,
    aPageUpDownInterval
  ) {
    let root = this.shadowRoot.getElementById("edit-wrapper");
    let field = this.shadowRoot.createElementAndAppendChildAt(root, "span");
    field.classList.add("datetime-edit-field");
    field.textContent = aPlaceHolder;
    field.placeholder = aPlaceHolder;
    field.setAttribute("aria-valuetext", "");
    field.tabIndex = this.editFieldTabIndex();

    field.setAttribute("readonly", this.mInputElement.readOnly);
    field.setAttribute("disabled", this.mInputElement.disabled);
    // Set property as well for convenience.
    field.disabled = this.mInputElement.disabled;
    field.readOnly = this.mInputElement.readOnly;
    field.setAttribute("aria-label", aLabel);

    // Used to store the non-formatted value, cleared when value is
    // cleared.
    // DateTimeInputTypeBase::HasBadInput() will read this to decide
    // if the input has value.
    field.setAttribute("value", "");

    if (aIsNumeric) {
      field.classList.add("numeric");
      // Maximum value allowed.
      field.setAttribute("min", aMinValue);
      // Minumim value allowed.
      field.setAttribute("max", aMaxValue);
      // Interval when pressing pageUp/pageDown key.
      field.setAttribute("pginterval", aPageUpDownInterval);
      // Used to store what the user has already typed in the field,
      // cleared when value is cleared and when field is blurred.
      field.setAttribute("typeBuffer", "");
      // Minimum digits to display, padded with leading 0s.
      field.setAttribute("mindigits", aMinDigits);
      // Maximum length for the field, will be advance to the next field
      // automatically if exceeded.
      field.setAttribute("maxlength", aMaxLength);
      // Set spinbutton ARIA role
      field.setAttribute("role", "spinbutton");

      if (this.mIsRTL) {
        // Force the direction to be "ltr", so that the field stays in the
        // same order even when it's empty (with placeholder). By using
        // "embed", the text inside the element is still displayed based
        // on its directionality.
        field.style.unicodeBidi = "embed";
        field.style.direction = "ltr";
      }
    } else {
      // Set generic textbox ARIA role
      field.setAttribute("role", "textbox");
    }

    return field;
  }

  updateResetButtonVisibility() {
    if (this.isAnyFieldAvailable(false) && !this.isRequired()) {
      this.mResetButton.style.visibility = "";
    } else {
      this.mResetButton.style.visibility = "hidden";
    }
  }

  notifyInputElementValueChanged() {
    this.log("inputElementValueChanged");
    this.setFieldsFromInputValue();
  }

  notifyMinMaxStepAttrChanged() {
    // Second and millisecond part are optional, rebuild edit fields if
    // needed.
    this.rebuildEditFieldsIfNeeded();
    // Fill in values again.
    this.setFieldsFromInputValue();
  }

  setValueFromPicker(aValue) {
    this.setFieldsFromPicker(aValue);
  }

  advanceToNextField(aReverse) {
    this.log("advanceToNextField");

    let focusedInput = this.mLastFocusedField;
    let next = aReverse
      ? focusedInput.previousElementSibling
      : focusedInput.nextElementSibling;
    if (!next && !aReverse) {
      this.setInputValueFromFields();
      return;
    }

    while (next) {
      if (next.matches("span.datetime-edit-field")) {
        next.focus();
        break;
      }
      next = aReverse ? next.previousElementSibling : next.nextElementSibling;
    }
  }

  setPickerState(aIsOpen) {
    this.log("picker is now " + (aIsOpen ? "opened" : "closed"));
    this.mIsPickerOpen = aIsOpen;
  }

  editFieldTabIndex() {
    if (this.mInputElement.disabled) {
      return -1;
    }
    return this.mInputElement.tabIndex;
  }

  updateEditAttributes() {
    this.log("updateEditAttributes");

    let editRoot = this.shadowRoot.getElementById("edit-wrapper");

    for (let child of editRoot.querySelectorAll(
      ":scope > span.datetime-edit-field"
    )) {
      // "disabled" and "readonly" must be set as attributes because they
      // are not defined properties of HTMLSpanElement, and the stylesheet
      // checks the literal string attribute values.
      child.setAttribute("disabled", this.mInputElement.disabled);
      child.setAttribute("readonly", this.mInputElement.readOnly);

      // Set property as well for convenience.
      child.disabled = this.mInputElement.disabled;
      child.readOnly = this.mInputElement.readOnly;

      // tabIndex as a property works on all relevant elements.
      child.tabIndex = this.editFieldTabIndex();
    }

    this.mResetButton.disabled =
      this.mInputElement.disabled || this.mInputElement.readOnly;
    this.updateResetButtonVisibility();
  }

  isEmpty(aValue) {
    return aValue == undefined || 0 === aValue.length;
  }

  getFieldValue(aField) {
    if (!aField || !aField.classList.contains("numeric")) {
      return undefined;
    }

    let value = aField.getAttribute("value");
    // Avoid returning 0 when field is empty.
    return this.isEmpty(value) ? undefined : Number(value);
  }

  clearFieldValue(aField) {
    aField.textContent = aField.placeholder;
    aField.setAttribute("value", "");
    aField.setAttribute("aria-valuetext", "");
    if (aField.classList.contains("numeric")) {
      aField.setAttribute("typeBuffer", "");
    }
    this.updateResetButtonVisibility();
  }

  openDateTimePicker() {
    this.mInputElement.openDateTimePicker(this.getCurrentValue());
  }

  closeDateTimePicker() {
    if (this.mIsPickerOpen) {
      this.mInputElement.closeDateTimePicker();
    }
  }

  notifyPicker() {
    if (this.mIsPickerOpen && this.isAnyFieldAvailable(true)) {
      this.mInputElement.updateDateTimePicker(this.getCurrentValue());
    }
  }

  isDisabled() {
    return this.mInputElement.hasAttribute("disabled");
  }

  isReadonly() {
    return this.mInputElement.hasAttribute("readonly");
  }

  isEditable() {
    return !this.isDisabled() && !this.isReadonly();
  }

  isRequired() {
    return this.mInputElement.hasAttribute("required");
  }

  containingTree() {
    return this.mInputElement.containingShadowRoot || this.document;
  }

  handleEvent(aEvent) {
    this.log("handleEvent: " + aEvent.type);

    if (!aEvent.isTrusted) {
      return;
    }

    switch (aEvent.type) {
      case "MozDateTimeValueChanged": {
        this.notifyInputElementValueChanged();
        break;
      }
      case "MozNotifyMinMaxStepAttrChanged": {
        this.notifyMinMaxStepAttrChanged();
        break;
      }
      case "MozDateTimeAttributeChanged": {
        this.updateEditAttributes();
        break;
      }
      case "MozPickerValueChanged": {
        this.setValueFromPicker(aEvent.detail);
        break;
      }
      case "MozSetDateTimePickerState": {
        this.setPickerState(aEvent.detail);
        break;
      }
      case "keypress": {
        this.onKeyPress(aEvent);
        break;
      }
      case "click": {
        this.onClick(aEvent);
        break;
      }
      case "focus": {
        this.onFocus(aEvent);
        break;
      }
      case "blur": {
        this.onBlur(aEvent);
        break;
      }
      case "mousedown":
      case "copy":
      case "cut":
      case "paste": {
        aEvent.preventDefault();
        break;
      }
      default:
        break;
    }
  }

  onFocus(aEvent) {
    this.log("onFocus originalTarget: " + aEvent.originalTarget);
    if (this.containingTree().activeElement != this.mInputElement) {
      return;
    }

    let target = aEvent.originalTarget;
    if (target.matches("span.datetime-edit-field")) {
      if (target.disabled) {
        return;
      }
      this.mLastFocusedField = target;
      this.mInputElement.setFocusState(true);
    }
    if (this.mIsPickerOpen && this.isPickerIrrelevantField(target)) {
      this.closeDateTimePicker();
    }
  }

  onBlur(aEvent) {
    this.log(
      "onBlur originalTarget: " +
        aEvent.originalTarget +
        " target: " +
        aEvent.target +
        " rt: " +
        aEvent.relatedTarget
    );

    let target = aEvent.originalTarget;
    target.setAttribute("typeBuffer", "");
    this.setInputValueFromFields();
    // No need to set and unset the focus state if the focus is staying within
    // our input. Same about closing the picker.
    if (aEvent.relatedTarget != this.mInputElement) {
      this.mInputElement.setFocusState(false);
      if (this.mIsPickerOpen) {
        this.closeDateTimePicker();
      }
    }
  }

  isTimeField(field) {
    return (
      field == this.mHourField ||
      field == this.mMinuteField ||
      field == this.mSecondField ||
      field == this.mDayPeriodField
    );
  }

  shouldOpenDateTimePickerOnKeyPress() {
    if (!this.mLastFocusedField) {
      return true;
    }
    return !this.isPickerIrrelevantField(this.mLastFocusedField);
  }

  shouldOpenDateTimePickerOnClick(target) {
    return !this.isPickerIrrelevantField(target);
  }

  // Whether a given field is irrelevant for the purposes of the datetime
  // picker. This is useful for datetime-local, which as of right now only
  // shows a date picker (not a time picker).
  isPickerIrrelevantField(field) {
    if (this.type != "datetime-local") {
      return false;
    }
    return this.isTimeField(field);
  }

  onKeyPress(aEvent) {
    this.log("onKeyPress key: " + aEvent.key);

    switch (aEvent.key) {
      // Toggle the picker on space/enter, close on Escape.
      case "Enter":
      case "Escape":
      case " ": {
        if (this.mIsPickerOpen) {
          this.closeDateTimePicker();
        } else if (
          aEvent.key != "Escape" &&
          this.shouldOpenDateTimePickerOnKeyPress()
        ) {
          this.openDateTimePicker();
        } else {
          // Don't preventDefault();
          break;
        }
        aEvent.preventDefault();
        break;
      }
      case "Backspace": {
        // TODO(emilio, bug 1571533): These functions should look at
        // defaultPrevented.
        if (this.isEditable()) {
          let targetField = aEvent.originalTarget;
          this.clearFieldValue(targetField);
          this.setInputValueFromFields();
          aEvent.preventDefault();
        }
        break;
      }
      case "ArrowRight":
      case "ArrowLeft": {
        this.advanceToNextField(!(aEvent.key == "ArrowRight"));
        aEvent.preventDefault();
        break;
      }
      case "ArrowUp":
      case "ArrowDown":
      case "PageUp":
      case "PageDown":
      case "Home":
      case "End": {
        this.handleKeyboardNav(aEvent);
        aEvent.preventDefault();
        break;
      }
      default: {
        // printable characters
        if (
          aEvent.keyCode == 0 &&
          !(aEvent.ctrlKey || aEvent.altKey || aEvent.metaKey)
        ) {
          this.handleKeypress(aEvent);
          aEvent.preventDefault();
        }
        break;
      }
    }
  }

  onClick(aEvent) {
    this.log(
      "onClick originalTarget: " +
        aEvent.originalTarget +
        " target: " +
        aEvent.target
    );

    if (aEvent.defaultPrevented || !this.isEditable()) {
      return;
    }

    if (aEvent.originalTarget == this.mResetButton) {
      this.clearInputFields(false);
    } else if (
      !this.mIsPickerOpen &&
      this.shouldOpenDateTimePickerOnClick(aEvent.originalTarget)
    ) {
      this.openDateTimePicker();
    }
  }

  buildEditFields() {
    let root = this.shadowRoot.getElementById("edit-wrapper");

    let options = {};

    if (this.shouldShowTime()) {
      options.hour = options.minute = "numeric";
      options.hour12 = this.mHour12;
      if (this.shouldShowSecondField()) {
        options.second = "numeric";
      }
    }

    if (this.shouldShowDate()) {
      options.year = options.month = options.day = "numeric";
    }

    let formatter = Intl.DateTimeFormat(this.mLocales, options);
    formatter.formatToParts(Date.now()).map(part => {
      switch (part.type) {
        case "year":
          this.mYearField = this.createEditFieldAndAppend(
            this.mYearPlaceHolder,
            this.mYearLabel,
            true,
            this.mYearLength,
            this.mMaxYear.toString().length,
            this.mMinYear,
            this.mMaxYear,
            this.mYearPageUpDownInterval
          );
          this.addEventListenersToField(this.mYearField);
          break;
        case "month":
          this.mMonthField = this.createEditFieldAndAppend(
            this.mMonthPlaceHolder,
            this.mMonthLabel,
            true,
            this.mMonthDayLength,
            this.mMonthDayLength,
            this.mMinMonth,
            this.mMaxMonth,
            this.mMonthPageUpDownInterval
          );
          this.addEventListenersToField(this.mMonthField);
          break;
        case "day":
          this.mDayField = this.createEditFieldAndAppend(
            this.mDayPlaceHolder,
            this.mDayLabel,
            true,
            this.mMonthDayLength,
            this.mMonthDayLength,
            this.mMinDay,
            this.mMaxDay,
            this.mDayPageUpDownInterval
          );
          this.addEventListenersToField(this.mDayField);
          break;
        case "hour":
          this.mHourField = this.createEditFieldAndAppend(
            this.mHourPlaceHolder,
            this.mHourLabel,
            true,
            this.mMaxLength,
            this.mMaxLength,
            this.mMinHour,
            this.mMaxHour,
            this.mHourPageUpDownInterval
          );
          this.addEventListenersToField(this.mHourField);
          break;
        case "minute":
          this.mMinuteField = this.createEditFieldAndAppend(
            this.mMinutePlaceHolder,
            this.mMinuteLabel,
            true,
            this.mMaxLength,
            this.mMaxLength,
            this.mMinMinute,
            this.mMaxMinute,
            this.mMinSecPageUpDownInterval
          );
          this.addEventListenersToField(this.mMinuteField);
          break;
        case "second":
          this.mSecondField = this.createEditFieldAndAppend(
            this.mSecondPlaceHolder,
            this.mSecondLabel,
            true,
            this.mMaxLength,
            this.mMaxLength,
            this.mMinSecond,
            this.mMaxSecond,
            this.mMinSecPageUpDownInterval
          );
          this.addEventListenersToField(this.mSecondField);
          if (this.shouldShowMillisecField()) {
            // Intl.DateTimeFormat does not support millisecond, so we
            // need to handle this on our own.
            let span = this.shadowRoot.createElementAndAppendChildAt(
              root,
              "span"
            );
            span.textContent = this.mMillisecSeparatorText;
            this.mMillisecField = this.createEditFieldAndAppend(
              this.mMillisecPlaceHolder,
              this.mMillisecLabel,
              true,
              this.mMillisecMaxLength,
              this.mMillisecMaxLength,
              this.mMinMillisecond,
              this.mMaxMillisecond,
              this.mMinSecPageUpDownInterval
            );
            this.addEventListenersToField(this.mMillisecField);
          }
          break;
        case "dayPeriod":
          this.mDayPeriodField = this.createEditFieldAndAppend(
            this.mDayPeriodPlaceHolder,
            this.mDayPeriodLabel,
            false
          );
          this.addEventListenersToField(this.mDayPeriodField);

          // Give aria autocomplete hint for am/pm
          this.mDayPeriodField.setAttribute("aria-autocomplete", "inline");
          break;
        default:
          let span = this.shadowRoot.createElementAndAppendChildAt(
            root,
            "span"
          );
          span.textContent = part.value;
          break;
      }
    });
  }

  clearInputFields(aFromInputElement) {
    this.log("clearInputFields");

    if (this.mMonthField) {
      this.clearFieldValue(this.mMonthField);
    }

    if (this.mDayField) {
      this.clearFieldValue(this.mDayField);
    }

    if (this.mYearField) {
      this.clearFieldValue(this.mYearField);
    }

    if (this.mHourField) {
      this.clearFieldValue(this.mHourField);
    }

    if (this.mMinuteField) {
      this.clearFieldValue(this.mMinuteField);
    }

    if (this.mSecondField) {
      this.clearFieldValue(this.mSecondField);
    }

    if (this.mMillisecField) {
      this.clearFieldValue(this.mMillisecField);
    }

    if (this.mDayPeriodField) {
      this.clearFieldValue(this.mDayPeriodField);
    }

    if (!aFromInputElement) {
      if (this.mInputElement.value) {
        this.mInputElement.setUserInput("");
      } else {
        this.mInputElement.updateValidityState();
      }
    }
  }

  setFieldsFromInputValue() {
    // Second and millisecond part are optional, rebuild edit fields if
    // needed.
    this.rebuildEditFieldsIfNeeded();

    let value = this.mInputElement.value;
    if (!value) {
      this.clearInputFields(true);
      return;
    }

    let {
      year,
      month,
      day,
      hour,
      minute,
      second,
      millisecond,
    } = this.getInputElementValues();
    if (this.shouldShowDate()) {
      this.log("setFieldsFromInputValue: " + value);
      this.setFieldValue(this.mYearField, year);
      this.setFieldValue(this.mMonthField, month);
      this.setFieldValue(this.mDayField, day);
    }

    if (this.shouldShowTime()) {
      if (this.isEmpty(hour) && this.isEmpty(minute)) {
        this.clearInputFields(true);
        return;
      }

      this.setFieldValue(this.mHourField, hour);
      this.setFieldValue(this.mMinuteField, minute);
      if (this.mHour12) {
        this.setDayPeriodValue(
          hour >= this.mMaxHour ? this.mPMIndicator : this.mAMIndicator
        );
      }

      if (this.mSecondField) {
        this.setFieldValue(this.mSecondField, second || 0);
      }

      if (this.mMillisecField) {
        this.setFieldValue(this.mMillisecField, millisecond || 0);
      }
    }

    this.notifyPicker();
  }

  setInputValueFromFields() {
    if (this.isAnyFieldEmpty()) {
      // Clear input element's value if any of the field has been cleared,
      // otherwise update the validity state, since it may become "not"
      // invalid if fields are not complete.
      if (this.mInputElement.value) {
        this.mInputElement.setUserInput("");
      } else {
        this.mInputElement.updateValidityState();
      }
      // We still need to notify picker in case any of the field has
      // changed.
      this.notifyPicker();
      return;
    }

    let {
      year,
      month,
      day,
      hour,
      minute,
      second,
      millisecond,
      dayPeriod,
    } = this.getCurrentValue();

    let time = "";
    let date = "";

    // Convert to a valid time string according to:
    // https://html.spec.whatwg.org/multipage/infrastructure.html#valid-time-string
    if (this.shouldShowTime()) {
      if (this.mHour12) {
        if (dayPeriod == this.mPMIndicator && hour < this.mMaxHour) {
          hour += this.mMaxHour;
        } else if (dayPeriod == this.mAMIndicator && hour == this.mMaxHour) {
          hour = 0;
        }
      }

      hour = hour < 10 ? "0" + hour : hour;
      minute = minute < 10 ? "0" + minute : minute;

      time = hour + ":" + minute;
      if (second != undefined) {
        second = second < 10 ? "0" + second : second;
        time += ":" + second;
      }

      if (millisecond != undefined) {
        // Convert milliseconds to fraction of second.
        millisecond = millisecond
          .toString()
          .padStart(this.mMillisecMaxLength, "0");
        time += "." + millisecond;
      }
    }

    if (this.shouldShowDate()) {
      // Convert to a valid date string according to:
      // https://html.spec.whatwg.org/multipage/infrastructure.html#valid-date-string
      year = year.toString().padStart(this.mYearLength, "0");
      month = month < 10 ? "0" + month : month;
      day = day < 10 ? "0" + day : day;
      date = [year, month, day].join("-");
    }

    let value;
    if (date) {
      value = date;
    }
    if (time) {
      // https://html.spec.whatwg.org/#valid-normalised-local-date-and-time-string
      value = value ? value + "T" + time : time;
    }

    if (value == this.mInputElement.value) {
      return;
    }
    this.log("setInputValueFromFields: " + value);
    this.notifyPicker();
    this.mInputElement.setUserInput(value);
  }

  setFieldsFromPicker({ year, month, day, hour, minute }) {
    if (!this.isEmpty(hour)) {
      this.setFieldValue(this.mHourField, hour);
      if (this.mHour12) {
        this.setDayPeriodValue(
          hour >= this.mMaxHour ? this.mPMIndicator : this.mAMIndicator
        );
      }
    }

    if (!this.isEmpty(minute)) {
      this.setFieldValue(this.mMinuteField, minute);
    }

    if (!this.isEmpty(year)) {
      this.setFieldValue(this.mYearField, year);
    }

    if (!this.isEmpty(month)) {
      this.setFieldValue(this.mMonthField, month);
    }

    if (!this.isEmpty(day)) {
      this.setFieldValue(this.mDayField, day);
    }

    // Update input element's .value if needed.
    this.setInputValueFromFields();
  }

  handleKeypress(aEvent) {
    if (!this.isEditable()) {
      return;
    }

    let targetField = aEvent.originalTarget;
    let key = aEvent.key;

    if (targetField == this.mDayPeriodField) {
      if (key == "a" || key == "A") {
        this.setDayPeriodValue(this.mAMIndicator);
      } else if (key == "p" || key == "P") {
        this.setDayPeriodValue(this.mPMIndicator);
      }
      if (!this.isAnyFieldEmpty()) {
        this.setInputValueFromFields();
      }
      return;
    }

    if (targetField.classList.contains("numeric") && key.match(/[0-9]/)) {
      let buffer = targetField.getAttribute("typeBuffer") || "";

      buffer = buffer.concat(key);
      this.setFieldValue(targetField, buffer);

      let n = Number(buffer);
      let max = targetField.getAttribute("max");
      let maxLength = targetField.getAttribute("maxlength");
      if (buffer.length >= maxLength || n * 10 > max) {
        buffer = "";
        this.advanceToNextField();
      }
      targetField.setAttribute("typeBuffer", buffer);
      if (!this.isAnyFieldEmpty()) {
        this.setInputValueFromFields();
      }
    }
  }

  getCurrentValue() {
    let value = {};
    if (this.shouldShowDate()) {
      value.year = this.getFieldValue(this.mYearField);
      value.month = this.getFieldValue(this.mMonthField);
      value.day = this.getFieldValue(this.mDayField);
    }

    if (this.shouldShowTime()) {
      let dayPeriod = this.getDayPeriodValue();
      let hour = this.getFieldValue(this.mHourField);
      if (!this.isEmpty(hour)) {
        if (this.mHour12) {
          if (dayPeriod == this.mPMIndicator && hour < this.mMaxHour) {
            hour += this.mMaxHour;
          } else if (dayPeriod == this.mAMIndicator && hour == this.mMaxHour) {
            hour = 0;
          }
        }
      }
      value.hour = hour;
      value.dayPeriod = dayPeriod;
      value.minute = this.getFieldValue(this.mMinuteField);
      value.second = this.getFieldValue(this.mSecondField);
      value.millisecond = this.getFieldValue(this.mMillisecField);
    }

    this.log("getCurrentValue: " + JSON.stringify(value));
    return value;
  }

  setFieldValue(aField, aValue) {
    if (!aField || !aField.classList.contains("numeric")) {
      return;
    }

    let value = Number(aValue);
    if (isNaN(value)) {
      this.log("NaN on setFieldValue!");
      return;
    }

    if (aField == this.mHourField) {
      if (this.mHour12) {
        // Try to change to 12hr format if user input is 0 or greater
        // than 12.
        let maxLength = aField.getAttribute("maxlength");
        if (value == 0 && aValue.length == maxLength) {
          value = this.mMaxHour;
        } else {
          value = value > this.mMaxHour ? value % this.mMaxHour : value;
        }
      } else if (value > this.mMaxHour) {
        value = this.mMaxHour;
      }
    }

    let maxLength = aField.getAttribute("maxlength");
    if (aValue.length == maxLength) {
      let min = Number(aField.getAttribute("min"));
      let max = Number(aField.getAttribute("max"));

      if (value < min) {
        value = min;
      } else if (value > max) {
        value = max;
      }
    }

    aField.setAttribute("value", value);

    let minDigits = aField.getAttribute("mindigits");
    let formatted = value.toLocaleString(this.mLocales, {
      minimumIntegerDigits: minDigits,
      useGrouping: false,
    });

    aField.textContent = formatted;
    aField.setAttribute("aria-valuetext", formatted);
    this.updateResetButtonVisibility();
  }

  isAnyFieldAvailable(aForPicker = false) {
    let {
      year,
      month,
      day,
      hour,
      minute,
      second,
      millisecond,
    } = this.getCurrentValue();
    if (
      !this.isEmpty(year) ||
      !this.isEmpty(month) ||
      !this.isEmpty(day) ||
      !this.isEmpty(hour) ||
      !this.isEmpty(minute)
    ) {
      return true;
    }

    // Picker doesn't care about seconds / milliseconds / day period.
    if (aForPicker) {
      return false;
    }

    let dayPeriod = this.getDayPeriodValue();
    return (
      (this.mDayPeriodField && !this.isEmpty(dayPeriod)) ||
      (this.mSecondField && !this.isEmpty(second)) ||
      (this.mMillisecField && !this.isEmpty(millisecond))
    );
  }

  isAnyFieldEmpty() {
    let {
      year,
      month,
      day,
      hour,
      minute,
      second,
      millisecond,
    } = this.getCurrentValue();
    return (
      (this.mYearField && this.isEmpty(year)) ||
      (this.mMonthField && this.isEmpty(month)) ||
      (this.mDayField && this.isEmpty(day)) ||
      (this.mHourField && this.isEmpty(hour)) ||
      (this.mMinuteField && this.isEmpty(minute)) ||
      (this.mDayPeriodField && this.isEmpty(this.getDayPeriodValue())) ||
      (this.mSecondField && this.isEmpty(second)) ||
      (this.mMillisecField && this.isEmpty(millisecond))
    );
  }

  get kMsPerSecond() {
    return 1000;
  }

  get kMsPerMinute() {
    return 60 * 1000;
  }

  getInputElementValues() {
    let value = this.mInputElement.value;
    if (value.length === 0) {
      return {};
    }

    let date, time;

    let year, month, day, hour, minute, second, millisecond;
    if (this.type == "date") {
      date = value;
    }
    if (this.type == "time") {
      time = value;
    }
    if (this.type == "datetime-local") {
      // https://html.spec.whatwg.org/#valid-normalised-local-date-and-time-string
      [date, time] = value.split("T");
    }
    if (date) {
      [year, month, day] = date.split("-");
    }
    if (time) {
      [hour, minute, second] = time.split(":");
      if (second) {
        [second, millisecond] = second.split(".");

        // Convert fraction of second to milliseconds.
        if (millisecond && millisecond.length === 1) {
          millisecond *= 100;
        } else if (millisecond && millisecond.length === 2) {
          millisecond *= 10;
        }
      }
    }
    return { year, month, day, hour, minute, second, millisecond };
  }

  shouldShowSecondField() {
    if (!this.shouldShowTime()) {
      return false;
    }
    let { second } = this.getInputElementValues();
    if (second != undefined) {
      return true;
    }

    let stepBase = this.mInputElement.getStepBase();
    if (stepBase % this.kMsPerMinute != 0) {
      return true;
    }

    let step = this.mInputElement.getStep();
    if (step % this.kMsPerMinute != 0) {
      return true;
    }

    return false;
  }

  shouldShowMillisecField() {
    if (!this.shouldShowTime()) {
      return false;
    }

    let { millisecond } = this.getInputElementValues();
    if (millisecond != undefined) {
      return true;
    }

    let stepBase = this.mInputElement.getStepBase();
    if (stepBase % this.kMsPerSecond != 0) {
      return true;
    }

    let step = this.mInputElement.getStep();
    if (step % this.kMsPerSecond != 0) {
      return true;
    }

    return false;
  }

  getStringsForLocale(aLocales) {
    this.log("getStringsForLocale: " + aLocales);

    let intlUtils = this.window.intlUtils;
    if (!intlUtils) {
      return {};
    }

    let result = intlUtils.getDisplayNames(this.mLocales, {
      type: "dayPeriod",
      style: "short",
      calendar: "gregory",
      keys: ["am", "pm"],
    });

    let [amString, pmString] = result.values;

    return { amString, pmString };
  }

  is12HourTime(aLocales) {
    let options = new Intl.DateTimeFormat(aLocales, {
      hour: "numeric",
    }).resolvedOptions();

    return options.hour12;
  }

  incrementFieldValue(aTargetField, aTimes) {
    let value = this.getFieldValue(aTargetField);

    // Use current time if field is empty.
    if (this.isEmpty(value)) {
      let now = new Date();

      if (aTargetField == this.mYearField) {
        value = now.getFullYear();
      } else if (aTargetField == this.mMonthField) {
        value = now.getMonth() + 1;
      } else if (aTargetField == this.mDayField) {
        value = now.getDate();
      } else if (aTargetField == this.mHourField) {
        value = now.getHours();
        if (this.mHour12) {
          value = value % this.mMaxHour || this.mMaxHour;
        }
      } else if (aTargetField == this.mMinuteField) {
        value = now.getMinutes();
      } else if (aTargetField == this.mSecondField) {
        value = now.getSeconds();
      } else if (aTargetField == this.mMillisecField) {
        value = now.getMilliseconds();
      } else {
        this.log("Field not supported in incrementFieldValue.");
        return;
      }
    }

    let min = +aTargetField.getAttribute("min");
    let max = +aTargetField.getAttribute("max");

    value += Number(aTimes);
    if (value > max) {
      value -= max - min + 1;
    } else if (value < min) {
      value += max - min + 1;
    }

    this.setFieldValue(aTargetField, value);
  }

  handleKeyboardNav(aEvent) {
    if (!this.isEditable()) {
      return;
    }

    let targetField = aEvent.originalTarget;
    let key = aEvent.key;

    if (targetField == this.mYearField && (key == "Home" || key == "End")) {
      // Home/End key does nothing on year field.
      return;
    }

    if (targetField == this.mDayPeriodField) {
      // Home/End key does nothing on AM/PM field.
      if (key == "Home" || key == "End") {
        return;
      }

      this.setDayPeriodValue(
        this.getDayPeriodValue() == this.mAMIndicator
          ? this.mPMIndicator
          : this.mAMIndicator
      );
      this.setInputValueFromFields();
      return;
    }

    switch (key) {
      case "ArrowUp":
        this.incrementFieldValue(targetField, 1);
        break;
      case "ArrowDown":
        this.incrementFieldValue(targetField, -1);
        break;
      case "PageUp": {
        let interval = targetField.getAttribute("pginterval");
        this.incrementFieldValue(targetField, interval);
        break;
      }
      case "PageDown": {
        let interval = targetField.getAttribute("pginterval");
        this.incrementFieldValue(targetField, 0 - interval);
        break;
      }
      case "Home":
        let min = targetField.getAttribute("min");
        this.setFieldValue(targetField, min);
        break;
      case "End":
        let max = targetField.getAttribute("max");
        this.setFieldValue(targetField, max);
        break;
    }
    this.setInputValueFromFields();
  }

  getDayPeriodValue(aValue) {
    if (!this.mDayPeriodField) {
      return "";
    }

    let placeholder = this.mDayPeriodField.placeholder;
    let value = this.mDayPeriodField.textContent;
    return value == placeholder ? "" : value;
  }

  setDayPeriodValue(aValue) {
    if (!this.mDayPeriodField) {
      return;
    }

    this.mDayPeriodField.textContent = aValue;
    this.mDayPeriodField.setAttribute("value", aValue);
    this.updateResetButtonVisibility();
  }
};
