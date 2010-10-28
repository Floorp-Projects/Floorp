var elementslib = {}; Components.utils.import('resource://mozmill/modules/elementslib.js', elementslib);
var mozmill = {}; Components.utils.import('resource://mozmill/modules/mozmill.js', mozmill);

// var test_DiggIterator = function () {
//   // Bring up browser controller.
//   var controller = mozmill.getBrowserController();
//   controller.open('http://www.digg.com');
//   controller.waitForElement(new elementslib.Elem( controller.window.content.document.body ));
//   controller.sleep(5000);
//   var links = controller.window.content.document.getElementsByTagName('a');
//  
//   for (var i = 0; i<links.length; i++){
//     controller.click(new elementslib.Elem( links[i] ));
//     controller.sleep(3000);
//     links = controller.window.content.document.getElementsByTagName('a');
//     controller.sleep(2000);
//   }
// }