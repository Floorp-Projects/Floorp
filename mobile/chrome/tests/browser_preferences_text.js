// Bug 571866 - --browser-chrome test for fennec preferences and text values

var gTests = [];
var gCurrentTest = null;
var expected = {
  "aboutButton": {"label": "Go to Page", "tagName": "button", "value": "About Fennec", "element_id": "prefs-about-button"},
  "homepage": {"element_id": "prefs-homepage", "value": "Start page", "label": "Fennec Start",
                    "home_page": "prefs-homepage-default", "home_page_label": "Fennec Start", 
                    "blank_page": "prefs-homepage-none", "blank_page_label": "Blank Page",
                    "current_page": "prefs-homepage-currentpage", "current_page_label": "Use Current Page"},
  "doneButton": {"label": "Done", "tagName": "button"},
  "contentRegion": {"element_id": "prefs-content", "label": "Content"},
  "imageRegion": {"pref": "permissions.default.image", "value": "Show images", "anonid": "input", "localName": "checkbox"},
  "jsRegion": {"pref": "javascript.enabled", "value": "Enable JavaScript", "anonid": "input", "localName": "checkbox"},
  "privacyRegion": {"element_id": "prefs-privacy", "label": "Privacy & Security"},
  "cookiesRegion": {"pref": "network.cookie.cookieBehavior",  "value": "Allow cookies", "anonid": "input", "localName": "checkbox"},
  "passwordsRegion": {"pref": "signon.rememberSignons", "value": "Remember passwords", "anonid": "input", "localName": "checkbox"},
  "clearDataButton": {"value": "Clear private data", "element_id": "prefs-clear-data", "label": "Clear", "tagName": "button"}
};

function getPreferencesElements() {
   var prefElements = {};
   prefElements.panelOpen = document.getElementById("tool-panel-open");
   prefElements.panelClose = document.getElementById("tool-panel-close");
   prefElements.panelContainer = document.getElementById("panel-container");
   prefElements.homeButton = document.getElementById("prefs-homepage-options");
   prefElements.doneButton = document.getElementById("select-buttons-done");
   prefElements.homePageControl = document.getElementById("prefs-homepage");
   prefElements.selectContainer = document.getElementById("select-container");
   return prefElements;
}

