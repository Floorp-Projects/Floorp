const LOCATIONS = [
  // Normal pages
  {url : "http://www.google.de", type: "id", value : "logo"},
  {url : "https://addons.mozilla.org/en-US/firefox/?browse=featured", type: "id", value : "query"},
  {url : "http://addons.mozilla.org", type: "id", value : "query"},

  // FTP pages
  {url : "ftp://ftp.mozilla.org/pub/", type : "link", value : "firefox" },

  // Error pages
 {url : "https://mur.at", type: "id", value : "cert_domain_link"},
 {url : "http://www.mozilla.com/firefox/its-a-trap.html", type: "id", value : "ignoreWarningButton"},
 //{url : "https://mozilla.org/", type: "id", value : "getMeOutOfHereButton"}
];
var setupTest = function() {
  controller = mozmill.getBrowserController();
}

var testWaitForPageLoad = function() {

  /**
   * PART I - Check different types of pages
   */
  for each (var location in LOCATIONS) {
    controller.open(location.url);
    controller.waitForPageLoad();
  
    // Check that the expected element exists
    if (location.type) {
      var elem = null;
  
      switch (location.type) {
        case "link":
          elem = new elementslib.Link(controller.tabs.activeTab, location.value);
          break;
        case "name":
          elem = new elementslib.Name(controller.tabs.activeTab, location.value);
          break;
        case "id":
          elem = new elementslib.ID(controller.tabs.activeTab, location.value);
          break;
        default:
      }
  
      controller.assertNode(elem);
    }
  }
  
  /**
   * PART II - Test different parameter sets
   */ 
  var location = LOCATIONS[0];
  for (var i = 0; i < 7; i++) {
    controller.open(location.url);
  
    switch (i) {
      case 0:
        controller.waitForPageLoad(controller.tabs.activeTab);
        break;
      case 1:
        controller.waitForPageLoad(controller.tabs.activeTab, undefined, 10);
        break;
      case 2:
        controller.waitForPageLoad(controller.tabs.activeTab, "invalid");
        break;
      case 3:
        controller.waitForPageLoad(undefined, null, 100);
        break;
      case 4:
        controller.waitForPageLoad(null, undefined, 100);
        break;
      case 5:
        controller.waitForPageLoad("invalid", undefined);
        break;
      case 6:
        controller.waitForPageLoad(undefined, "invalid");
        break;
    }
  }
  
  /**
   * PART III - Check that we correctly handle timeouts for waitForPageLoad
   */
  try {
    controller.open(LOCATIONS[0].url);
    controller.waitForPageLoad(0);
  
    throw new Error("controller.waitForPageLoad() not timed out for timeout=0.");
  } catch (ex) {}

  /**
   * PART IV - Make sure we don't fail when clicking links on a page
   */ 
  controller.open("http://www.mozilla.org");
  controller.waitForPageLoad();

  var link = new elementslib.Link(controller.tabs.activeTab, "Get Involved");
  controller.click(link);
  controller.waitForPageLoad();

  var target = new elementslib.Name(controller.tabs.activeTab, "area");
  controller.waitForElement(target, 1000);

  /**
   * PART V - When waitForPageLoad is called when the page has already been loaded
   * we shouldn't fail
   */
  controller.open(LOCATIONS[0].url);
  controller.waitForPageLoad();
  controller.waitForPageLoad(500);

 
  /**
   * PART VI - Loading a page in another tab should wait for its completion
   */
  controller.open(LOCATIONS[1].url);
 
  controller.keypress(null, "t", {accelKey: true});
  controller.open(LOCATIONS[0].url);
 
  var firstTab = controller.tabs.getTab(0);
  var element = new elementslib.ID(firstTab, LOCATIONS[1].value);
  controller.waitForPageLoad(firstTab);
  controller.assertNode(element);
}

