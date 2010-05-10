_("Make sure lock prevents calling with a shared lock");
Cu.import("resource://weave/util.js");

function run_test() {
  let ret, rightThis, didCall;
  let state, lockState, lockedState, unlockState;
  let obj = {
    _lock: Utils.lock,
    lock: function() {
      lockState = ++state;
      if (this._locked) {
        lockedState = ++state;
        return false;
      }
      this._locked = true;
      return true;
    },
    unlock: function() {
      unlockState = ++state;
      this._locked = false;
    },

    func: function() this._lock(function() {
      rightThis = this == obj;
      didCall = true;
      return 5;
    })(),

    throwy: function() this._lock(function() {
      rightThis = this == obj;
      didCall = true;
      this.throwy();
    })()
  };

  _("Make sure a normal call will call and return");
  rightThis = didCall = false;
  state = 0;
  ret = obj.func();
  do_check_eq(ret, 5);
  do_check_true(rightThis);
  do_check_true(didCall);
  do_check_eq(lockState, 1);
  do_check_eq(unlockState, 2);
  do_check_eq(state, 2);

  _("Make sure code that calls locked code throws");
  ret = null;
  rightThis = didCall = false;
  try {
    ret = obj.throwy();
    do_throw("throwy internal call should have thrown!");
  }
  catch(ex) {
    do_check_eq(ex, "Could not acquire lock");
  }
  do_check_eq(ret, null);
  do_check_true(rightThis);
  do_check_true(didCall);
  _("Lock should be called twice so state 3 is skipped");
  do_check_eq(lockState, 4);
  do_check_eq(lockedState, 5);
  do_check_eq(unlockState, 6);
  do_check_eq(state, 6);
}
