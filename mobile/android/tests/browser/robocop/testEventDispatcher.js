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
    null: null,
    string: "foo",
    nullString: null,
    emptyString: "",
    stringArray: ["bar", "baz"],
    emptyBooleanArray: [],
    emptyIntArray: [],
    emptyDoubleArray: [],
    emptyStringArray: [],
    // XXX enable when we remove NativeJSObject
    // mixedArray: [1, 1.5],
  };

  // Make a copy
  let outerObject = JSON.parse(JSON.stringify(innerObject));

  outerObject.object = innerObject;
  outerObject.nullObject = null;
  outerObject.objectArray = [null, innerObject];
  outerObject.emptyObjectArray = [];
  return outerObject;
}

function send_test_message(type) {
  let outerObject = get_test_message();
  outerObject.type = type;

  Messaging.sendRequest(outerObject);
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

function send_message_for_response(type, response) {
  Messaging.sendRequestForResult({
    type: type,
    response: response,
  }).then(result => do_check_eq(result, response),
          error => do_check_eq(error, response));
}

function dispatch_message_for_response(scope, type, response) {
  get_dispatcher(scope).dispatch(type, {
    response: response,
  }, {
    onSuccess: result => do_check_eq(result, response),
    onError: error => do_check_eq(error, response),
  });
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

    do_check_eq(obj.emptyBooleanArray.length, 0);
    do_check_eq(obj.emptyIntArray.length, 0);
    do_check_eq(obj.emptyDoubleArray.length, 0);
    do_check_eq(obj.emptyStringArray.length, 0);
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
    do_check_eq(data.emptyObjectArray.length, 0);
  }
};

let callbackListener = {
  onEvent: function (event, data, callback) {
    do_check_eq(event, this._type);
    this._callCount++;

    if (data.response == "success") {
      callback.onSuccess(data.response);
    } else if (data.response == "error") {
      callback.onError(data.response);
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

function unregister_js_events(scope, type, callbackType) {
  get_dispatcher(scope).unregisterListener(listener, type);
  get_dispatcher(scope).unregisterListener(callbackListener, callbackType);

  // Listeners should have been called 6 times total.
  do_check_eq(listener._callCount, 2);
  do_check_eq(callbackListener._callCount, 4);
}

function finish_test() {
  do_test_finished();
}
