_("Make sure notify sends out the right notifications");
Cu.import("resource://services-sync/util.js");

function run_test() {
  let ret, rightThis, didCall;
  let obj = {
    notify: Utils.notify("foo:"),
    _log: {
      trace: function() {}
    },

    func: function() {
      return this.notify("bar", "baz", function() {
        rightThis = this == obj;
        didCall = true;
        return 5;
      })();
    },

    throwy: function() {
      return this.notify("bad", "one", function() {
        rightThis = this == obj;
        didCall = true;
        throw 10;
      })();
    }
  };

  let state = 0;
  let makeObs = function(topic) {
    let obj = {
      observe: function(subject, topic, data) {
        this.state = ++state;
        this.subject = subject;
        this.topic = topic;
        this.data = data;
      }
    };

    Svc.Obs.add(topic, obj);
    return obj;
  };

  _("Make sure a normal call will call and return with notifications");
  rightThis = didCall = false;
  let fs = makeObs("foo:bar:start");
  let ff = makeObs("foo:bar:finish");
  let fe = makeObs("foo:bar:error");
  ret = obj.func();
  do_check_eq(ret, 5);
  do_check_true(rightThis);
  do_check_true(didCall);

  do_check_eq(fs.state, 1);
  do_check_eq(fs.subject, undefined);
  do_check_eq(fs.topic, "foo:bar:start");
  do_check_eq(fs.data, "baz");

  do_check_eq(ff.state, 2);
  do_check_eq(ff.subject, 5);
  do_check_eq(ff.topic, "foo:bar:finish");
  do_check_eq(ff.data, "baz");

  do_check_eq(fe.state, undefined);
  do_check_eq(fe.subject, undefined);
  do_check_eq(fe.topic, undefined);
  do_check_eq(fe.data, undefined);

  _("Make sure a throwy call will call and throw with notifications");
  ret = null;
  rightThis = didCall = false;
  let ts = makeObs("foo:bad:start");
  let tf = makeObs("foo:bad:finish");
  let te = makeObs("foo:bad:error");
  try {
    ret = obj.throwy();
    do_throw("throwy should have thrown!");
  }
  catch(ex) {
    do_check_eq(ex, 10);
  }
  do_check_eq(ret, null);
  do_check_true(rightThis);
  do_check_true(didCall);

  do_check_eq(ts.state, 3);
  do_check_eq(ts.subject, undefined);
  do_check_eq(ts.topic, "foo:bad:start");
  do_check_eq(ts.data, "one");

  do_check_eq(tf.state, undefined);
  do_check_eq(tf.subject, undefined);
  do_check_eq(tf.topic, undefined);
  do_check_eq(tf.data, undefined);

  do_check_eq(te.state, 4);
  do_check_eq(te.subject, 10);
  do_check_eq(te.topic, "foo:bad:error");
  do_check_eq(te.data, "one");
}
