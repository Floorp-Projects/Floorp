"use strict";

var EXPORTED_SYMBOLS = ["sinon"];

var {Services} = ChromeUtils.import("resource://gre/modules/Services.jsm");

// ================================================
// Load mocking/stubbing library sinon
// docs: http://sinonjs.org/releases/v7.2.7/
const {clearInterval, clearTimeout, setInterval, setIntervalWithTarget, setTimeout, setTimeoutWithTarget} = ChromeUtils.import("resource://gre/modules/Timer.jsm");
// eslint-disable-next-line no-unused-vars
const global = {
    clearInterval,
    clearTimeout,
    setInterval,
    setIntervalWithTarget,
    setTimeout,
    setTimeoutWithTarget,
};
Services.scriptloader.loadSubScript("resource://testing-common/sinon-7.2.7.js", this);
const sinon = global.sinon;
// ================================================
