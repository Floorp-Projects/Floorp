var columns_simpletree =
[
  { name: "name", label: "Name", key: true, properties: "one two" },
  { name: "address", label: "Address" }
];

var columns_hiertree =
[
  { name: "name", label: "Name", primary: true, key: true, properties: "one two" },
  { name: "address", label: "Address" },
  { name: "planet", label: "Planet" },
  { name: "gender", label: "Gender", cycler: true }
];

// XXXndeakin still to add some tests for:
//   cycler columns, checkbox cells, progressmeter cells

// this test function expects a tree to have 8 rows in it when it isn't
// expanded. The tree should only display four rows at a time. If editable,
// the cell at row 1 and column 0 must be editable, and the cell at row 2 and
// column 1 must not be editable.
function testtag_tree(treeid, treerowinfoid, seltype, columnstype, testid)
{
  netscape.security.PrivilegeManager.enablePrivilege("UniversalXPConnect");

  var multiple = (seltype == "multiple");
  var editable = false;

  var tree = document.getElementById(treeid);
  var treerowinfo = document.getElementById(treerowinfoid);
  var rowInfo = convertDOMtoTreeRowInfo(treerowinfo, 0, { value: -1 });
  var columnInfo = (columnstype == "simple") ? columns_simpletree : columns_hiertree;

  is(tree.view.selection.currentColumn, null, testid + " initial currentColumn");
  is(tree.selType, seltype == "multiple" ? "" : seltype, testid + " seltype");

  // note: the functions below should be in this order due to changes made in later tests
  
  // select the first column in cell selection mode so that the selection
  // functions can be tested
  if (seltype == "cell")
    tree.view.selection.currentColumn = tree.columns[0];

  testtag_tree_columns(tree, columnInfo, testid);
  testtag_tree_TreeSelection(tree, testid, multiple);
  testtag_tree_TreeSelection_UI(tree, testid, multiple);
  if (seltype == "cell")
    testtag_tree_TreeSelection_UI_cell(tree, testid, rowInfo);

  testtag_tree_TreeView(tree, testid, rowInfo);

  is(tree.editable, editable, "editable");
  // currently, the editable flag means that tree editing cannot be invoked
  // by the user. However, editing can still be started with a script.
  is(tree.editingRow, -1, testid + " initial editingRow");
  is(tree.editingColumn, null, testid + " initial editingColumn");

  testtag_tree_UI_editing(tree, testid);

  var ecolumn = tree.columns[0];
  tree.startEditing(1, ecolumn);
  is(tree.editingRow, 1, testid + " startEditing editingRow");
  is(tree.editingColumn, ecolumn, testid + " startEditing editingColumn");
  is(tree.getAttribute("editing"), "true", testid + " startEditing editing attribute");

  tree.stopEditing(true);
  is(tree.editingRow, -1, testid + " stopEditing editingRow");
  is(tree.editingColumn, null, testid + " stopEditing editingColumn");
  is(tree.hasAttribute("editing"), false, testid + " stopEditing editing attribute");

  tree.startEditing(-1, ecolumn);
  is(tree.editingRow == -1 && tree.editingColumn == null, true, testid + " startEditing -1 editingRow");
  tree.startEditing(15, ecolumn);
  is(tree.editingRow == -1 && tree.editingColumn == null, true, testid + " startEditing 15 editingRow");
  tree.startEditing(1, null);
  is(tree.editingRow == -1 && tree.editingColumn == null, true, testid + " startEditing null column editingRow");
  tree.startEditing(2, tree.columns[1]);
  is(tree.editingRow == -1 && tree.editingColumn == null, true, testid + " startEditing non editable cell editingRow");

  tree.startEditing(1, ecolumn);
  var inputField = tree.inputField;
  is(inputField instanceof Components.interfaces.nsIDOMXULTextBoxElement, true, testid + "inputField");
  inputField.value = "Changed Value";
  tree.stopEditing(true);
  is(tree.view.getCellText(1, ecolumn), "Changed Value", testid + "edit cell accept");

  // this cell cannot be edited
  tree.startEditing(1, ecolumn);
  inputField.value = "Second Value";
  tree.stopEditing(false);
  is(tree.view.getCellText(1, ecolumn), "Changed Value", testid + "edit cell no accept");

  // do the sorting tests last as it will cause the rows to rearrange
  testtag_tree_TreeView_rows_sort(tree, testid, rowInfo);

  SimpleTest.finish();
}

