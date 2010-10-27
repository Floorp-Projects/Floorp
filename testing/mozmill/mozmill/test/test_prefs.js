var elementslib = {}; Components.utils.import('resource://mozmill/modules/elementslib.js', elementslib);
var mozmill = {}; Components.utils.import('resource://mozmill/modules/mozmill.js', mozmill);
var controller = {};  Components.utils.import('resource://mozmill/modules/controller.js', controller);

var setupModule = function(module) {
  module.controller = new controller.MozMillController(mozmill.utils.getWindowByType("Browser:Preferences"));
}

var test_TabsTab = function() {
  // Click on Tab tab
  controller.click(new elementslib.Elem( controller.tabs.Tabs.button ));
  // Warn on close
  var warnElem = new elementslib.ID(controller.window.document, 'warnCloseMultiple');
  controller.waitForElement(warnElem);
  controller.click(warnElem);
  // Warn on opening many windows
  controller.click(new elementslib.ID(controller.window.document, 'warnOpenMany'));
  // Show the tab bar
  controller.click(new elementslib.ID(controller.window.document, 'showTabBar'));
  // Switch to new Tabs
  controller.click(new elementslib.ID(controller.window.document, 'switchToNewTabs'));
  controller.sleep(1000);
}

var test_ContentTab = function() {
  // Click on the proper Tab Button
  controller.click(new elementslib.Elem( controller.tabs.Content.button ));
  controller.sleep(1000);
  
  // disable "Block popups"
  controller.click(new elementslib.ID(controller.window.document, 'popupPolicy'));
  // disable "Load Images"
  controller.click(new elementslib.ID(controller.window.document, 'loadImages'));
  // disable JavaScript
  controller.click(new elementslib.ID(controller.window.document, 'enableJavaScript'));
  // disable Java
  controller.click(new elementslib.ID(controller.window.document, 'enableJava'));

  // Set Default Font
  // controller.click(new elementslib.ID(controller.window.document, 'defaultFont'))
  // controller.click(new elementslib.XPATH(controller.window.document, '/prefwindow[@id='BrowserPreferences']/prefpane[@id='paneContent']/xul:vbox'))
  
}

var test_ApplicationsTab = function() {
  e = new elementslib.Elem( controller.tabs.Applications.button );
  controller.click(e);  
  controller.sleep(500);
  
  // // Click on the search box
  // var node = controller.window.document.getAnonymousElementByAttribute(
  //    controller.window.document.getElementById('paneApplications').getElementsByTagName(
  //     'hbox')[0].getElementsByTagName('textbox')[0], 
  //     'class', 
  //     'textbox-input-box');
  //     
  // e = new elementslib.Elem(node.childNodes[0]);
  // 
  // controller.click(new elementslib.Elem(node.childNodes[0]));
  // controller.sleep(1000);
  
}

var test_PrivacyTab = function() {
  controller.click(new elementslib.Elem( controller.tabs.Privacy.button ));
  controller.sleep(500);
  controller.click(new elementslib.ID(controller.window.document, 'rememberHistoryDays'));
  controller.click(new elementslib.ID(controller.window.document, 'rememberForms'));
  controller.click(new elementslib.ID(controller.window.document, 'rememberDownloads'));
  controller.click(new elementslib.ID(controller.window.document, 'acceptThirdParty'));
  controller.click(new elementslib.ID(controller.window.document, 'acceptCookies'));
  controller.click(new elementslib.ID(controller.window.document, 'alwaysClear'));
  controller.click(new elementslib.ID(controller.window.document, 'askBeforeClear'));
}
