/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is MozMill Test code.
 *
 * The Initial Developer of the Original Code is Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Henrik Skupin <hskupin@mozilla.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

/**
 * @fileoverview
 * The TabbedBrowsingAPI adds support for accessing and interacting with tab elements
 *
 * @version 1.0.0
 */

var MODULE_NAME = 'TabbedBrowsingAPI';

const gTimeout = 5000;

const tabsBrowser = '/id("main-window")/id("browser")/id("appcontent")/id("content")';
const tabsStrip = tabsBrowser + '/anon({"anonid":"tabbox"})/anon({"anonid":"strip"})';
const tabsContainer = tabsStrip + '/anon({"anonid":"tabcontainer"})/anon({"class":"tabs-stack"})/{"class":"tabs-container"}';
const tabsArrowScrollbox = tabsContainer + '/anon({"anonid":"arrowscrollbox"})';


/**
 * Close all tabs and open about:blank
 *
 * @param {MozMillController} controller
 *        MozMillController of the window to operate on
 */
function closeAllTabs(controller)
{
  var browser = new tabBrowser(controller);
  browser.closeAllTabs();
}

/**
 * Constructor
 * 
 * @param {MozMillController} controller
 *        MozMill controller of the window to operate on
 */
function tabBrowser(controller)
{
  this._controller = controller;
  this._tabs = this.getElement({type: "tabs"});
}

/**
 * Tabbed Browser class
 */
