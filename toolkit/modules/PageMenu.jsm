/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

this.EXPORTED_SYMBOLS = ["PageMenuParent", "PageMenuChild"];

var {interfaces: Ci} = Components;

this.PageMenu = function PageMenu() {
};

PageMenu.prototype = {
  PAGEMENU_ATTR: "pagemenu",
  GENERATEDITEMID_ATTR: "generateditemid",

  _popup: null,

  // Only one of builder or browser will end up getting set.
  _builder: null,
  _browser: null,

  // Given a target node, get the context menu for it or its ancestor.
  getContextMenu(aTarget) {
    let target = aTarget;
    while (target) {
      let contextMenu = target.contextMenu;
      if (contextMenu) {
        return contextMenu;
      }
      target = target.parentNode;
    }

    return null;
  },

  // Given a target node, generate a JSON object for any context menu
  // associated with it, or null if there is no context menu.
  maybeBuild(aTarget) {
    let pageMenu = this.getContextMenu(aTarget);
    if (!pageMenu) {
      return null;
    }

    pageMenu.sendShowEvent();
    // the show event is not cancelable, so no need to check a result here

    this._builder = pageMenu.createBuilder();
    if (!this._builder) {
      return null;
    }

    pageMenu.build(this._builder);

    // This serializes then parses again, however this could be avoided in
    // the single-process case with further improvement.
    let menuString = this._builder.toJSONString();
    if (!menuString) {
      return null;
    }

    return JSON.parse(menuString);
  },

  // Given a JSON menu object and popup, add the context menu to the popup.
  buildAndAttachMenuWithObject(aMenu, aBrowser, aPopup) {
    if (!aMenu) {
      return false;
    }

    let insertionPoint = this.getInsertionPoint(aPopup);
    if (!insertionPoint) {
      return false;
    }

    let fragment = aPopup.ownerDocument.createDocumentFragment();
    this.buildXULMenu(aMenu, fragment);

    let pos = insertionPoint.getAttribute(this.PAGEMENU_ATTR);
    if (pos == "start") {
      insertionPoint.insertBefore(fragment,
                                  insertionPoint.firstChild);
    } else if (pos.startsWith("#")) {
      insertionPoint.insertBefore(fragment, insertionPoint.querySelector(pos));
    } else {
      insertionPoint.appendChild(fragment);
    }

    this._browser = aBrowser;
    this._popup = aPopup;

    this._popup.addEventListener("command", this);
    this._popup.addEventListener("popuphidden", this);

    return true;
  },

  // Construct the XUL menu structure for a given JSON object.
  buildXULMenu(aNode, aElementForAppending) {
    let document = aElementForAppending.ownerDocument;

    let children = aNode.children;
    for (let child of children) {
      let menuitem;
      switch (child.type) {
        case "menuitem":
          if (!child.id) {
            continue; // Ignore children without ids
          }

          menuitem = document.createElement("menuitem");
          if (child.checkbox) {
            menuitem.setAttribute("type", "checkbox");
            if (child.checked) {
              menuitem.setAttribute("checked", "true");
            }
          }

          if (child.label) {
            menuitem.setAttribute("label", child.label);
          }
          if (child.icon) {
            menuitem.setAttribute("image", child.icon);
            menuitem.className = "menuitem-iconic";
          }
          if (child.disabled) {
            menuitem.setAttribute("disabled", true);
          }

          break;

        case "separator":
          menuitem = document.createElement("menuseparator");
          break;

        case "menu":
          menuitem = document.createElement("menu");
          if (child.label) {
            menuitem.setAttribute("label", child.label);
          }

          let menupopup = document.createElement("menupopup");
          menuitem.appendChild(menupopup);

          this.buildXULMenu(child, menupopup);
          break;
      }

      menuitem.setAttribute(this.GENERATEDITEMID_ATTR, child.id ? child.id : 0);
      aElementForAppending.appendChild(menuitem);
    }
  },

  // Called when the generated menuitem is executed.
  handleEvent(event) {
    let type = event.type;
    let target = event.target;
    if (type == "command" && target.hasAttribute(this.GENERATEDITEMID_ATTR)) {
      // If a builder is assigned, call click on it directly. Otherwise, this is
      // likely a menu with data from another process, so send a message to the
      // browser to execute the menuitem.
      if (this._builder) {
        this._builder.click(target.getAttribute(this.GENERATEDITEMID_ATTR));
      } else if (this._browser) {
        let win = target.ownerGlobal;
        let windowUtils = win.QueryInterface(Ci.nsIInterfaceRequestor)
                             .getInterface(Ci.nsIDOMWindowUtils);
        this._browser.messageManager.sendAsyncMessage("ContextMenu:DoCustomCommand", {
          generatedItemId: target.getAttribute(this.GENERATEDITEMID_ATTR),
          handlingUserInput: windowUtils.isHandlingUserInput
        });
      }
    } else if (type == "popuphidden" && this._popup == target) {
      this.removeGeneratedContent(this._popup);

      this._popup.removeEventListener("popuphidden", this);
      this._popup.removeEventListener("command", this);

      this._popup = null;
      this._builder = null;
      this._browser = null;
    }
  },

  // Get the first child of the given element with the given tag name.
  getImmediateChild(element, tag) {
    let child = element.firstChild;
    while (child) {
      if (child.localName == tag) {
        return child;
      }
      child = child.nextSibling;
    }
    return null;
  },

  // Return the location where the generated items should be inserted into the
  // given popup. They should be inserted as the next sibling of the returned
  // element.
  getInsertionPoint(aPopup) {
    if (aPopup.hasAttribute(this.PAGEMENU_ATTR))
      return aPopup;

    let element = aPopup.firstChild;
    while (element) {
      if (element.localName == "menu") {
        let popup = this.getImmediateChild(element, "menupopup");
        if (popup) {
          let result = this.getInsertionPoint(popup);
          if (result) {
            return result;
          }
        }
      }
      element = element.nextSibling;
    }

    return null;
  },

  // Remove the generated content from the given popup.
  removeGeneratedContent(aPopup) {
    let ungenerated = [];
    ungenerated.push(aPopup);

    let count;
    while (0 != (count = ungenerated.length)) {
      let last = count - 1;
      let element = ungenerated[last];
      ungenerated.splice(last, 1);

      let i = element.childNodes.length;
      while (i-- > 0) {
        let child = element.childNodes[i];
        if (!child.hasAttribute(this.GENERATEDITEMID_ATTR)) {
          ungenerated.push(child);
          continue;
        }
        element.removeChild(child);
      }
    }
  }
};

