const Cc = Components.classes;
const Ci = Components.interfaces;

var addedTopic = "xpcom-category-entry-added";
var removedTopic = "xpcom-category-entry-removed";
var testCategory = "bug-test-category";
var testEntry = "@mozilla.org/bug-test-entry;1";

var testValue= "check validity";
var result = "";
var expected = "add remove add remove ";
var timer;

var observer = {
  QueryInterface: function(iid) {
    if (iid.equals(Ci.nsISupports) || iid.equals(Ci.nsIObserver))
      return this;

    throw Components.results.NS_ERROR_NO_INTERFACE;
  },

  observe: function(subject, topic, data) {
    if (topic == "timer-callback") {
      do_check_eq(result, expected);

      var observerService = Cc["@mozilla.org/observer-service;1"].getService(Ci.nsIObserverService);
      observerService.removeObserver(this, addedTopic);
      observerService.removeObserver(this, removedTopic);

      do_test_finished();

      timer = null;
    }

    if (subject.QueryInterface(Ci.nsISupportsCString).data != testEntry || data != testCategory)
      return;

    if (topic == addedTopic)
      result += "add ";
    else if (topic == removedTopic)
      result += "remove ";
  }
};

function run_test() {
  do_test_pending();

  var observerService = Cc["@mozilla.org/observer-service;1"].getService(Ci.nsIObserverService);
  observerService.addObserver(observer, addedTopic, false);
  observerService.addObserver(observer, removedTopic, false);

  var categoryManager = Cc["@mozilla.org/categorymanager;1"].getService(Ci.nsICategoryManager);
  categoryManager.addCategoryEntry(testCategory, testEntry, testValue, false, true);
  categoryManager.addCategoryEntry(testCategory, testEntry, testValue, false, true);
  categoryManager.deleteCategoryEntry(testCategory, testEntry, false);

  timer = Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer);
  timer.init(observer, 0, timer.TYPE_ONE_SHOT);
}
