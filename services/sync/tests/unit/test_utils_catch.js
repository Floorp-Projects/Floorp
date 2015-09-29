Cu.import("resource://services-sync/util.js");
Cu.import("resource://services-sync/service.js");

function run_test() {
  _("Make sure catch when copied to an object will correctly catch stuff");
  let ret, rightThis, didCall, didThrow, wasTen, wasLocked;
  let obj = {
    catch: Utils.catch,
    _log: {
      debug: function(str) {
        didThrow = str.search(/^Exception: /) == 0;
      },
      info: function(str) {
        wasLocked = str.indexOf("Cannot start sync: already syncing?") == 0;
      }
    },

    func: function() {
      return this.catch(function() {
        rightThis = this == obj;
        didCall = true;
        return 5;
      })();
    },

    throwy: function() {
      return this.catch(function() {
        rightThis = this == obj;
        didCall = true;
        throw 10;
      })();
    },

    callbacky: function() {
      return this.catch(function() {
        rightThis = this == obj;
        didCall = true;
        throw 10;
      }, function(ex) {
        wasTen = (ex == 10)
      })();
    },

    lockedy: function() {
      return this.catch(function() {
        rightThis = this == obj;
        didCall = true;
        throw("Could not acquire lock.");
      })();
    }
  };

  _("Make sure a normal call will call and return");
  rightThis = didCall = didThrow = wasLocked = false;
  ret = obj.func();
  do_check_eq(ret, 5);
  do_check_true(rightThis);
  do_check_true(didCall);
  do_check_false(didThrow);
  do_check_eq(wasTen, undefined);
  do_check_false(wasLocked);

  _("Make sure catch/throw results in debug call and caller doesn't need to handle exception");
  rightThis = didCall = didThrow = wasLocked = false;
  ret = obj.throwy();
  do_check_eq(ret, undefined);
  do_check_true(rightThis);
  do_check_true(didCall);
  do_check_true(didThrow);
  do_check_eq(wasTen, undefined);
  do_check_false(wasLocked);

  _("Test callback for exception testing.");
  rightThis = didCall = didThrow = wasLocked = false;
  ret = obj.callbacky();
  do_check_eq(ret, undefined);
  do_check_true(rightThis);
  do_check_true(didCall);
  do_check_true(didThrow);
  do_check_true(wasTen);
  do_check_false(wasLocked);

  _("Test the lock-aware catch that Service uses.");
  obj.catch = Service._catch;
  rightThis = didCall = didThrow = wasLocked = false;
  wasTen = undefined;
  ret = obj.lockedy();
  do_check_eq(ret, undefined);
  do_check_true(rightThis);
  do_check_true(didCall);
  do_check_true(didThrow);
  do_check_eq(wasTen, undefined);
  do_check_true(wasLocked);
}