function testtag_tree_columns(tree, expectedColumns, testid)
{
  testid += " ";

  var columns = tree.columns;

  is(columns instanceof TreeColumns, true, testid + "columns is a TreeColumns");
  is(columns.count, expectedColumns.length, testid + "TreeColumns count");
  is(columns.length, expectedColumns.length, testid + "TreeColumns length");

  var treecols = tree.getElementsByTagName("treecols")[0];
  var treecol = treecols.getElementsByTagName("treecol");

  var x = 0;
  var primary = null, sorted = null, key = null;
  for (var c = 0; c < expectedColumns.length; c++) {
    var adjtestid = testid + " column " + c + " ";
    var column = columns[c];
    var expectedColumn = expectedColumns[c];
    is(columns.getColumnAt(c), column, adjtestid + "getColumnAt");
    is(columns.getNamedColumn(expectedColumn.name), column, adjtestid + "getNamedColumn");
    is(columns.getColumnFor(treecol[c]), column, adjtestid + "getColumnFor");
    if (expectedColumn.primary)
      primary = column;
    if (expectedColumn.sorted)
      sorted = column;
    if (expectedColumn.key)
      key = column;

    // XXXndeakin on Windows and Linux, the last column is one pixel to the
    // left of where is should be. Could just be a rounding issue.
    var adj = (c == expectedColumns.length - 1) ? 1 : 0;
    is(column.x + adj >= x, true, adjtestid + "position is after last column " +
       column.x + "," + column.width + "," + x);
    is(column.width > 0, true, adjtestid + "width is greater than 0");
    x = column.x + column.width;

    // now check the TreeColumn properties
    is(column instanceof TreeColumn, true, adjtestid + "is a TreeColumn");
    is(column.element, treecol[c], adjtestid + "element is treecol");
    is(column.columns, columns, adjtestid + "columns is TreeColumns");
    is(column.id, expectedColumn.name, adjtestid + "name");
    is(column.index, c, adjtestid + "index");
    is(column.primary, primary == column, adjtestid + "column is primary");

    is(column.cycler, "cycler" in expectedColumn && expectedColumn.cycler,
                  adjtestid + "column is cycler");
    is(column.selectable, true, adjtestid + "column is selectable");
    is(column.editable, "editable" in expectedColumn && expectedColumn.editable,
                  adjtestid + "column is editable");

    is(column.type, "type" in expectedColumn ? expectedColumn.type : 1, adjtestid + "type");

    is(column.getPrevious(), c > 0 ? columns[c - 1] : null, adjtestid + "getPrevious");
    is(column.getNext(), c < columns.length - 1 ? columns[c + 1] : null, adjtestid + "getNext");

    // check the view's getColumnProperties method
    var properties = Components.classes["@mozilla.org/supports-array;1"].
                       createInstance(Components.interfaces.nsISupportsArray);
    tree.view.getColumnProperties(column, properties);
    properties = convertProperties(properties);
    var expectedProperties = expectedColumn.properties;
    is(properties,  expectedProperties ? expectedProperties : "", adjtestid + "getColumnProperties");
  }

  is(columns.getFirstColumn(), columns[0], testid + "getFirstColumn");
  is(columns.getLastColumn(), columns[columns.length - 1], testid + "getLastColumn");
  is(columns.getPrimaryColumn(), primary, testid + "getPrimaryColumn");
  is(columns.getSortedColumn(), sorted, testid + "getSortedColumn");
  is(columns.getKeyColumn(), key, testid + "getKeyColumn");

  is(columns.getColumnAt(-1), null, testid + "getColumnAt under");
  is(columns.getColumnAt(columns.length), null, testid + "getColumnAt over");
  is(columns.getNamedColumn(""), null, testid + "getNamedColumn null");
  is(columns.getNamedColumn("unknown"), null, testid + "getNamedColumn unknown");
  is(columns.getColumnFor(null), null, testid + "getColumnFor null");
  is(columns.getColumnFor(tree), null, testid + "getColumnFor other");
}

function testtag_tree_TreeSelection(tree, testid, multiple)
{
  testid += " selection ";

  var selection = tree.view.selection;
  is(selection instanceof Components.interfaces.nsITreeSelection, true,
                testid + "selection is a TreeSelection");
  is(selection.single, !multiple, testid + "single");

  testtag_tree_TreeSelection_State(tree, testid + "initial", -1, []);
  is(selection.shiftSelectPivot, -1, testid + "initial shiftSelectPivot");

  selection.currentIndex = 2;
  testtag_tree_TreeSelection_State(tree, testid + "set currentIndex", 2, []);
  tree.currentIndex = 3;
  testtag_tree_TreeSelection_State(tree, testid + "set tree.currentIndex", 3, []);

  // test the select() method, which should deselect all rows and select
  // a single row
  selection.select(1);
  testtag_tree_TreeSelection_State(tree, testid + "select 1", 1, [1]);
  selection.select(3);
  testtag_tree_TreeSelection_State(tree, testid + "select 2", 3, [3]);
  selection.select(3);
  testtag_tree_TreeSelection_State(tree, testid + "select same", 3, [3]);

  selection.currentIndex = 1;
  testtag_tree_TreeSelection_State(tree, testid + "set currentIndex with single selection", 1, [3]);

  tree.currentIndex = 2;
  testtag_tree_TreeSelection_State(tree, testid + "set tree.currentIndex with single selection", 2, [3]);

  // check the toggleSelect method. In single selection mode, it only toggles on when
  // there isn't currently a selection.
  selection.toggleSelect(2);
  testtag_tree_TreeSelection_State(tree, testid + "toggleSelect 1", 2, multiple ? [2, 3] : [3]);
  selection.toggleSelect(2);
  selection.toggleSelect(3);
  testtag_tree_TreeSelection_State(tree, testid + "toggleSelect 2", 3, []);

  // the current index doesn't change after a selectAll, so it should still be set to 1
  // selectAll has no effect on single selection trees
  selection.currentIndex = 1;
  selection.selectAll();
  testtag_tree_TreeSelection_State(tree, testid + "selectAll 1", 1, multiple ? [0, 1, 2, 3, 4, 5, 6 , 7] : []);
  selection.toggleSelect(2);
  testtag_tree_TreeSelection_State(tree, testid + "toggleSelect after selectAll", 2,
                                   multiple ? [0, 1, 3, 4, 5, 6, 7] : [2]);
  selection.clearSelection();
  testtag_tree_TreeSelection_State(tree, testid + "clearSelection", 2, []);
  selection.toggleSelect(3);
  selection.toggleSelect(1);
  if (multiple) {
    selection.selectAll();
    testtag_tree_TreeSelection_State(tree, testid + "selectAll 2", 1, [0, 1, 2, 3, 4, 5, 6, 7]);
  }
  selection.currentIndex = 2;
  selection.clearSelection();
  testtag_tree_TreeSelection_State(tree, testid + "clearSelection after selectAll", 2, []);

  // XXXndeakin invertSelection isn't implemented
  //  selection.invertSelection();

  is(selection.shiftSelectPivot, -1, testid + "shiftSelectPivot set to -1");

  // rangedSelect and clearRange set the currentIndex to the endIndex. The
  // shiftSelectPivot property will be set to startIndex.
  selection.rangedSelect(1, 3, false);
  testtag_tree_TreeSelection_State(tree, testid + "rangedSelect no augment",
                                   multiple ? 3 : 2, multiple ? [1, 2, 3] : []);
  is(selection.shiftSelectPivot, multiple ? 1 : -1,
                testid + "shiftSelectPivot after rangedSelect no augment");
  if (multiple) {
    selection.select(1);
    selection.rangedSelect(0, 2, true);
    testtag_tree_TreeSelection_State(tree, testid + "rangedSelect augment", 2, [0, 1, 2]);
    is(selection.shiftSelectPivot, 0, testid + "shiftSelectPivot after rangedSelect augment");

    selection.clearRange(1, 3);
    testtag_tree_TreeSelection_State(tree, testid + "rangedSelect augment", 3, [0]);

    // check that rangedSelect can take a start value higher than end
    selection.rangedSelect(3, 1, false);
    testtag_tree_TreeSelection_State(tree, testid + "rangedSelect reverse", 1, [1, 2, 3]);
    is(selection.shiftSelectPivot, 3, testid + "shiftSelectPivot after rangedSelect reverse");

    // check that setting the current index doesn't change the selection
    selection.currentIndex = 0;
    testtag_tree_TreeSelection_State(tree, testid + "currentIndex with range selection", 0, [1, 2, 3]);
  }

  // both values of rangedSelect may be the same
  selection.rangedSelect(2, 2, false);
  testtag_tree_TreeSelection_State(tree, testid + "rangedSelect one row", 2, [2]);
  is(selection.shiftSelectPivot, 2, testid + "shiftSelectPivot after selecting one row");

  if (multiple) {
    selection.rangedSelect(2, 3, true);

    // a start index of -1 means from the last point
    selection.rangedSelect(-1, 0, true);
    testtag_tree_TreeSelection_State(tree, testid + "rangedSelect -1 existing selection", 0, [0, 1, 2, 3]);
    is(selection.shiftSelectPivot, 2, testid + "shiftSelectPivot after -1 existing selection");

    selection.currentIndex = 2;
    selection.rangedSelect(-1, 0, false);
    testtag_tree_TreeSelection_State(tree, testid + "rangedSelect -1 from currentIndex", 0, [0, 1, 2]);
    is(selection.shiftSelectPivot, 2, testid + "shiftSelectPivot -1 from currentIndex");
  }

  // XXXndeakin need to test out of range values but these don't work properly
/*
  selection.select(-1);
  testtag_tree_TreeSelection_State(tree, testid + "rangedSelect augment -1", -1, []);

  selection.select(8);
  testtag_tree_TreeSelection_State(tree, testid + "rangedSelect augment 8", 3, [0]);
*/
}

