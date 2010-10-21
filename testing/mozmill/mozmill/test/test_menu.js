var setupTest = function() {
  controller = mozmill.getBrowserController();

  // Create a new menu instance for the context menu
  contextMenu = controller.getMenu("#contentAreaContextMenu");
}

var testMenuAPI = function() {
  // Open a new tab by using the main menu
  controller.mainMenu.click("#menu_newNavigatorTab");

  controller.open("http://www.mozilla.org");
  controller.waitForPageLoad();

  // Enter text in a text field and select all via the context menu
  var search = new elementslib.ID(controller.tabs.activeTab, "q");
  controller.type(search, "mozmill");
  contextMenu.select("#context-selectall", search);

  // Reopen the context menu and check the 'Paste' menu item
  contextMenu.open(search);
  var state = contextMenu.getItem("#context-viewimage");
  controller.assert(function() {
    return state.getNode().hidden;
  }, "Context menu entry 'View Image' is not visible");

  // Remove the text by selecting 'Undo'
  contextMenu.keypress("VK_DOWN", {});
  contextMenu.keypress("VK_ENTER", {});
  contextMenu.close();

  controller.assert(function() {
    return search.getNode().value == "";
  }, "Text field has been emptied.");
}

