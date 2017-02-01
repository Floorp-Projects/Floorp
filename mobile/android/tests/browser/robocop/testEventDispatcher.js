Components.utils.import("resource://gre/modules/Messaging.jsm");
Components.utils.import("resource://gre/modules/Services.jsm");

var java = new JavaBridge(this);

do_register_cleanup(() => {
  java.disconnect();
});
do_test_pending();

function get_test_message() {
  let innerObject = {
    boolean: true,
    booleanArray: [false, true],
    int: 1,
    intArray: [2, 3],
    double: 0.5,
    doubleArray: [1.5, 2.5],
    string: "foo",
    nullString: null,
    emptyString: "",
    stringArray: ["bar", "baz"],
    stringArrayOfNull: [null, null],
    emptyBooleanArray: [],
    emptyIntArray: [],
    emptyDoubleArray: [],
    emptyStringArray: [],
    nullBooleanArray: null,
    nullIntArray: null,
    nullDoubleArray: null,
    nullStringArray: null,
    mixedArray: [1, 1.5],
    byte: 1,
    short: 1,
    float: 0.5,
    long: 1.0,
    char: "f",
  };

  // Make a copy
  let outerObject = JSON.parse(JSON.stringify(innerObject));

  outerObject.object = innerObject;
  outerObject.nullObject = null;
  outerObject.objectArray = [null, innerObject];
  outerObject.objectArrayOfNull = [null, null];
  outerObject.emptyObjectArray = [];
  outerObject.nullObjectArray = null;
  return outerObject;
}

function send_test_message(scope, type) {
  let outerObject = get_test_message();
  outerObject.type = type;

  get_dispatcher(scope).sendRequest(outerObject);
}

function get_dispatcher(scope) {
  if (scope === 'global') {
    return EventDispatcher.instance;
  }
  if (scope === 'window') {
    let win = Services.wm.getMostRecentWindow("navigator:browser");
    return EventDispatcher.for(win);
  }
  ok(false, "Invalid scope argument: " + scope);
}

function dispatch_test_message(scope, type) {
  let data = get_test_message();
  get_dispatcher(scope).dispatch(type, data);
}

function send_message_for_response(scope, type, mode, key) {
  get_dispatcher(scope).sendRequestForResult({
    type: type,
    mode: mode,
    key: key,
  }).then(result => {
    do_check_eq(mode, "success");
    check_response(key, result);
  }, error => {
    do_check_eq(mode, "error");
    check_response(key, error);
  });
}

function dispatch_message_for_response(scope, type, mode, key) {
  // Gecko thread callbacks should be synchronous.
  let sync = (type === "Robocop:TestGeckoResponse");
  let dispatching = true;

  get_dispatcher(scope).dispatch(type, {
    mode: mode,
    key: key,
  }, {
    onSuccess: result => {
      do_check_eq(mode, "success");
      check_response(key, result);
      sync && do_check_eq(dispatching, true);
    },
    onError: error => {
      do_check_eq(mode, "error");
      check_response(key, error);
      sync && do_check_eq(dispatching, true);
    },
  });

  dispatching = false;
}

function check_response(key, response) {
  let expected = get_test_message()[key];
  if (expected !== null && typeof expected === "object") {
    listener._checkObject(response);
  } else {
    do_check_eq(response, expected);
  }
}

let listener = {
  _checkObject: function (obj) {
    do_check_eq(obj.boolean, true);
    do_check_eq(obj.booleanArray.length, 2);
    do_check_eq(obj.booleanArray[0], false);
    do_check_eq(obj.booleanArray[1], true);

    do_check_eq(obj.int, 1);
    do_check_eq(obj.intArray.length, 2);
    do_check_eq(obj.intArray[0], 2);
    do_check_eq(obj.intArray[1], 3);

    do_check_eq(obj.double, 0.5);
    do_check_eq(obj.doubleArray.length, 2);
    do_check_eq(obj.doubleArray[0], 1.5);
    do_check_eq(obj.doubleArray[1], 2.5);

    do_check_eq(obj.string, "foo");
    do_check_eq(obj.nullString, null);
    do_check_eq(obj.emptyString, "");

    do_check_eq(obj.stringArray.length, 2);
    do_check_eq(obj.stringArray[0], "bar");
    do_check_eq(obj.stringArray[1], "baz");

    do_check_eq(obj.stringArrayOfNull.length, 2);
    do_check_eq(obj.stringArrayOfNull[0], null);
    do_check_eq(obj.stringArrayOfNull[1], null);

    do_check_eq(obj.emptyBooleanArray.length, 0);
    do_check_eq(obj.emptyIntArray.length, 0);
    do_check_eq(obj.emptyDoubleArray.length, 0);
    do_check_eq(obj.emptyStringArray.length, 0);

    do_check_eq(obj.nullBooleanArray, null);
    do_check_eq(obj.nullIntArray, null);
    do_check_eq(obj.nullDoubleArray, null);
    do_check_eq(obj.nullStringArray, null);

    do_check_eq(obj.mixedArray.length, 2);
    do_check_eq(obj.mixedArray[0], 1);
    do_check_eq(obj.mixedArray[1], 1.5);
  },

  onEvent: function (event, data, callback) {
    do_check_eq(event, this._type);
    this._callCount++;

    this._checkObject(data);

    this._checkObject(data.object);
    do_check_eq(data.nullObject, null);

    do_check_eq(data.objectArray.length, 2);
    do_check_eq(data.objectArray[0], null);
    this._checkObject(data.objectArray[1]);

    do_check_eq(data.objectArrayOfNull.length, 2);
    do_check_eq(data.objectArrayOfNull[0], null);
    do_check_eq(data.objectArrayOfNull[1], null);

    do_check_eq(data.emptyObjectArray.length, 0);
    do_check_eq(data.nullObjectArray, null);
  }
};

let callbackListener = {
  onEvent: function (event, data, callback) {
    do_check_eq(event, this._type);
    this._callCount++;

    if (data.mode == "success") {
      callback.onSuccess(get_test_message()[data.key]);
    } else if (data.mode == "error") {
      callback.onError(get_test_message()[data.key]);
    } else {
      ok(false, "Response type should be valid: " + data.response);
    }
  }
};

function register_js_events(scope, type, callbackType) {
  listener._type = type;
  listener._callCount = 0;

  callbackListener._type = callbackType;
  callbackListener._callCount = 0;

  get_dispatcher(scope).registerListener(listener, type);
  get_dispatcher(scope).registerListener(callbackListener, callbackType);
}

function unregister_js_events(scope, type, callbackType, count) {
  get_dispatcher(scope).unregisterListener(listener, type);
  get_dispatcher(scope).unregisterListener(callbackListener, callbackType);

  // Listeners should have been called `count` times total.
  do_check_eq(listener._callCount + callbackListener._callCount, count);
}

function finish_test() {
  do_test_finished();
}
