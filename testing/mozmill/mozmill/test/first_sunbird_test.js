var elementslib = {}; Components.utils.import('resource://mozmill/modules/elementslib.js', elementslib);
var mozmill = {}; Components.utils.import('resource://mozmill/modules/mozmill.js', mozmill);

var setupModule = function (module) {
  var _w = mozmill.wm.getMostRecentWindow("calendarMainWindow");
  module.calendar = new mozmill.controller.MozMillController(_w);
}

var test_foo = function(){
 calendar.type(new elementslib.ID(_w.document, 'unifinder-search-field'), "test");
 calendar.sleep(5000);
}
