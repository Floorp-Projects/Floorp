var mozmill = {}; Components.utils.import('resource://mozmill/modules/mozmill.js', mozmill);
var elementslib = {}; Components.utils.import('resource://mozmill/modules/elementslib.js', elementslib);

var setupModule = function(module) {
  module.controller = mozmill.getBrowserController();
}


var testKnowRightsDoesNotExist = function(){
  if (!persisted.test) {
    throw "Persisted is not working."
  }
  var e = new elementslib.Lookup(controller.window.document, '/id("main-window")/id("browser")/id("appcontent")/id("content")/anon({"anonid":"tabbox"})/anon({"anonid":"panelcontainer"})/[0]/{"value":"about-rights"}/{"accesskey":"K"}');
  
  if (e.exists()) {
    throw 'First run "know our rights" dialog is up after restart';
  }
}