Cu.import("resource://services-sync/util.js");

function WinMock(href) {
  this.location = {href: href};
  this._open = true;
};
WinMock.prototype = {
  addEventListener: function(type, listener) {
    this._listener = listener;
  },
  close: function() {
    if (this._listener) {
        this._listener();
    }
    this._open = false;
  }
};

function run_test() {
    let w1 = new WinMock("chrome://win/win.xul");
    Utils.ensureOneOpen(w1);
    do_check_true(w1._open);

    let w2 = new WinMock("chrome://win/win.xul");
    Utils.ensureOneOpen(w2);
    do_check_false(w1._open);

    // close w2 and test that ensureOneOpen doesn't
    // close it again
    w2.close();
    w2._open = true;
    let w3 = new WinMock("chrome://win/win.xul");
    Utils.ensureOneOpen(w3);
    do_check_true(w2._open);
}
