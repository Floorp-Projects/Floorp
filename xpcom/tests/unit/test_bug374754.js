ChromeUtils.import("resource://gre/modules/Services.jsm");

var addedTopic = "xpcom-category-entry-added";
var removedTopic = "xpcom-category-entry-removed";
var testCategory = "bug-test-category";
var testEntry = "@mozilla.org/bug-test-entry;1";

var testValue = "check validity";
var result = "";
var expected = "add remove add remove ";
var timer;

var observer = {
  QueryInterface: ChromeUtils.generateQI(["nsIObserver"]),

  observe(subject, topic, data) {
    if (topic == "timer-callback") {
      Assert.equal(result, expected);

      Services.obs.removeObserver(this, addedTopic);
      Services.obs.removeObserver(this, removedTopic);

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

  Services.obs.addObserver(observer, addedTopic);
  Services.obs.addObserver(observer, removedTopic);

  var categoryManager = Cc["@mozilla.org/categorymanager;1"].getService(Ci.nsICategoryManager);
  categoryManager.addCategoryEntry(testCategory, testEntry, testValue, false, true);
  categoryManager.addCategoryEntry(testCategory, testEntry, testValue, false, true);
  categoryManager.deleteCategoryEntry(testCategory, testEntry, false);

  timer = Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer);
  timer.init(observer, 0, timer.TYPE_ONE_SHOT);
}