tabBrowser.prototype = {
  /**
   * Returns the MozMill controller
   *
   * @returns Mozmill controller
   * @type {MozMillController}
   */
  get controller() {
    return this._controller;
  },

  /**
   * Get the amount of open tabs
   *
   * @returns Number of tabs
   * @type {number}
   */
  get length() {
    return this._tabs.getNode().itemCount;
  },

  /**
   * Get the currently selected tab index
   *
   * @returns Index of currently selected tab
   * @type {number}
   */
  get selectedIndex() {
    return this._tabs.getNode().selectedIndex;
  },

  /**
   * Select the tab with the given index
   *
   * @param {number} index
   *        Index of the tab which should be selected
   */
  set selectedIndex(index) {
    this._controller.click(this.getTab(index), 2, 2);
  },

  /**
   * Close all tabs of the window except the last one and open a blank page.
   */
  closeAllTabs : function tabBrowser_closeAllTabs()
  {
    while (this._controller.tabs.length > 1)
      this.closeTab({type: "menu"});

    this._controller.open("about:blank");
    this._controller.waitForPageLoad();
  },

  /**
   * Close an open tab
   *
   * @param {object} event
   *        The event specifies how to close a tab (menu, middle click,
   *        shortcut, or the tab close button). Only with middle click an
   *        inactive tab can be closed.
   */
  closeTab : function tabBrowser_closeTab(event) {
    switch (event.type) {
      case "closeButton":
        var button = this.getElement({type: "tabs_tabCloseButton",
                                     subtype: "tab", value: this.getTab()});
        controller.click(button);
        break;
      case "menu":
        var menuitem = new elementslib.Elem(this._controller.menus['file-menu'].menu_close);
        this._controller.click(menuitem);
        break;
      case "middleClick":
        var tab = this.getTab(event.index);
        this._controller.middleClick(tab);
        break;
      case "shortcut":
        this._controller.keypress(null, "w", {accelKey: true});
        break;
      default:
        throw new Error(arguments.callee.name + ": Unknown event - " + event.type);
    }
  },

  /**
   * Retrieve an UI element based on the given spec
   *
   * @param {object} spec
   *        Information of the UI element which should be retrieved
   *        type: General type information
   *        subtype: Specific element or property
   *        value: Value of the element or property
   * @returns Element which has been created  
   * @type {ElemBase}
   */
  getElement : function tabBrowser_getElement(spec) {
    var elem = null;

    switch(spec.type) {
      /**
       * subtype: subtype to match
       * value: value to match
       */
      case "tabs":
        elem = new elementslib.Lookup(this._controller.window.document,
                                      tabsStrip + '/anon({"anonid":"tabcontainer"})');
        break;
      case "tabs_allTabsButton":
        elem = new elementslib.Lookup(this._controller.window.document,
                                      tabsContainer + '/{"pack":"end"}/anon({"anonid":"alltabs-button"})');
        break;
      case "tabs_allTabsPopup":
        elem = new elementslib.Lookup(this._controller.window.document,
                                      tabsContainer + '/{"pack":"end"}/anon({"anonid":"alltabs-button"})' +
                                      '/anon({"anonid":"alltabs-popup"})');
        break;
      case "tabs_animateBox":
        elem = new elementslib.Lookup(this._controller.window.document, tabsContainer +
                                      '/{"pack":"end"}/anon({"anonid":"alltabs-box-animate"})');
        break;
      case "tabs_container":
        elem = new elementslib.Lookup(this._controller.window.document, tabsContainer);
        break;
      case "tabs_newTabButton":
        elem = new elementslib.Lookup(this._controller.window.document,
                                      tabsArrowScrollbox + '/anon({"class":"tabs-newtab-button"})');
        break;
      case "tabs_scrollButton":
        elem = new elementslib.Lookup(controller.window.document,
                                      tabsArrowScrollbox +
                                      '/anon({"anonid":"scrollbutton-' + spec.subtype + '"})');
        break;
      case "tabs_strip":
        elem = new elementslib.Lookup(this._controller.window.document, tabsStrip);
        break;
      case "tabs_tab":
        switch (spec.subtype) {
          case "index":
            elem = new elementslib.Elem(this._tabs.getNode().getItemAtIndex(spec.value));
            break;
        }
        break;
      case "tabs_tabCloseButton":
        elem = new elementslib.Elem(spec.value.getNode().boxObject.lastChild);
        break;
      case "tabs_tabFavicon":
        elem = new elementslib.Elem(spec.value.getNode().boxObject.firstChild);
        break;
      case "tabs_tabPanel":
        var panelId = spec.value.getNode().getAttribute("linkedpanel");
        elem = new elementslib.Lookup(this._controller.window.document, tabsBrowser +
                                      '/anon({"anonid":"tabbox"})/anon({"anonid":"panelcontainer"})' +
                                      '/{"id":"' + panelId + '"}');
        break;
      default:
        throw new Error(arguments.callee.name + ": Unknown element type - " + spec.type);
    }

    return elem;
  },

  /**
   * Get the tab at the specified index
   *
   * @param {number} index
   *        Index of the tab
   * @returns The requested tab
   * @type {ElemBase}
   */
  getTab : function tabBrowser_getTab(index) {
    if (index === undefined)
      index = this.selectedIndex;

    return this.getElement({type: "tabs_tab", subtype: "index", value: index});
  },

  /**
   * Creates the child element of the tab's notification bar
   *
   * @param {number} tabIndex
   *        (Optional) Index of the tab to check
   * @param {string} elemString
   *        (Optional) Lookup string of the notification bar's child element
   * @return The created child element
   * @type {ElemBase}
   */
  getTabPanelElement : function tabBrowser_getTabPanelElement(tabIndex, elemString)
  {
    var index = tabIndex ? tabIndex : this.selectedIndex;
    var elemStr = elemString ? elemString : "";

    // Get the tab panel and check if an element has to be fetched
    var panel = this.getElement({type: "tabs_tabPanel", subtype: "tab", value: this.getTab(index)});
    var elem = new elementslib.Lookup(controller.window.document, panel.expression + elemStr);

    return elem;
  },

  /**
   * Open a new tab
   *
   * @param {object} event
   *        The event specifies how to open a new tab (menu, shortcut,
   *        new tab button, or double click on the tabstrip)
   */
  openTab : function tabBrowser_openTab(event) {
    switch (event.type) {
      case "menu":
        var menuitem = new elementslib.Elem(this._controller.menus['file-menu'].menu_newNavigatorTab);
        this._controller.click(menuitem);
        break;
      case "shortcut":
        this._controller.keypress(null, "t", {accelKey: true});
        break;
      case "newTabButton":
        var newTabButton = this.getElement({type: "tabs_newTabButton"});
        this._controller.click(newTabButton);
        break;
      case "tabStrip":
        var tabStrip = this.getElement({type: "tabs_strip"});

        // XXX: Workaround until bug 537968 has been fixed
        this._controller.click(tabStrip, tabStrip.getNode().clientWidth - 100, 3);

        // Todo: Calculate the correct x position
        this._controller.doubleClick(tabStrip, tabStrip.getNode().clientWidth - 100, 3);
        break;
    }
  }
}