function testtag_tree_TreeSelection_UI(tree, testid, multiple)
{
  testid += " selection UI ";

  var selection = tree.view.selection;
  selection.clearSelection();
  selection.currentIndex = 0;
  tree.focus();

  // check that cursor up and down keys navigate up and down
  // select event fires after a delay so don't expect it. The reason it fires after a delay
  // is so that cursor navigation allows quicking skimming over a set of items without
  // actually firing events in-between, improving performance. The select event will only
  // be fired on the row where the cursor stops.
  synthesizeKeyExpectEvent("VK_DOWN", {}, tree, "!select", "key down");
  testtag_tree_TreeSelection_State(tree, testid + "key down", 1, [1], 0);

  synthesizeKeyExpectEvent("VK_UP", {}, tree, "!select", "key up");
  testtag_tree_TreeSelection_State(tree, testid + "key up", 0, [0], 0);

  synthesizeKeyExpectEvent("VK_UP", {}, tree, "!select", "key up at start");
  testtag_tree_TreeSelection_State(tree, testid + "key up at start", 0, [0], 0);

  // pressing down while the last row is selected should not fire a select event,
  // as the selection won't have changed. Also the view is not scrolled in this case.
  selection.select(7);
  synthesizeKeyExpectEvent("VK_DOWN", {}, tree, "!select", "key down at end");
  testtag_tree_TreeSelection_State(tree, testid + "key down at end", 7, [7], 0);

  // pressing keys while at the edge of the visible rows should scroll the list
  tree.treeBoxObject.scrollToRow(4);
  selection.select(4);
  synthesizeKeyExpectEvent("VK_UP", {}, tree, "!select", "key up with scroll");
  is(tree.treeBoxObject.getFirstVisibleRow(), 3, testid + "key up with scroll");

  tree.treeBoxObject.scrollToRow(0);
  selection.select(3);
  synthesizeKeyExpectEvent("VK_DOWN", {}, tree, "!select", "key down with scroll");
  is(tree.treeBoxObject.getFirstVisibleRow(), 1, testid + "key down with scroll");
  
  // accel key and cursor movement adjust currentIndex but should not change
  // the selection. In single selection mode, the selection will not change,
  // but instead will just scroll up or down a line
  tree.treeBoxObject.scrollToRow(0);
  selection.select(1);
  synthesizeKeyExpectEvent("VK_DOWN", { accelKey: true }, tree, "!select", "key down with accel");
  testtag_tree_TreeSelection_State(tree, testid + "key down with accel", multiple ? 2 : 1, [1]);
  if (!multiple)
    is(tree.treeBoxObject.getFirstVisibleRow(), 1, testid + "key down with accel and scroll");

  tree.treeBoxObject.scrollToRow(4);
  selection.select(4);
  synthesizeKeyExpectEvent("VK_UP", { accelKey: true }, tree, "!select", "key up with accel");
  testtag_tree_TreeSelection_State(tree, testid + "key up with accel", multiple ? 3 : 4, [4]);
  if (!multiple)
    is(tree.treeBoxObject.getFirstVisibleRow(), 3, testid + "key up with accel and scroll");

  // do this three times, one for each state of pageUpOrDownMovesSelection,
  // and then once with the accel key pressed
  for (var t = 0; t < 3; t++) {
    var testidmod = (t == 2) ? " with accel" : (t == 1) ? " rev" : "";
    var keymod = (t == 2) ? { accelKey: true } : { };

    var moveselection = tree.pageUpOrDownMovesSelection;
    if (t == 2)
      moveselection = !moveselection;

    tree.treeBoxObject.scrollToRow(4);
    selection.currentIndex = 6;
    selection.select(6);
    var expected = moveselection ? 4 : 6;
    synthesizeKeyExpectEvent("VK_PAGE_UP", keymod, tree, "!select", "key page up");
    testtag_tree_TreeSelection_State(tree, testid + "key page up" + testidmod,
                                     expected, [expected], moveselection ? 4 : 0);

    expected = moveselection ? 0 : 6;
    synthesizeKeyExpectEvent("VK_PAGE_UP", keymod, tree, "!select", "key page up again");
    testtag_tree_TreeSelection_State(tree, testid + "key page up again" + testidmod,
                                     expected, [expected], 0);

    expected = moveselection ? 0 : 6;
    synthesizeKeyExpectEvent("VK_PAGE_UP", keymod, tree, "!select", "key page up at start");
    testtag_tree_TreeSelection_State(tree, testid + "key page up at start" + testidmod,
                                     expected, [expected], 0);

    tree.treeBoxObject.scrollToRow(0);
    selection.currentIndex = 1;
    selection.select(1);
    expected = moveselection ? 3 : 1;
    synthesizeKeyExpectEvent("VK_PAGE_DOWN", keymod, tree, "!select", "key page down");
    testtag_tree_TreeSelection_State(tree, testid + "key page down" + testidmod,
                                     expected, [expected], moveselection ? 0 : 4);

    expected = moveselection ? 7 : 1;
    synthesizeKeyExpectEvent("VK_PAGE_DOWN", keymod, tree, "!select", "key page down again");
    testtag_tree_TreeSelection_State(tree, testid + "key page down again" + testidmod,
                                     expected, [expected], 4);

    expected = moveselection ? 7 : 1;
    synthesizeKeyExpectEvent("VK_PAGE_DOWN", keymod, tree, "!select", "key page down at start");
    testtag_tree_TreeSelection_State(tree, testid + "key page down at start" + testidmod,
                                     expected, [expected], 4);

    if (t < 2)
      tree.pageUpOrDownMovesSelection = !tree.pageUpOrDownMovesSelection;
  }

  tree.treeBoxObject.scrollToRow(4);
  selection.select(6);
  synthesizeKeyExpectEvent("VK_HOME", {}, tree, "!select", "key home");
  testtag_tree_TreeSelection_State(tree, testid + "key home", 0, [0], 0);

  tree.treeBoxObject.scrollToRow(0);
  selection.select(1);
  synthesizeKeyExpectEvent("VK_END", {}, tree, "!select", "key end");
  testtag_tree_TreeSelection_State(tree, testid + "key end", 7, [7], 4);

  // in single selection mode, the selection doesn't change in this case
  tree.treeBoxObject.scrollToRow(4);
  selection.select(6);
  synthesizeKeyExpectEvent("VK_HOME", { accelKey: true }, tree, "!select", "key home with accel");
  testtag_tree_TreeSelection_State(tree, testid + "key home with accel", multiple ? 0 : 6, [6], 0);

  tree.treeBoxObject.scrollToRow(0);
  selection.select(1);
  synthesizeKeyExpectEvent("VK_END", { accelKey: true }, tree, "!select", "key end with accel");
  testtag_tree_TreeSelection_State(tree, testid + "key end with accel", multiple ? 7 : 1, [1], 4);

  // next, test cursor navigation with selection. Here the select event will be fired
  selection.select(1);
  var eventExpected = multiple ? "select" : "!select";
  synthesizeKeyExpectEvent("VK_DOWN", { shiftKey: true }, tree, eventExpected, "key shift down to select");
  testtag_tree_TreeSelection_State(tree, testid + "key shift down to select",
                                   multiple ? 2 : 1, multiple ? [1, 2] : [1]);
  is(selection.shiftSelectPivot, multiple ? 1 : -1,
                testid + "key shift down to select shiftSelectPivot");
  synthesizeKeyExpectEvent("VK_UP", { shiftKey: true }, tree, eventExpected, "key shift up to unselect");
  testtag_tree_TreeSelection_State(tree, testid + "key shift up to unselect", 1, [1]);
  is(selection.shiftSelectPivot, multiple ? 1 : -1,
                testid + "key shift up to unselect shiftSelectPivot");
  if (multiple) {
    synthesizeKeyExpectEvent("VK_UP", { shiftKey: true }, tree, "select", "key shift up to select");
    testtag_tree_TreeSelection_State(tree, testid + "key shift up to select", 0, [0, 1]);
    is(selection.shiftSelectPivot, 1, testid + "key shift up to select shiftSelectPivot");
    synthesizeKeyExpectEvent("VK_DOWN", { shiftKey: true }, tree, "select", "key shift down to unselect");
    testtag_tree_TreeSelection_State(tree, testid + "key shift down to unselect", 1, [1]);
    is(selection.shiftSelectPivot, 1, testid + "key shift down to unselect shiftSelectPivot");
  }

  // do this twice, one for each state of pageUpOrDownMovesSelection, however
  // when selecting with the shift key, pageUpOrDownMovesSelection is ignored
  // and the selection always changes
  var lastidx = tree.view.rowCount - 1;
  for (var t = 0; t < 2; t++) {
    var testidmod = (t == 0) ? "" : " rev";

    // If the top or bottom visible row is the current row, pressing shift and
    // page down / page up selects one page up or one page down. Otherwise, the
    // selection is made to the top or bottom of the visible area.
    tree.treeBoxObject.scrollToRow(lastidx - 3);
    selection.currentIndex = 6;
    selection.select(6);
    synthesizeKeyExpectEvent("VK_PAGE_UP", { shiftKey: true }, tree, eventExpected, "key shift page up");
    testtag_tree_TreeSelection_State(tree, testid + "key shift page up" + testidmod,
                                     multiple ? 4 : 6, multiple ? [4, 5, 6] : [6]);
    if (multiple) {
      synthesizeKeyExpectEvent("VK_PAGE_UP", { shiftKey: true }, tree, "select", "key shift page up again");
      testtag_tree_TreeSelection_State(tree, testid + "key shift page up again" + testidmod,
                                       0, [0, 1, 2, 3, 4, 5, 6]);
      // no change in the selection, so no select event should be fired
      synthesizeKeyExpectEvent("VK_PAGE_UP", { shiftKey: true }, tree, "!select", "key shift page up at start");
      testtag_tree_TreeSelection_State(tree, testid + "key shift page up at start" + testidmod,
                                       0, [0, 1, 2, 3, 4, 5, 6]);
      // deselect by paging down again
      synthesizeKeyExpectEvent("VK_PAGE_DOWN", { shiftKey: true }, tree, "select", "key shift page down deselect");
      testtag_tree_TreeSelection_State(tree, testid + "key shift page down deselect" + testidmod,
                                       3, [3, 4, 5, 6]);
    }

    tree.treeBoxObject.scrollToRow(1);
    selection.currentIndex = 2;
    selection.select(2);
    synthesizeKeyExpectEvent("VK_PAGE_DOWN", { shiftKey: true }, tree, eventExpected, "key shift page down");
    testtag_tree_TreeSelection_State(tree, testid + "key shift page down" + testidmod,
                                     multiple ? 4 : 2, multiple ? [2, 3, 4] : [2]);
    if (multiple) {
      synthesizeKeyExpectEvent("VK_PAGE_DOWN", { shiftKey: true }, tree, "select", "key shift page down again");
      testtag_tree_TreeSelection_State(tree, testid + "key shift page down again" + testidmod,
                                       7, [2, 3, 4, 5, 6, 7]);
      synthesizeKeyExpectEvent("VK_PAGE_DOWN", { shiftKey: true }, tree, "!select", "key shift page down at start");
      testtag_tree_TreeSelection_State(tree, testid + "key shift page down at start" + testidmod,
                                       7, [2, 3, 4, 5, 6, 7]);
      synthesizeKeyExpectEvent("VK_PAGE_UP", { shiftKey: true }, tree, "select", "key shift page up deselect");
      testtag_tree_TreeSelection_State(tree, testid + "key shift page up deselect" + testidmod,
                                       4, [2, 3, 4]);
    }

    // test when page down / page up is pressed when the view is scrolled such
    // that the selection is not visible
    if (multiple) {
      tree.treeBoxObject.scrollToRow(3);
      selection.currentIndex = 1;
      selection.select(1);
      synthesizeKeyExpectEvent("VK_PAGE_DOWN", { shiftKey: true }, tree, eventExpected,
                               "key shift page down with view scrolled down");
      testtag_tree_TreeSelection_State(tree, testid + "key shift page down with view scrolled down" + testidmod,
                                       6, [1, 2, 3, 4, 5, 6], 3);

      tree.treeBoxObject.scrollToRow(2);
      selection.currentIndex = 6;
      selection.select(6);
      synthesizeKeyExpectEvent("VK_PAGE_UP", { shiftKey: true }, tree, eventExpected,
                               "key shift page up with view scrolled up");
      testtag_tree_TreeSelection_State(tree, testid + "key shift page up with view scrolled up" + testidmod,
                                       2, [2, 3, 4, 5, 6], 2);

      tree.treeBoxObject.scrollToRow(2);
      selection.currentIndex = 0;
      selection.select(0);
      // don't expect the select event, as the selection won't have changed
      synthesizeKeyExpectEvent("VK_PAGE_UP", { shiftKey: true }, tree, "!select",
                               "key shift page up at start with view scrolled down");
      testtag_tree_TreeSelection_State(tree, testid + "key shift page up at start with view scrolled down" + testidmod,
                                       0, [0], 0);

      tree.treeBoxObject.scrollToRow(0);
      selection.currentIndex = 7;
      selection.select(7);
      // don't expect the select event, as the selection won't have changed
      synthesizeKeyExpectEvent("VK_PAGE_DOWN", { shiftKey: true }, tree, "!select",
                               "key shift page down at end with view scrolled up");
      testtag_tree_TreeSelection_State(tree, testid + "key shift page down at end with view scrolled up" + testidmod,
                                       7, [7], 4);
    }

    tree.pageUpOrDownMovesSelection = !tree.pageUpOrDownMovesSelection;
  }

  tree.treeBoxObject.scrollToRow(4);
  selection.select(5);
  synthesizeKeyExpectEvent("VK_HOME", { shiftKey: true }, tree, eventExpected, "key shift home");
  testtag_tree_TreeSelection_State(tree, testid + "key shift home",
                                   multiple ? 0 : 5, multiple ? [0, 1, 2, 3, 4, 5] : [5], multiple ? 0 : 4);

  tree.treeBoxObject.scrollToRow(0);
  selection.select(3);
  synthesizeKeyExpectEvent("VK_END", { shiftKey: true }, tree, eventExpected, "key shift end");
  testtag_tree_TreeSelection_State(tree, testid + "key shift end",
                                   multiple ? 7 : 3, multiple ? [3, 4, 5, 6, 7] : [3], multiple ? 4 : 0);

  // pressing space selects a row, pressing accel + space unselects a row
  selection.select(2);
  selection.currentIndex = 4;
  synthesizeKeyExpectEvent(" ", {}, tree, "select", "key space on");
  // in single selection mode, space shouldn't do anything
  testtag_tree_TreeSelection_State(tree, testid + "key space on", 4, multiple ? [2, 4] : [2]);

  if (multiple) {
    synthesizeKeyExpectEvent(" ", { accelKey: true }, tree, "select", "key space off");
    testtag_tree_TreeSelection_State(tree, testid + "key space off", 4, [2]);
  }

  // check that clicking on a row selects it
  tree.treeBoxObject.scrollToRow(0);
  selection.select(2);
  selection.currentIndex = 2;
  if (0) { // XXXndeakin disable these tests for now
    mouseOnCell(tree, 1, tree.columns[1], "mouse on row");
    testtag_tree_TreeSelection_State(tree, testid + "mouse on row", 1, [1], 0,
                                     tree.selType == "cell" ? tree.columns[1] : null);
  }
}

