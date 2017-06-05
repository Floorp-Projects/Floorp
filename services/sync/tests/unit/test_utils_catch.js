Cu.import("resource://services-sync/util.js");
Cu.import("resource://services-sync/service.js");

add_task(async function run_test() {
  _("Make sure catch when copied to an object will correctly catch stuff");
  let ret, rightThis, didCall, didThrow, wasTen, wasLocked;
  let obj = {
    _catch: Utils.catch,
    _log: {
      debug(str) {
        didThrow = str.search(/^Exception/) == 0;
      },
      info(str) {
        wasLocked = str.indexOf("Cannot start sync: already syncing?") == 0;
      }
    },

    func() {
      return this._catch(async function() {
        rightThis = this == obj;
        didCall = true;
        return 5;
      })();
    },

    throwy() {
      return this._catch(async function() {
        rightThis = this == obj;
        didCall = true;
        throw 10;
      })();
    },

    callbacky() {
      return this._catch(async function() {
        rightThis = this == obj;
        didCall = true;
        throw 10;
      }, async function(ex) {
        wasTen = (ex == 10)
      })();
    },

    lockedy() {
      return this._catch(async function() {
        rightThis = this == obj;
        didCall = true;
        throw ("Could not acquire lock.");
      })();
    },

    lockedy_chained() {
      return this._catch(function() {
        rightThis = this == obj;
        didCall = true;
        return Promise.resolve().then( () => { throw ("Could not acquire lock.") });
      })();
    },
  };

  _("Make sure a normal call will call and return");
  rightThis = didCall = didThrow = wasLocked = false;
  ret = await obj.func();
  do_check_eq(ret, 5);
  do_check_true(rightThis);
  do_check_true(didCall);
  do_check_false(didThrow);
  do_check_eq(wasTen, undefined);
  do_check_false(wasLocked);

  _("Make sure catch/throw results in debug call and caller doesn't need to handle exception");
  rightThis = didCall = didThrow = wasLocked = false;
  ret = await obj.throwy();
  do_check_eq(ret, undefined);
  do_check_true(rightThis);
  do_check_true(didCall);
  do_check_true(didThrow);
  do_check_eq(wasTen, undefined);
  do_check_false(wasLocked);

  _("Test callback for exception testing.");
  rightThis = didCall = didThrow = wasLocked = false;
  ret = await obj.callbacky();
  do_check_eq(ret, undefined);
  do_check_true(rightThis);
  do_check_true(didCall);
  do_check_true(didThrow);
  do_check_true(wasTen);
  do_check_false(wasLocked);

  _("Test the lock-aware catch that Service uses.");
  obj._catch = Service._catch;
  rightThis = didCall = didThrow = wasLocked = false;
  wasTen = undefined;
  ret = await obj.lockedy();
  do_check_eq(ret, undefined);
  do_check_true(rightThis);
  do_check_true(didCall);
  do_check_true(didThrow);
  do_check_eq(wasTen, undefined);
  do_check_true(wasLocked);

  _("Test the lock-aware catch that Service uses with a chained promise.");
  rightThis = didCall = didThrow = wasLocked = false;
  wasTen = undefined;
  ret = await obj.lockedy_chained();
  do_check_eq(ret, undefined);
  do_check_true(rightThis);
  do_check_true(didCall);
  do_check_true(didThrow);
  do_check_eq(wasTen, undefined);
  do_check_true(wasLocked);
});
