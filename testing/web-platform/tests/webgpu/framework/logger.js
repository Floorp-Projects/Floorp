/**
* AUTO-GENERATED - DO NOT EDIT. Source: https://github.com/gpuweb/cts
**/

function _defineProperty(obj, key, value) { if (key in obj) { Object.defineProperty(obj, key, { value: value, enumerable: true, configurable: true, writable: true }); } else { obj[key] = value; } return obj; }

import { SkipTestCase } from './fixture.js';
import { makeQueryString } from './url_query.js';
import { extractPublicParams } from './url_query.js';
import { getStackTrace, now } from './util/index.js';
import { version } from './version.js';
export class Logger {
  constructor() {
    _defineProperty(this, "results", []);
  }

  record(spec) {
    const result = {
      spec: makeQueryString(spec),
      cases: []
    };
    this.results.push(result);
    return [new TestSpecRecorder(result), result];
  }

  asJSON(space) {
    return JSON.stringify({
      version,
      results: this.results
    }, undefined, space);
  }

}
export class TestSpecRecorder {
  constructor(result) {
    _defineProperty(this, "result", void 0);

    this.result = result;
  }

  record(test, params) {
    const result = {
      test,
      params: params ? extractPublicParams(params) : null,
      status: 'running',
      timems: -1
    };
    this.result.cases.push(result);
    return [new TestCaseRecorder(result), result];
  }

}
var PassState;

(function (PassState) {
  PassState[PassState["pass"] = 0] = "pass";
  PassState[PassState["skip"] = 1] = "skip";
  PassState[PassState["warn"] = 2] = "warn";
  PassState[PassState["fail"] = 3] = "fail";
})(PassState || (PassState = {}));

export class TestCaseRecorder {
  constructor(result) {
    _defineProperty(this, "result", void 0);

    _defineProperty(this, "state", PassState.pass);

    _defineProperty(this, "startTime", -1);

    _defineProperty(this, "logs", []);

    _defineProperty(this, "debugging", false);

    this.result = result;
  }

  start(debug = false) {
    this.startTime = now();
    this.logs = [];
    this.state = PassState.pass;
    this.debugging = debug;
  }

  finish() {
    if (this.startTime < 0) {
      throw new Error('finish() before start()');
    }

    const endTime = now(); // Round to next microsecond to avoid storing useless .xxxx00000000000002 in results.

    this.result.timems = Math.ceil((endTime - this.startTime) * 1000) / 1000;
    this.result.status = PassState[this.state];
    this.result.logs = this.logs;
    this.debugging = false;
  }

  debug(msg) {
    if (!this.debugging) {
      return;
    }

    this.log('DEBUG: ' + msg);
  }

  log(msg) {
    this.logs.push(msg);
  }

  warn(msg) {
    this.setState(PassState.warn);
    let m = 'WARN';

    if (msg) {
      m += ': ' + msg;
    }

    m += ' ' + getStackTrace(new Error());
    this.log(m);
  }

  fail(msg) {
    this.setState(PassState.fail);
    let m = 'FAIL';

    if (msg) {
      m += ': ' + msg;
    }

    m += '\n' + getStackTrace(new Error());
    this.log(m);
  }

  skipped(ex) {
    this.setState(PassState.skip);
    const m = 'SKIPPED: ' + getStackTrace(ex);
    this.log(m);
  }

  threw(ex) {
    if (ex instanceof SkipTestCase) {
      this.skipped(ex);
      return;
    }

    this.setState(PassState.fail);
    this.log('EXCEPTION: ' + ex.name + ':\n' + ex.message + '\n' + getStackTrace(ex));
  }

  setState(state) {
    this.state = Math.max(this.state, state);
  }

}
//# sourceMappingURL=logger.js.map