// This object is expected to be used from a parent process.
this.PageMenuParent = function PageMenuParent() {
};

PageMenuParent.prototype = {
  __proto__: PageMenu.prototype,

  /*
   * Given a target node and popup, add the context menu to the popup. This is
   * intended to be called when a single process is used. This is equivalent to
   * calling PageMenuChild.build and PageMenuParent.addToPopup in sequence.
   *
   * Returns true if custom menu items were present.
   */
  buildAndAddToPopup(aTarget, aPopup) {
    let menuObject = this.maybeBuild(aTarget);
    if (!menuObject) {
      return false;
    }

    return this.buildAndAttachMenuWithObject(menuObject, null, aPopup);
  },

  /*
   * Given a JSON menu object and popup, add the context menu to the popup. This
   * is intended to be called when the child page is in a different process.
   * aBrowser should be the browser containing the page the context menu is
   * displayed for, which may be null.
   *
   * Returns true if custom menu items were present.
   */
  addToPopup(aMenu, aBrowser, aPopup) {
    return this.buildAndAttachMenuWithObject(aMenu, aBrowser, aPopup);
  }
};

// This object is expected to be used from a child process.
this.PageMenuChild = function PageMenuChild() {
};

PageMenuChild.prototype = {
  __proto__: PageMenu.prototype,

  /*
   * Given a target node, return a JSON object for the custom menu commands. The
   * object will consist of a hierarchical structure of menus, menuitems or
   * separators. Supported properties of each are:
   *   Menu: children, label, type="menu"
   *   Menuitems: checkbox, checked, disabled, icon, label, type="menuitem"
   *   Separators: type="separator"
   *
   * In addition, the id of each item will be used to identify the item
   * when it is executed. The type will either be 'menu', 'menuitem' or
   * 'separator'. The toplevel node will be a menu with a children property. The
   * children property of a menu is an array of zero or more other items.
   *
   * If there is no menu associated with aTarget, null will be returned.
   */
  build(aTarget) {
    return this.maybeBuild(aTarget);
  },

  /*
   * Given the id of a menu, execute the command associated with that menu. It
   * is assumed that only one command will be executed so the builder is
   * cleared afterwards.
   */
  executeMenu(aId) {
    if (this._builder) {
      this._builder.click(aId);
      this._builder = null;
    }
  }
};
