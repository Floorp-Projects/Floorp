
var elementslib = {}; 
Components.utils.import('resource://mozmill/modules/elementslib.js', elementslib);
var mozmill = {}; 
Components.utils.import('resource://mozmill/modules/mozmill.js', mozmill);

var test_assert = function(){
 var controller = mozmill.getBrowserController();
 controller.open('http://www.google.com');
 controller.sleep(2000);
 controller.type(new elementslib.Name(controller.window.content.document, 'q'), 'Mozilla');
 controller.assertValue(new elementslib.Name(controller.window.content.document, 'q'), 'Mozilla');
 controller.keypress(new elementslib.Name(controller.window.content.document, 'btnG'), 13);
 controller.sleep(2000);
 controller.type(new elementslib.Name(controller.window.content.document, 'q'), 'Mozilla Summit');
 controller.click(new elementslib.Name(controller.window.content.document, 'btnG'));
 controller.sleep(2000);
 
 // chrome
 controller.assertNode(new elementslib.ID(controller.window.document, 'searchbar'));
 controller.type(new elementslib.ID(controller.window.document, 'searchbar'), 'Mozilla Summit');
 controller.assertValue(new elementslib.ID(controller.window.document, 'searchbar'), 'Mozilla Summit');
 controller.keypress(new elementslib.ID(controller.window.document, 'searchbar'), '13');
}