Components.utils.import("resource://gre/modules/Messaging.jsm");

let java = new JavaBridge(this);

do_register_cleanup(() => {
  java.disconnect();
});
do_test_pending();

function add_request_listener(message) {
  RequestService.addListener(function (data) {
    return { result: data + "bar" };
  }, message);
}

function add_exception_listener(message) {
  RequestService.addListener(function (data) {
    throw "error!";
  }, message);
}

function add_second_request_listener(message) {
  let exceptionCaught = false;

  try {
    RequestService.addListener(() => {}, message);
  } catch (e) {
    exceptionCaught = true;
  }

  do_check_true(exceptionCaught);
}

function remove_request_listener(message) {
  RequestService.removeListener(message);
}

function finish_test() {
  do_test_finished();
}