function getHeight(element){
  let style = window.getComputedStyle(element, null);
  return style.height;
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
  _currentTab : null,

  run: function(){
    var prefs = getPreferencesElements();
    // 1.Click preferences to view prefs
    prefs.panelOpen.click();

    // 2. For each prefs *verify text *the button/option type *verify height of each field to be the same
    is(prefs.panelContainer.hidden, false, "Preferences should be visible");

    // Check whether the Preferences pan title is Preferences
    var prefTitle = document.getElementById("prefs-header");
    is(prefTitle.firstChild.value, "Preferences", "The title must be preferences");

    var prefsList = document.getElementById("prefs-messages");

    // Check for *About page*
    let about = expected.aboutButton;
    var aboutRegion = document.getAnonymousElementByAttribute(prefsList, "title", "About Fennec");
    var aboutLabel = document.getAnonymousElementByAttribute(aboutRegion, "class", "preftitle");
    is(aboutLabel.value, about.value, "Label Text for About Fennec");
    var aboutButton = document.getElementById(about.element_id);
    is(aboutButton.label, about.label, "Text for about button is Go to Page");
    is(aboutButton.tagName, about.tagName, "The About Fennec input must be a button");

    // Check for *Startpage*
    let homepage = expected.homepage;
    var homepageRegion = document.getElementById(homepage.element_id);
    var homepageLabel = document.getAnonymousElementByAttribute(homepageRegion, "class", "preftitle");
    is(homepageLabel.value, homepage.value, "Label Text for Start Page");
    is(prefs.homeButton.label, homepage.label, "The text on the homepage label must be Fennec Start");
    prefs.homeButton.click();

    is(prefs.selectContainer.hidden, false, "Homepage select dialog must be visible");
    is(document.getElementById(homepage.home_page).label, 
       homepage.home_page_label, " First option is Fennec Start");
    is(document.getElementById(homepage.blank_page).label,
      homepage.blank_page_label, " Second option is Blank page");
    is(document.getElementById(homepage.current_page).label,
      homepage.current_page_label, " First option is Custom page");

    let doneButton = expected.doneButton;
    is(prefs.doneButton.label, doneButton.label, "The text for the Done button");
    is(prefs.doneButton.tagName, doneButton.tagName, "The type of the Done input is button");
    prefs.doneButton.click();
    is(prefs.selectContainer.hidden, true, "Homepage select dialog must be closed");


    let content = expected.contentRegion;
    var contentRegion = document.getElementById(content.element_id);
    todo_is(contentRegion.label, content.label, "The Content region");

    // Check for *Show images*
    images = expected.imageRegion;
    var imageRegion = document.getAnonymousElementByAttribute(contentRegion, "pref", images.pref); 
    var imageLabel = document.getAnonymousElementByAttribute(imageRegion, "class", "preftitle");
    is(imageLabel.value, images.value, "Show images label");
    var imageButton = document.getAnonymousElementByAttribute(imageRegion, "anonid", images.anonid);
    is(imageButton.localName, images.localName, "Show images checkbox check");
    // Checkbox or radiobutton?

    // Check for *Enable javascript*
    let js = expected.jsRegion;
    var jsRegion = document.getAnonymousElementByAttribute(contentRegion, "pref", js.pref); 
    var jsLabel = document.getAnonymousElementByAttribute(jsRegion, "class", "preftitle");
    is(jsLabel.value, js.value, "Enable JavaScript Label"); 
    var jsButton = document.getAnonymousElementByAttribute(jsRegion, "anonid", js.anonid); 
    is(jsButton.localName, js.localName, "Enable JavaScript checkbox check"); 
    // Checkbox or radiobutton?

    let privacyRegion = expected.privacyRegion;
    var prefsPrivacy = document.getElementById(privacyRegion.element_id); 
    todo_is(prefsPrivacy.label, privacyRegion.label, "The privacy and security region"); 

    // Check for *Allow cookies*    
    let cookies = expected.cookiesRegion;
    var cookiesRegion = document.getAnonymousElementByAttribute(prefsPrivacy, "pref", cookies.pref); 
    var cookiesLabel = document.getAnonymousElementByAttribute(cookiesRegion, "class", "preftitle");
    is(cookiesLabel.value, cookies.value, "Allow cookies label"); 
    var cookiesButton = document.getAnonymousElementByAttribute(cookiesRegion, "anonid", cookies.anonid);
    is(cookiesButton.localName, cookies.localName, "Allow cookies checkbox check"); 
    // Checkbox or radiobutton?

    // Check for *Remember password*
    let passwords = expected.passwordsRegion;
    var passwordsRegion = document.getAnonymousElementByAttribute(prefsPrivacy, "pref", passwords.pref); 
    var passwordLabel = document.getAnonymousElementByAttribute(passwordsRegion, "class", "preftitle");
    is(passwordLabel.value, passwords.value, "Remember Passwords Label");
    var passwordsButton = document.getAnonymousElementByAttribute(passwordsRegion, "anonid", passwords.anonid);
    is(passwordsButton.localName, passwords.localName, "Allow cookies checkbox check");
    // Checkbox or radiobutton?

    // Check for *Clear Private Data*
    let clearData = expected.clearDataButton;
    var clearDataRegion = prefsPrivacy.lastChild;
    var clearDataLabel = document.getAnonymousElementByAttribute(clearDataRegion, "class", "preftitle");
    is(clearDataLabel.value, clearData.value, "Clear Private Data Label");
    var clearDataButton = document.getElementById(clearData.element_id);
    is(clearDataButton.label, clearData.label, "Label for Clear Private Data button");
    is(clearDataButton.tagName, clearData.tagName, "Check for Clear Private Data button type");

    // 3. Verify content & privacy and security reasons are gray and of same height
    // Check for height
    let aboutRegionHeight = getHeight(aboutRegion);
    let imageRegionHeight = getHeight(imageRegion);
    let cookiesRegionHeight = getHeight(cookiesRegion);
    ok(aboutRegionHeight == getHeight(homepageRegion), "The About Page and the Fennec Start are of same height");
    ok(imageRegionHeight == getHeight(jsRegion), "The fields of Content region are of same height");
    ok((cookiesRegionHeight == getHeight(passwordsRegion)) && (cookiesRegionHeight == getHeight(clearDataRegion)),
       "The fields of Privacy & Security are of same height");
    ok(aboutRegionHeight == imageRegionHeight, "The fields of Content Region and above are of same height");
    ok(aboutRegionHeight == cookiesRegionHeight, "The fields of Privacy & Security and above are of same height");
    ok(imageRegionHeight == cookiesRegionHeight, "The fields of Content and Privacy & Security are of same height");

    prefs.panelClose.click()
    is(document.getElementById("panel-container").hidden, true, "Preferences panel should be closed");
    Browser.closeTab(gCurrentTest._currentTab);
    runNextTest();
  }
});

