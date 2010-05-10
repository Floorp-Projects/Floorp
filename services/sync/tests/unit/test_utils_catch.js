_("Make sure catch when copied to an object will correctly catch stuff");
Cu.import("resource://weave/util.js");

function run_test() {
  let ret, rightThis, didCall, didThrow;
  let obj = {
    catch: Utils.catch,
    _log: {
      debug: function(str) {
        didThrow = str.search(/^Exception: /) == 0;
      }
    },

    func: function() this.catch(function() {
      rightThis = this == obj;
      didCall = true;
      return 5;
    })(),

    throwy: function() this.catch(function() {
      rightThis = this == obj;
      didCall = true;
      throw 10;
    })()
  };

  _("Make sure a normal call will call and return");
  rightThis = didCall = didThrow = false;
  ret = obj.func();
  do_check_eq(ret, 5);
  do_check_true(rightThis);
  do_check_true(didCall);
  do_check_false(didThrow);

  _("Make sure catch/throw results in debug call and caller doesn't need to handle exception");
  rightThis = didCall = didThrow = false;
  ret = obj.throwy();
  do_check_eq(ret, undefined);
  do_check_true(rightThis);
  do_check_true(didCall);
  do_check_true(didThrow);
}
