var gEditor;
function onLoad() {
  if (wysiwyg) {
    // Initialize the WYSIWYG content editor.  We could just call
    // HTMLArea.replace() or .replaceAll(), but we want a reference
    // to the editor so we can make it update the content textarea
    // when the user switches modes.
    var content = document.getElementById("content");
    try {
      gEditor = new HTMLArea(content);
      gEditor.registerPlugin(FullPage);
      gEditor.generate();
    }
    catch(e) {
      throw(e);
    }
    switchToTab("edit");
  }
  else
    switchToTab("hack");
  checkOriginMode();
}


function synchronizeEditModes() {
  // Hack to make sure the content and its WYSIWYG representation
  // are synchronized every time the view mode changes (so any edits
  // get reflected in the other views) by toggling the edit mode
  // from the current mode to the other one and back.
  // Wrapped in a try/catch block because this throws an error sometimes
  // the first time this code is loaded, although it doesn't seem to matter.

  if (!wysiwyg)
    return;

  try {
    gEditor.setMode(); gEditor.setMode();
  }
  catch(e) {}
}


const TABS = ["edit", "hack", "original", "modified", "diff", "save"];
var gActiveTab;
function switchToTab(newTab) {
  if (newTab == "edit") {
    try { gEditor.setMode("wysiwyg"); } catch(e) {}
  }
  else if (newTab == "hack") {
    try { gEditor.setMode("textmode"); } catch(e) {}
  }
  else if (newTab == "original") {}
  else if (newTab == "modified") {
    synchronizeEditModes();
    var modifiedPanel = document.getElementById('modifiedPanel');
    var content_file = document.getElementById('content_file');
    if (content_file.value) {
      modifiedPanel.innerHTML = '<iframe id="modified" name="modified"></iframe>';
      submitForm("modified", "regurgitate");
    }
    else {
      modifiedPanel.innerHTML = document.getElementById('content').value;
    }
  }
  else if (newTab == "diff") {
    synchronizeEditModes();
    submitForm("diffPanel", "diff", { raw: "1" });
  }
  else if (newTab == "save") {
    synchronizeEditModes();
  }
  gActiveTab = newTab;
  updateTabState();
}

function updateTabState() {
  for ( var i=0 ; i<TABS.length ; i++ ) {
    var tab = TABS[i];

    // Disable the active tab and enable the others.
    var button = document.getElementById(tab + "Tab");
    if (tab == gActiveTab) {
      addClass(button, "active");
      button.disabled = true;
    }
    else {
      removeClass(button, "active");
      if (gOriginMode == "upload" && (tab == "edit" || tab == "hack"))
        // The edit and hack buttons should remain disabled no matter what
        // if the origin of the changes is an uploaded file.
        button.disabled = true;
      else
        button.disabled = false;
    }

    // Show the active panel and hide the others.
    var panel = document.getElementById(tab + "Panel");
    var visibility = (tab == gActiveTab);
    if (tab == "edit" || tab == "hack") {
      // The edit and hack tabs share the edit panel, so we have to make sure
      // either tab makes the edit panel visible.
      panel = document.getElementById("editPanel");
      visibility = gActiveTab == "edit" || gActiveTab == "hack";
    }
    panel.style.visibility = visibility ? "visible" : "hidden";
  }
}


// The origin mode specifies where the changes are coming from and is either
// "inline" for changes typed into the inline textarea on the page or
// "upload" for changes in a file being uploaded via the file upload control.
var gOriginMode = "inline";

function checkOriginMode() {
  // Checks to see if there's something in the file upload control, and updates
  // the origin mode accordingly.  We should just listen for onchange or blur
  // events on the control, but that doesn't work because of Mozilla bug 4033,
  // so instead we just invoke this function every tenth of a second.

  var content_file = document.getElementById('content_file');

  if (gOriginMode == "upload" && !content_file.value) {
    gOriginMode = "inline";

    // Update the view state to reenable the edit and hack buttons.
    updateTabState();
  }
  else if (content_file.value && gOriginMode == "inline") {
    gOriginMode = "upload";

    // Modified and Diff view modes make no sense until the user is finished
    // entering the filename, but we can't find that out with onchange or onblur
    // because of Mozilla bug 4033, so we force the user into Original view mode
    // until they switch to another mode manually.
    switchToTab("original");

    // Switching to Original view mode updates the view state, so no need
    // to do it manually here to disable the edit and hack buttons.  However,
    // if bug 4033 gets fixed at some point, and we no longer force the user
    // into Original view mode, then we'll need to update the view state here.
    // updateTabState();
  }

  window.setTimeout(checkOriginMode, 100);
}


function submitForm(target, action, params) {
  // Hacks the form with a specific action (and any extra params) and submits it.
  var form = document.getElementById('form');

  if (target)
    form.target = target;

  var action = createHiddenField("action", action);
  form.appendChild(action);

  var paramNodes = new Array();
  for (var name in params) {
    var value = params[name];
    var param = createHiddenField(name, value);
    form.appendChild(param);
    paramNodes.push(param);
  }

  form.submit();

  form.target = null;
  form.removeChild(action);
  for ( var i=0 ; i<paramNodes.length ; i++ ) {
    form.removeChild(paramNodes[i]);
  }
}

function addClass(node, className) {
  if (!node || !className)
    return;

  if (!node.className) {
    node.className = className;
    return true;
  }

  var classes = node.className.split(/\s+/);

  // Make sure the class isn't already in the node first.
  for (var i=0 ; i<classes.length ; i++ ) {
    if (classes[i] == className)
      return true;
  }

  // Add the class to the list of classes for the node.
  classes.push(className);
  node.className = classes.join(" ");
  return true;
}

function removeClass(node, className) {
  if (!node || !node.className || !className)
    return;

  // This could probably be done with a regular expression:
  //var regex = new RegExp("(^|\\s+)" + className + "(\\s+|$)");
  //node.className = node.className.replace(regex, " ");

  var classes = node.className.split(/\s+/);
  var newClasses = "";
  for (var i=0 ; i<classes.length ; i++ ) {
    if (classes[i] != className)
      newClasses += " " + classes[i];
  }

  node.className = newClasses;
  return true;
}

function createHiddenField(name, value) {
  var field = document.createElement('input');
  field.type = "hidden";
  field.name = name;
  field.value = value;
  return field;
}
