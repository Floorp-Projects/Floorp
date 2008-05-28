const EXPORTED_SYMBOLS = ["FaultTolerance"];

FaultTolerance = {
  get Service() {
    if (!this._Service)
      this._Service = new FaultToleranceService();
    return this._Service;
  }
}

function FaultToleranceService() {
}

FaultToleranceService.prototype = {
  processMessage: function FTApp_doAppend(message) {
    dump(message);
  }
};