function testtag_tree_UI_editing(tree, testid)
{
  testid += " editing UI ";

  // check editing UI
  var ecolumn = tree.columns[1];
  var inputField = tree.inputField;
  tree.startEditing(0, ecolumn);

  var ci = tree.currentIndex;

  // cursor navigation should not change the selection while editing
  var testKey = function(key) {
    synthesizeKeyExpectEvent(key, {}, tree, "!select", "key " + key + " with editing");
    is(tree.editingRow == 0 && tree.editingColumn == ecolumn && tree.currentIndex == ci,
                  true, testid + "key " + key + " while editing");
  }

  testKey("VK_DOWN");
  testKey("VK_UP");
  testKey("VK_PAGE_DOWN");
  testKey("VK_PAGE_UP");
  testKey("VK_HOME");
  testKey("VK_END");

  // XXXndeakin figure out how to send characters to the textbox
  // inputField.inputField.focus()
  // synthesizeKeyExpectEvent(inputField.inputField, "b", null, "");
  // tree.stopEditing(true);
  // is(tree.view.getCellText(0, ecolumn), "b", testid + "edit cell");

  if (1) // XXXndeakin disable these tests for now
    return;

  tree.startEditing(0, ecolumn);
  inputField.value = "Value for Return";
  synthesizeKey("VK_RETURN", {});
  is(tree.view.getCellText(0, ecolumn), "Value for Return", testid + "edit cell return");

  tree.startEditing(0, ecolumn);
  inputField.value = "Value for Enter";
  synthesizeKey("VK_ENTER", {});
  is(tree.view.getCellText(0, ecolumn), "Value for Enter", testid + "edit cell enter");

  tree.startEditing(0, ecolumn);
  inputField.value = "Value for Escape";
  synthesizeKey("VK_ESCAPE", {});
  is(tree.view.getCellText(0, ecolumn), "Value for Enter", testid + "edit cell escape");
}

