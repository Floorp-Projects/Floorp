var jum = {}; Components.utils.import('resource://mozmill/modules/jum.js', jum);

var testPythonCallPost = function() {
  var status = "post";
  mozmill.firePythonCallbackAfterRestart("postCallback", status);
}
