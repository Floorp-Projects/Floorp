// Bug 571866 - --browser-chrome test for fennec preferences and text values

var gTests = [];
var gCurrentTest = null;
var expected = {
  "aboutButton": {"tagName": "button", "element_id": "prefs-about-button"},
  "homepage": {"element_id": "prefs-homepage",
               "home_page": "prefs-homepage-default",
               "blank_page": "prefs-homepage-none",
               "current_page": "prefs-homepage-currentpage"},
  "doneButton": {"tagName": "button"},
  "contentRegion": {"element_id": "prefs-content"},
  "imageRegion": {"pref": "permissions.default.image", "anonid": "input", "localName": "checkbox"},
  "jsRegion": {"pref": "javascript.enabled", "anonid": "input", "localName": "checkbox"},
  "privacyRegion": {"element_id": "prefs-privacy" },
  "cookiesRegion": {"pref": "network.cookie.cookieBehavior", "anonid": "input", "localName": "checkbox"},
  "passwordsRegion": {"pref": "signon.rememberSignons", "anonid": "input", "localName": "checkbox"},
  "clearDataButton": {"element_id": "prefs-clear-data", "tagName": "button"}
};

function getPreferencesElements() {
   let prefElements = {};
   prefElements.panelOpen = document.getElementById("tool-panel-open");
   prefElements.panelContainer = document.getElementById("panel-container");
   prefElements.homeButton = document.getElementById("prefs-homepage-options");
   prefElements.doneButton = document.getElementById("select-buttons-done");
   prefElements.homePageControl = document.getElementById("prefs-homepage");
   prefElements.selectContainer = document.getElementById("menulist-container");
   return prefElements;
}

function test() {
  // The "runNextTest" approach is async, so we need to call "waitForExplicitFinish()"
  // We call "finish()" when the tests are finished
  waitForExplicitFinish();

  // Start the tests
  runNextTest();
}
//------------------------------------------------------------------------------
// Iterating tests by shifting test out one by one as runNextTest is called.
function runNextTest() {
  // Run the next test until all tests completed
  if (gTests.length > 0) {
    gCurrentTest = gTests.shift();
    info(gCurrentTest.desc);
    gCurrentTest.run();
  }
  else {
    // Cleanup. All tests are completed at this point
    finish();
  }
}

// -----------------------------------------------------------------------------------------
// Verify preferences and text
gTests.push({
  desc: "Verify Preferences and Text",

  run: function(){
    var prefs = getPreferencesElements();
    // 1.Click preferences to view prefs
    prefs.panelOpen.click();

    // 2. For each prefs *verify text *the button/option type *verify height of each field to be the same
    is(prefs.panelContainer.hidden, false, "Preferences should be visible");

    var prefsList = document.getElementById("prefs-messages");

    // Check for *About page*
    let about = expected.aboutButton;
    var aboutRegion = document.getAnonymousElementByAttribute(prefsList, "title", "About Fennec");
    var aboutButton = document.getElementById(about.element_id);
    is(aboutButton.tagName, about.tagName, "The About Fennec input must be a button");

    // Check for *Startpage*
    let homepage = expected.homepage;
    var homepageRegion = document.getElementById(homepage.element_id);
    prefs.homeButton.click();

    is(prefs.selectContainer.hidden, false, "Homepage select dialog must be visible");

    EventUtils.synthesizeKey("VK_ESCAPE", {}, window);
    is(prefs.selectContainer.hidden, true, "Homepage select dialog must be closed");

    let content = expected.contentRegion;
    var contentRegion = document.getElementById(content.element_id);

    // Check for *Show images*
    var images = expected.imageRegion;
    var imageRegion = document.getAnonymousElementByAttribute(contentRegion, "pref", images.pref); 
    var imageButton = document.getAnonymousElementByAttribute(imageRegion, "anonid", images.anonid);
    is(imageButton.localName, images.localName, "Show images checkbox check");
    // Checkbox or radiobutton?

    // Check for *Enable javascript*
    let js = expected.jsRegion;
    var jsRegion = document.getAnonymousElementByAttribute(contentRegion, "pref", js.pref); 
    var jsButton = document.getAnonymousElementByAttribute(jsRegion, "anonid", js.anonid); 
    is(jsButton.localName, js.localName, "Enable JavaScript checkbox check"); 
    // Checkbox or radiobutton?

    let privacyRegion = expected.privacyRegion;
    var prefsPrivacy = document.getElementById(privacyRegion.element_id);

    // Check for *Allow cookies*    
    let cookies = expected.cookiesRegion;
    var cookiesRegion = document.getAnonymousElementByAttribute(prefsPrivacy, "pref", cookies.pref); 
    var cookiesButton = document.getAnonymousElementByAttribute(cookiesRegion, "anonid", cookies.anonid);
    is(cookiesButton.localName, cookies.localName, "Allow cookies checkbox check"); 
    // Checkbox or radiobutton?

    // Check for *Remember password*
    let passwords = expected.passwordsRegion;
    var passwordsRegion = document.getAnonymousElementByAttribute(prefsPrivacy, "pref", passwords.pref); 
    var passwordsButton = document.getAnonymousElementByAttribute(passwordsRegion, "anonid", passwords.anonid);
    is(passwordsButton.localName, passwords.localName, "Allow cookies checkbox check");
    // Checkbox or radiobutton?

    // Check for *Clear Private Data*
    let clearData = expected.clearDataButton;
    var clearDataRegion = prefsPrivacy.lastChild;
    var clearDataButton = document.getElementById(clearData.element_id);
    is(clearDataButton.tagName, clearData.tagName, "Check for Clear Private Data button type");

    BrowserUI.hidePanel();
    is(document.getElementById("panel-container").hidden, true, "Preferences panel should be closed");
    runNextTest();
  }
});