function testtag_tree_TreeSelection_UI_cell(tree, testid, rowInfo)
{
  testid += " selection UI cell ";

  var columns = tree.columns;
  var firstcolumn = columns[0];
  var secondcolumn = columns[1];
  var lastcolumn = columns[columns.length - 1];
  var secondlastcolumn = columns[columns.length - 2];
  var selection = tree.view.selection;

  selection.clearSelection();
  selection.currentIndex = -1;
  selection.currentColumn = firstcolumn;
  is(selection.currentColumn, firstcolumn, testid + " first currentColumn");

  // no selection yet so nothing should happen when the left and right cursor keys are pressed
  synthesizeKeyExpectEvent("VK_RIGHT", {}, tree, "!select", "key right no selection");
  testtag_tree_TreeSelection_State(tree, testid + "key right no selection", -1, [], null, firstcolumn);

  selection.currentColumn = secondcolumn;
  synthesizeKeyExpectEvent("VK_LEFT", {}, tree, "!select", "key left no selection");
  testtag_tree_TreeSelection_State(tree, testid + "key left no selection", -1, [], null, secondcolumn);

  selection.select(2);
  selection.currentIndex = 2;
  if (0) { // XXXndeakin disable these tests for now
    mouseOnCell(tree, 1, secondlastcolumn, "mouse on cell");
    testtag_tree_TreeSelection_State(tree, testid + "mouse on cell", 1, [1], null, secondlastcolumn);
  }

  tree.focus();
 
  // selection is set, so it should move when the left and right cursor keys are pressed
  tree.treeBoxObject.scrollToRow(0);
  selection.select(1);
  selection.currentIndex = 1;
  selection.currentColumn = secondcolumn;
  synthesizeKeyExpectEvent("VK_LEFT", {}, tree, "!select", "key left in second column");
  testtag_tree_TreeSelection_State(tree, testid + "key left in second column", 1, [1], 0, firstcolumn);

  synthesizeKeyExpectEvent("VK_LEFT", {}, tree, "!select", "key left in first column");
  testtag_tree_TreeSelection_State(tree, testid + "key left in first column", 1, [1], 0, firstcolumn);

  selection.currentColumn = secondlastcolumn;
  synthesizeKeyExpectEvent("VK_RIGHT", {}, tree, "!select", "key right in second last column");
  testtag_tree_TreeSelection_State(tree, testid + "key right in second last column", 1, [1], 0, lastcolumn);

  synthesizeKeyExpectEvent("VK_RIGHT", {}, tree, "!select", "key right in last column");
  testtag_tree_TreeSelection_State(tree, testid + "key right in last column", 1, [1], 0, lastcolumn);

  synthesizeKeyExpectEvent("VK_UP", {}, tree, "!select", "key up in second row");
  testtag_tree_TreeSelection_State(tree, testid + "key up in second row", 0, [0], 0, lastcolumn);

  synthesizeKeyExpectEvent("VK_UP", {}, tree, "!select", "key up in first row");
  testtag_tree_TreeSelection_State(tree, testid + "key up in first row", 0, [0], 0, lastcolumn);

  synthesizeKeyExpectEvent("VK_DOWN", {}, tree, "!select", "key down in first row");
  testtag_tree_TreeSelection_State(tree, testid + "key down in first row", 1, [1], 0, lastcolumn);

  var lastidx = tree.view.rowCount - 1;
  tree.treeBoxObject.scrollToRow(lastidx - 3);
  selection.select(lastidx);
  selection.currentIndex = lastidx;
  synthesizeKeyExpectEvent("VK_DOWN", {}, tree, "!select", "key down in last row");
  testtag_tree_TreeSelection_State(tree, testid + "key down in last row", lastidx, [lastidx], lastidx - 3, lastcolumn);

  synthesizeKeyExpectEvent("VK_HOME", {}, tree, "!select", "key home");
  testtag_tree_TreeSelection_State(tree, testid + "key home", 0, [0], 0, lastcolumn);

  synthesizeKeyExpectEvent("VK_END", {}, tree, "!select", "key end");
  testtag_tree_TreeSelection_State(tree, testid + "key end", lastidx, [lastidx], lastidx - 3, lastcolumn);

  for (var t = 0; t < 2; t++) {
    var testidmod = (t == 0) ? "" : " rev";

    // scroll to the end, subtract 3 because we want lastidx to appear
    // at the end of view
    tree.treeBoxObject.scrollToRow(lastidx - 3);
    selection.select(lastidx);
    selection.currentIndex = lastidx;
    var expectedrow = tree.pageUpOrDownMovesSelection ? lastidx - 3 : lastidx;
    synthesizeKeyExpectEvent("VK_PAGE_UP", {}, tree, "!select", "key page up");
    testtag_tree_TreeSelection_State(tree, testid + "key page up" + testidmod,
                                     expectedrow, [expectedrow],
                                     tree.pageUpOrDownMovesSelection ? lastidx - 3 : 0, lastcolumn);

    tree.treeBoxObject.scrollToRow(1);
    selection.select(1);
    selection.currentIndex = 1;
    var expectedrow = tree.pageUpOrDownMovesSelection ? 4 : 1;
    synthesizeKeyExpectEvent("VK_PAGE_DOWN", {}, tree, "!select", "key page down");
    testtag_tree_TreeSelection_State(tree, testid + "key page down" + testidmod,
                                     expectedrow, [expectedrow],
                                     tree.pageUpOrDownMovesSelection ? 1 : lastidx - 3, lastcolumn);

    tree.pageUpOrDownMovesSelection = !tree.pageUpOrDownMovesSelection;
  }

  // now check navigation when there is unselctable column
  secondcolumn.element.setAttribute("selectable", "false");
  secondcolumn.invalidate();
  is(secondcolumn.selectable, false, testid + "set selectable attribute");

  if (columns.length >= 3) {
    selection.select(3);
    selection.currentIndex = 3;
    // check whether unselectable columns are skipped over
    selection.currentColumn = firstcolumn;
    synthesizeKeyExpectEvent("VK_RIGHT", {}, tree, "!select", "key right unselectable column");
    testtag_tree_TreeSelection_State(tree, testid + "key right unselectable column",
                                     3, [3], null, secondcolumn.getNext());

    synthesizeKeyExpectEvent("VK_LEFT", {}, tree, "!select", "key left unselectable column");
    testtag_tree_TreeSelection_State(tree, testid + "key left unselectable column",
                                     3, [3], null, firstcolumn);
  }

  secondcolumn.element.removeAttribute("selectable");
  secondcolumn.invalidate();
  is(secondcolumn.selectable, true, testid + "clear selectable attribute");

  // check to ensure that navigation isn't allowed if the first column is not selectable
  selection.currentColumn = secondcolumn;
  firstcolumn.element.setAttribute("selectable", "false");
  firstcolumn.invalidate();
  synthesizeKeyExpectEvent("VK_LEFT", {}, tree, "!select", "key left unselectable first column");
  testtag_tree_TreeSelection_State(tree, testid + "key left unselectable first column",
                                   3, [3], null, secondcolumn);
  firstcolumn.element.removeAttribute("selectable");
  firstcolumn.invalidate();

  // check to ensure that navigation isn't allowed if the last column is not selectable
  selection.currentColumn = secondlastcolumn;
  lastcolumn.element.setAttribute("selectable", "false");
  lastcolumn.invalidate();
  synthesizeKeyExpectEvent("VK_RIGHT", {}, tree, "!select", "key right unselectable last column");
  testtag_tree_TreeSelection_State(tree, testid + "key right unselectable last column",
                                   3, [3], null, secondlastcolumn);
  lastcolumn.element.removeAttribute("selectable");
  lastcolumn.invalidate();

  // now check for cells with selectable false
  if (!rowInfo.rows[4].cells[1].selectable && columns.length >= 3) {
    // check whether unselectable cells are skipped over
    selection.select(4);
    selection.currentIndex = 4;

    selection.currentColumn = firstcolumn;
    synthesizeKeyExpectEvent("VK_RIGHT", {}, tree, "!select", "key right unselectable cell");
    testtag_tree_TreeSelection_State(tree, testid + "key right unselectable cell",
                                     4, [4], null, secondcolumn.getNext());

    synthesizeKeyExpectEvent("VK_LEFT", {}, tree, "!select", "key left unselectable cell");
    testtag_tree_TreeSelection_State(tree, testid + "key left unselectable cell",
                                     4, [4], null, firstcolumn);

    tree.treeBoxObject.scrollToRow(1);
    selection.select(3);
    selection.currentIndex = 3;
    selection.currentColumn = secondcolumn;

    synthesizeKeyExpectEvent("VK_DOWN", {}, tree, "!select", "key down unselectable cell");
    testtag_tree_TreeSelection_State(tree, testid + "key down unselectable cell",
                                     5, [5], 2, secondcolumn);

    tree.treeBoxObject.scrollToRow(4);
    synthesizeKeyExpectEvent("VK_UP", {}, tree, "!select", "key up unselectable cell");
    testtag_tree_TreeSelection_State(tree, testid + "key up unselectable cell",
                                     3, [3], 3, secondcolumn);
  }
}

