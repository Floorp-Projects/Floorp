const Cu = Components.utils;
Cu.import("resource://weave/log4moz.js");

const EXPORTED_SYMBOLS = ["FaultTolerance"];

FaultTolerance = {
  get Service() {
    if (!this._Service)
      this._Service = new FaultToleranceService();
    return this._Service;
  }
};

function FaultToleranceService() {
}

FaultToleranceService.prototype = {
  init: function FTS_init() {
    var appender = new Appender();

    Log4Moz.Service.rootLogger.addAppender(appender);
  }
};

function Formatter() {
}
Formatter.prototype = {
  format: function FTF_format(message) {
    return message;
  }
};
Formatter.prototype.__proto__ = new Log4Moz.Formatter();

function Appender() {
  this._name = "FaultToleranceAppender";
  this._formatter = new Formatter();
}
Appender.prototype = {
  doAppend: function FTA_append(message) {
    // TODO: Implement this.
  }
};
Appender.prototype.__proto__ = new Log4Moz.Appender();
