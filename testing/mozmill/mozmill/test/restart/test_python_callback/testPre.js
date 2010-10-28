var jum = {}; Components.utils.import('resource://mozmill/modules/jum.js', jum);

var testPythonCallNow = function() {
  var state = "pre"
  mozmill.firePythonCallback("nowCallback", state)
}

var testPythonFail = function() {
    mozmill.firePythonCallback("failCallback", null);
}