function testtag_tree_TreeView(tree, testid, rowInfo)
{
  testid += " view ";

  var columns = tree.columns;
  var view = tree.view;

  is(view instanceof Components.interfaces.nsITreeView, true, testid + "view is a TreeView");
  is(view.rowCount, rowInfo.rows.length, testid + "rowCount");

  testtag_tree_TreeView_rows(tree, testid, rowInfo, 0);

  // note that this will only work for content trees currently
  view.setCellText(0, columns[1], "Changed Value");
  is(view.getCellText(0, columns[1]), "Changed Value", "setCellText");

  view.setCellValue(1, columns[0], "Another Changed Value");
  is(view.getCellValue(1, columns[0]), "Another Changed Value", "setCellText");
}

function testtag_tree_TreeView_rows(tree, testid, rowInfo, startRow)
{
  var r;
  var columns = tree.columns;
  var view = tree.view;
  var length = rowInfo.rows.length;

  // methods to test along with the functions which determine the expected value
  var checkRowMethods =
  {
    isContainer: function(row) { return row.container },
    isContainerOpen: function(row) { return false },
    isContainerEmpty: function(row) { return (row.children != null && row.children.rows.length == 0) },
    isSeparator: function(row) { return row.separator },
    getRowProperties: function(row) { return row.properties },
    getLevel: function(row) { return row.level },
    getParentIndex: function(row) { return row.parent },
    hasNextSibling: function(row) { return r < startRow + length - 1; }
  };

  var checkCellMethods =
  {
    getCellText: function(row, cell) { return cell.label },
    getCellValue: function(row, cell) { return cell.value },
    getCellProperties: function(row, cell) { return cell.properties },
    isEditable: function(row, cell) { return cell.editable },
    isSelectable: function(row, cell) { return cell.selectable },
    getImageSrc: function(row, cell) { return cell.image },
    getProgressMode: function(row, cell) { return cell.mode }
  };

  var failedMethods = { };
  var checkMethod, actual, expected;
  var containerInfo = null;
  var toggleOpenStateOK = true;

  for (r = startRow; r < length; r++) {
    var row = rowInfo.rows[r];
    for (var c = 0; c < row.cells.length; c++) {
      var cell = row.cells[c];

      for (checkMethod in checkCellMethods) {
        expected = checkCellMethods[checkMethod](row, cell);
        if (checkMethod == "getCellProperties") {
          var properties = Components.classes["@mozilla.org/supports-array;1"].
                             createInstance(Components.interfaces.nsISupportsArray);
          view.getCellProperties(r, columns[c], properties);
          actual = convertProperties(properties);
        }
        else {
          actual = view[checkMethod](r, columns[c]);
        }
        if (actual !== expected) {
          failedMethods[checkMethod] = true;
          is(actual, expected, testid + "row " + r + " column " + c + " " + checkMethod + " is incorrect");
        }
      }
    }

    // compare row properties
    for (checkMethod in checkRowMethods) {
      expected = checkRowMethods[checkMethod](row, r);
      if (checkMethod == "getRowProperties") {
        var properties = Components.classes["@mozilla.org/supports-array;1"].
                           createInstance(Components.interfaces.nsISupportsArray);
        view.getRowProperties(r, properties);
        actual = convertProperties(properties);
      }
      else if (checkMethod == "hasNextSibling") {
        actual = view[checkMethod](r, r);
      }
      else {
        actual = view[checkMethod](r);
      }
      if (actual !== expected) {
        failedMethods[checkMethod] = true;
        is(actual, expected, testid + "row " + r + " " + checkMethod + " is incorrect");
      }
    }
/*
    // open and recurse into containers
    if (row.container) {
      view.toggleOpenState(r);
      if (!view.isContainerOpen(r)) {
        toggleOpenStateOK = false;
        is(view.isContainerOpen(r), true, testid + "row " + r + " toggleOpenState open");
      }
      testtag_tree_TreeView_rows(tree, testid + "container " + r + " ", row.children, r + 1);
      view.toggleOpenState(r);
      if (view.isContainerOpen(r)) {
        toggleOpenStateOK = false;
        is(view.isContainerOpen(r), false, testid + "row " + r + " toggleOpenState close");
      }
    }
*/
  }

  for (var failedMethod in failedMethods) {
    if (failedMethod in checkRowMethods)
      delete checkRowMethods[failedMethod];
    if (failedMethod in checkCellMethods)
      delete checkCellMethods[failedMethod];
  }

  for (checkMethod in checkRowMethods)
    is(checkMethod + " ok", checkMethod + " ok", testid + checkMethod);
  for (checkMethod in checkCellMethods)
    is(checkMethod + " ok", checkMethod + " ok", testid + checkMethod);
  if (toggleOpenStateOK)
    is("toggleOpenState ok", "toggleOpenState ok", testid + "toggleOpenState");
}

