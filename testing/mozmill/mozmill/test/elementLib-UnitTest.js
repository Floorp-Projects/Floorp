var elementslib = {}; 
Components.utils.import('resource://mozmill/modules/elementslib.js', elementslib);
var mozmill = {}; 
Components.utils.import('resource://mozmill/modules/mozmill.js', mozmill);

var test_foo = function(){
  var controller = mozmill.getBrowserController();
  controller.open("http://en-us.www.mozilla.com/en-US/firefox/3.0.1/firstrun/");

  // Shorthand
  var content = controller.window.content.document;
  var chrome = controller.window.document;

  // Test content XPath detection
  var image = new elementslib.XPath(controller.window.content.document, "/html/body[@id='firstrun']/div[@id='wrapper']/div[@id='doc']/div[@id='main-feature']/h2/img");
  controller.waitForElement(image);
  controller.sleep(1000);

  // Test content - ID detection
  e = new elementslib.ID(content, "return");
  controller.click(e);
  controller.sleep(2000);

  // Now go back - test chrome button anonymous lookup detection
  e = new elementslib.Lookup(chrome, '/id("main-window")/id("navigator-toolbox")/id("nav-bar")/id("unified-back-forward-button")/id("back-button")/anon({"anonid":"button"})');
  controller.click(e);
  controller.waitForElement(image);

  // Test chrome ID detection
  e = new elementslib.ID(chrome, "star-button");
  controller.click(e);
  controller.sleep(1000);

  // Test chrome drop down interaction using lookup detection - change searchbox to Yahoo
  e = new elementslib.Lookup(chrome, '/id("main-window")/id("navigator-toolbox")/id("nav-bar")/id("search-container")/id("searchbar")/anon({"anonid":"searchbar-textbox"})/anon({"anonid":"searchbar-engine-button"})/anon({"anonid":"searchbar-popup"})/id("Yahoo")');
  controller.click(e);
  controller.sleep(1000);

  // Type in searchbox and hit return
  e = new elementslib.Lookup(chrome, '/id("main-window")/id("navigator-toolbox")/id("nav-bar")/id("search-container")/id("searchbar")/anon({"anonid":"searchbar-textbox"})/anon({"class":"autocomplete-textbox-container"})/anon({"anonid":"textbox-input-box"})/anon({"anonid":"input"})');
  controller.type(e, "mozilla");
  controller.keypress(e, 13);
}