function testtag_tree_TreeView_rows_sort(tree, testid, rowInfo)
{
  // check if cycleHeader sorts the columns
  var view = tree.view;
  var column = tree.columns[0];
  var columnElement = column.element;
  var sortkey = columnElement.getAttribute("sort");
  if (sortkey) {
    view.cycleHeader(column);
    is(tree.getAttribute("sort"), sortkey, "cycleHeader sort");
    is(tree.getAttribute("sortDirection"), "ascending", "cycleHeader sortDirection ascending");
    is(columnElement.getAttribute("sortDirection"), "ascending", "cycleHeader column sortDirection");
    is(columnElement.getAttribute("sortActive"), "true", "cycleHeader column sortActive");
    view.cycleHeader(column);
    is(tree.getAttribute("sortDirection"), "descending", "cycleHeader sortDirection descending");
    is(columnElement.getAttribute("sortDirection"), "descending", "cycleHeader column sortDirection descending");
    view.cycleHeader(column);
    is(tree.getAttribute("sortDirection"), "", "cycleHeader sortDirection natural");
    is(columnElement.getAttribute("sortDirection"), "", "cycleHeader column sortDirection natural");
    // XXXndeakin content view isSorted needs to be tested
  }
}

// checks if the current and selected rows are correct
// current is the index of the current row
// selected is an array of the indicies of the selected rows
// column is the selected column
// viewidx is the row that should be visible at the top of the tree
function testtag_tree_TreeSelection_State(tree, testid, current, selected, viewidx, column)
{
  var selection = tree.view.selection;

  if (!column)
    column = (tree.selType == "cell") ? tree.columns[0] : null;

  is(selection.count, selected.length, testid + " count");
  is(tree.currentIndex, current, testid + " currentIndex");
  is(selection.currentIndex, current, testid + " TreeSelection currentIndex");
  is(selection.currentColumn, column, testid + " currentColumn");
  if (viewidx !== null && viewidx !== undefined)
    is(tree.treeBoxObject.getFirstVisibleRow(), viewidx, testid + " first visible row");

  var actualSelected = [];
  var count = tree.view.rowCount;
  for (var s = 0; s < count; s++) {
    if (selection.isSelected(s))
      actualSelected.push(s);
  }

  is(compareArrays(selected, actualSelected), true, testid + " selection [" + selected + "]");

  actualSelected = [];
  var rangecount = selection.getRangeCount();
  for (var r = 0; r < rangecount; r++) {
    var start = {}, end = {};
    selection.getRangeAt(r, start, end);
    for (var rs = start.value; rs <= end.value; rs++)
      actualSelected.push(rs);
  }

  is(compareArrays(selected, actualSelected), true, testid + " range selection [" + selected + "]");
}

function mouseOnCell(tree, row, column, testname)
{
  var x = {}, y = {}, width = {}, height = {};
  tree.boxObject.getCoordsForCellItem(row, column, "cell", x, y, width, height);

  synthesizeMouseExpectEvent(tree.body, x.value + 2, y.value + 2, {}, tree, "select", testname);
}

function compareArrays(arr1, arr2)
{
  if (arr1.length != arr2.length)
    return false;

  for (var i = 0; i < arr1.length; i++) {
    if (arr1[i] != arr2[i])
      return false;
  }

  return true;
}

function convertProperties(arr)
{
  var results = [];
  var count = arr.Count();
  for (var i = 0; i < count; i++)
    results.push(arr.GetElementAt(i).QueryInterface(Components.interfaces.nsIAtom).toString());

  results.sort();
  return results.join(" ");
}

function convertDOMtoTreeRowInfo(treechildren, level, rowidx)
{
  var obj = { rows: [] };

  var parentidx = rowidx.value;

  treechildren = treechildren.childNodes;
  for (var r = 0; r < treechildren.length; r++) {
    rowidx.value++;

    var treeitem = treechildren[r];
    if (treeitem.hasChildNodes()) {
      var treerow = treeitem.firstChild;
      var cellInfo = [];
      for (var c = 0; c < treerow.childNodes.length; c++) {
        var cell = treerow.childNodes[c];
        cellInfo.push({ label: "" + cell.getAttribute("label"),
                        value: cell.getAttribute("value"),
                        properties: cell.getAttribute("properties"),
                        editable: cell.getAttribute("editable") != "false",
                        selectable: cell.getAttribute("selectable") != "false",
                        image: cell.getAttribute("src"),
                        mode: cell.hasAttribute("mode") ? parseInt(cell.getAttribute("mode")) : 3 });
      }

      var descendants = treeitem.lastChild;
      var children = (treerow == descendants) ? null :
                     convertDOMtoTreeRowInfo(descendants, level + 1, rowidx);
      obj.rows.push({ cells: cellInfo,
                      properties: treerow.getAttribute("properties"),
                      container: treeitem.getAttribute("container") == "true",
                      separator: treeitem.localName == "treeseparator",
                      children: children,
                      level: level,
                      parent: parentidx });
    }
  }

  return obj;
}
