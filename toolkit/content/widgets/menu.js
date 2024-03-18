/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// This is loaded into all XUL windows. Wrap in a block to prevent
// leaking to window scope.
{
  let imports = {};
  ChromeUtils.defineESModuleGetters(imports, {
    ShortcutUtils: "resource://gre/modules/ShortcutUtils.sys.mjs",
  });

  const MozMenuItemBaseMixin = Base => {
    class MozMenuItemBase extends MozElements.BaseTextMixin(Base) {
      // nsIDOMXULSelectControlItemElement
      set value(val) {
        this.setAttribute("value", val);
      }
      get value() {
        return this.getAttribute("value") || "";
      }

      // nsIDOMXULSelectControlItemElement
      get selected() {
        return this.getAttribute("selected") == "true";
      }

      // nsIDOMXULSelectControlItemElement
      get control() {
        var parent = this.parentNode;
        // Return the parent if it is a menu or menulist.
        if (parent && XULMenuElement.isInstance(parent.parentNode)) {
          return parent.parentNode;
        }
        return null;
      }

      // nsIDOMXULContainerItemElement
      get parentContainer() {
        for (var parent = this.parentNode; parent; parent = parent.parentNode) {
          if (XULMenuElement.isInstance(parent)) {
            return parent;
          }
        }
        return null;
      }
    }
    MozXULElement.implementCustomInterface(MozMenuItemBase, [
      Ci.nsIDOMXULSelectControlItemElement,
      Ci.nsIDOMXULContainerItemElement,
    ]);
    return MozMenuItemBase;
  };

  const MozMenuBaseMixin = Base => {
    class MozMenuBase extends MozMenuItemBaseMixin(Base) {
      set open(val) {
        this.openMenu(val);
      }

      get open() {
        return this.hasAttribute("open");
      }

      get itemCount() {
        var menupopup = this.menupopup;
        return menupopup ? menupopup.children.length : 0;
      }

      get menupopup() {
        const XUL_NS =
          "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul";

        for (
          var child = this.firstElementChild;
          child;
          child = child.nextElementSibling
        ) {
          if (child.namespaceURI == XUL_NS && child.localName == "menupopup") {
            return child;
          }
        }
        return null;
      }

      appendItem(aLabel, aValue) {
        var menupopup = this.menupopup;
        if (!menupopup) {
          menupopup = this.ownerDocument.createXULElement("menupopup");
          this.appendChild(menupopup);
        }

        var menuitem = this.ownerDocument.createXULElement("menuitem");
        menuitem.setAttribute("label", aLabel);
        menuitem.setAttribute("value", aValue);

        return menupopup.appendChild(menuitem);
      }

      getIndexOfItem(aItem) {
        var menupopup = this.menupopup;
        if (menupopup) {
          var items = menupopup.children;
          var length = items.length;
          for (var index = 0; index < length; ++index) {
            if (items[index] == aItem) {
              return index;
            }
          }
        }
        return -1;
      }

      getItemAtIndex(aIndex) {
        var menupopup = this.menupopup;
        if (!menupopup || aIndex < 0 || aIndex >= menupopup.children.length) {
          return null;
        }

        return menupopup.children[aIndex];
      }
    }
    MozXULElement.implementCustomInterface(MozMenuBase, [
      Ci.nsIDOMXULContainerElement,
    ]);
    return MozMenuBase;
  };

  // The <menucaption> element is used for rendering <html:optgroup> inside of <html:select>,
  // See SelectParentHelper.sys.mjs.
  class MozMenuCaption extends MozMenuBaseMixin(MozXULElement) {
    static get inheritedAttributes() {
      return {
        ".menu-iconic-left": "selected,disabled,checked",
        ".menu-iconic-icon": "src=image,validate,src",
        ".menu-iconic-text": "value=label,crop,highlightable",
        ".menu-iconic-highlightable-text": "text=label,crop,highlightable",
      };
    }

    connectedCallback() {
      this.textContent = "";
      this.appendChild(
        MozXULElement.parseXULToFragment(`
      <hbox class="menu-iconic-left" align="center" pack="center" aria-hidden="true">
        <image class="menu-iconic-icon" aria-hidden="true"></image>
      </hbox>
      <label class="menu-iconic-text" flex="1" crop="end" aria-hidden="true"></label>
      <label class="menu-iconic-highlightable-text" crop="end" aria-hidden="true"></label>
    `)
      );
      this.initializeAttributeInheritance();
    }
  }

  customElements.define("menucaption", MozMenuCaption);

  // In general, wait to render menus and menuitems inside menupopups
  // until they are going to be visible:
  window.addEventListener(
    "popupshowing",
    e => {
      if (e.originalTarget.ownerDocument != document) {
        return;
      }
      e.originalTarget.setAttribute("hasbeenopened", "true");
      for (let el of e.originalTarget.querySelectorAll("menuitem, menu")) {
        el.render();
      }
    },
    { capture: true }
  );

  class MozMenuItem extends MozMenuItemBaseMixin(MozXULElement) {
    static get observedAttributes() {
      return super.observedAttributes.concat("acceltext", "key");
    }

    attributeChangedCallback(name, oldValue, newValue) {
      if (name == "acceltext") {
        if (this._ignoreAccelTextChange) {
          this._ignoreAccelTextChange = false;
        } else {
          this._accelTextIsDerived = false;
          this._computeAccelTextFromKeyIfNeeded();
        }
      }
      if (name == "key") {
        this._computeAccelTextFromKeyIfNeeded();
      }
      super.attributeChangedCallback(name, oldValue, newValue);
    }

    static get inheritedAttributes() {
      return {
        ".menu-iconic-text": "value=label,crop,accesskey,highlightable",
        ".menu-text": "value=label,crop,accesskey,highlightable",
        ".menu-iconic-highlightable-text":
          "text=label,crop,accesskey,highlightable",
        ".menu-iconic-left": "selected,_moz-menuactive,disabled,checked",
        ".menu-iconic-icon":
          "src=image,validate,triggeringprincipal=iconloadingprincipal",
        ".menu-iconic-accel": "value=acceltext",
        ".menu-accel": "value=acceltext",
      };
    }

    static get iconicNoAccelFragment() {
      // Add aria-hidden="true" on all DOM, since XULMenuAccessible handles accessibility here.
      let frag = document.importNode(
        MozXULElement.parseXULToFragment(`
      <hbox class="menu-iconic-left" align="center" pack="center" aria-hidden="true">
        <image class="menu-iconic-icon"/>
      </hbox>
      <label class="menu-iconic-text" flex="1" crop="end" aria-hidden="true"/>
      <label class="menu-iconic-highlightable-text" crop="end" aria-hidden="true"/>
    `),
        true
      );
      Object.defineProperty(this, "iconicNoAccelFragment", { value: frag });
      return frag;
    }

    static get iconicFragment() {
      let frag = document.importNode(
        MozXULElement.parseXULToFragment(`
      <hbox class="menu-iconic-left" align="center" pack="center" aria-hidden="true">
        <image class="menu-iconic-icon"/>
      </hbox>
      <label class="menu-iconic-text" flex="1" crop="end" aria-hidden="true"/>
      <label class="menu-iconic-highlightable-text" crop="end" aria-hidden="true"/>
      <hbox class="menu-accel-container" aria-hidden="true">
        <label class="menu-iconic-accel"/>
      </hbox>
    `),
        true
      );
      Object.defineProperty(this, "iconicFragment", { value: frag });
      return frag;
    }

    static get plainFragment() {
      let frag = document.importNode(
        MozXULElement.parseXULToFragment(`
      <label class="menu-text" crop="end" aria-hidden="true"/>
      <hbox class="menu-accel-container" aria-hidden="true">
        <label class="menu-accel"/>
      </hbox>
    `),
        true
      );
      Object.defineProperty(this, "plainFragment", { value: frag });
      return frag;
    }

    get isIconic() {
      let type = this.getAttribute("type");
      return (
        type == "checkbox" ||
        type == "radio" ||
        this.classList.contains("menuitem-iconic")
      );
    }

    get isMenulistChild() {
      return this.matches("menulist > menupopup > menuitem");
    }

    get isInHiddenMenupopup() {
      return this.matches("menupopup:not([hasbeenopened]) menuitem");
    }

    _computeAccelTextFromKeyIfNeeded() {
      if (!this._accelTextIsDerived && this.getAttribute("acceltext")) {
        return;
      }
      let accelText = (() => {
        if (!document.contains(this)) {
          return null;
        }
        let keyId = this.getAttribute("key");
        if (!keyId) {
          return null;
        }
        let key = document.getElementById(keyId);
        if (!key) {
          let msg =
            `Key ${keyId} of menuitem ${this.getAttribute("label")} ` +
            `could not be found`;
          if (keyId.startsWith("ext-key-id-")) {
            console.info(msg);
          } else {
            console.error(msg);
          }
          return null;
        }
        return imports.ShortcutUtils.prettifyShortcut(key);
      })();

      this._accelTextIsDerived = true;
      // We need to ignore the next attribute change callback for acceltext, in
      // order to not reenter here.
      this._ignoreAccelTextChange = true;
      if (accelText) {
        this.setAttribute("acceltext", accelText);
      } else {
        this.removeAttribute("acceltext");
      }
    }

    render() {
      if (this.renderedOnce) {
        return;
      }
      this.renderedOnce = true;
      this.textContent = "";
      if (this.isMenulistChild) {
        this.append(this.constructor.iconicNoAccelFragment.cloneNode(true));
      } else if (this.isIconic) {
        this.append(this.constructor.iconicFragment.cloneNode(true));
      } else {
        this.append(this.constructor.plainFragment.cloneNode(true));
      }

      this._computeAccelTextFromKeyIfNeeded();
      this.initializeAttributeInheritance();
    }

    connectedCallback() {
      if (this.renderedOnce) {
        this._computeAccelTextFromKeyIfNeeded();
      }
      // Eagerly render if we are being inserted into a menulist (since we likely need to
      // size it), or into an already-opened menupopup (since we are already visible).
      // Checking isConnectedAndReady is an optimization that will let us quickly skip
      // non-menulists that are being connected during parse.
      if (
        this.isMenulistChild ||
        (this.isConnectedAndReady && !this.isInHiddenMenupopup)
      ) {
        this.render();
      }
    }
  }

  customElements.define("menuitem", MozMenuItem);

  const isHiddenWindow =
    document.documentURI == "chrome://browser/content/hiddenWindowMac.xhtml";

  class MozMenu extends MozMenuBaseMixin(
    MozElements.MozElementMixin(XULMenuElement)
  ) {
    static get inheritedAttributes() {
      return {
        ".menubar-text": "value=label,accesskey,crop",
        ".menu-iconic-text": "value=label,accesskey,crop,highlightable",
        ".menu-text": "value=label,accesskey,crop",
        ".menu-iconic-highlightable-text":
          "text=label,crop,accesskey,highlightable",
        ".menubar-left": "src=image",
        ".menu-iconic-icon":
          "src=image,triggeringprincipal=iconloadingprincipal,validate",
        ".menu-iconic-accel": "value=acceltext",
        ".menu-right": "_moz-menuactive,disabled",
        ".menu-accel": "value=acceltext",
      };
    }

    get needsEagerRender() {
      return (
        this.isMenubarChild || this.isMenulistChild || !this.isInHiddenMenupopup
      );
    }

    get isMenubarChild() {
      return this.matches("menubar > menu");
    }

    get isMenulistChild() {
      return this.matches("menulist > menupopup > menu");
    }

    get isInHiddenMenupopup() {
      return this.matches("menupopup:not([hasbeenopened]) menu");
    }

    get isIconic() {
      return this.classList.contains("menu-iconic");
    }

    get fragment() {
      let { isMenubarChild, isIconic } = this;
      let fragment = null;
      // Add aria-hidden="true" on all DOM, since XULMenuAccessible handles accessibility here.
      if (isMenubarChild && isIconic) {
        if (!MozMenu.menubarIconicFrag) {
          MozMenu.menubarIconicFrag = MozXULElement.parseXULToFragment(`
          <image class="menubar-left" aria-hidden="true"/>
          <label class="menubar-text" crop="end" aria-hidden="true"/>
        `);
        }
        fragment = document.importNode(MozMenu.menubarIconicFrag, true);
      }
      if (isMenubarChild && !isIconic) {
        if (!MozMenu.menubarFrag) {
          MozMenu.menubarFrag = MozXULElement.parseXULToFragment(`
          <label class="menubar-text" crop="end" aria-hidden="true"/>
        `);
        }
        fragment = document.importNode(MozMenu.menubarFrag, true);
      }
      if (!isMenubarChild && isIconic) {
        if (!MozMenu.normalIconicFrag) {
          MozMenu.normalIconicFrag = MozXULElement.parseXULToFragment(`
          <hbox class="menu-iconic-left" align="center" pack="center" aria-hidden="true">
            <image class="menu-iconic-icon"/>
          </hbox>
          <label class="menu-iconic-text" flex="1" crop="end" aria-hidden="true"/>
          <label class="menu-iconic-highlightable-text" crop="end" aria-hidden="true"/>
          <hbox class="menu-accel-container" anonid="accel" aria-hidden="true">
            <label class="menu-iconic-accel"/>
          </hbox>
          <hbox align="center" class="menu-right" aria-hidden="true">
            <image/>
          </hbox>
       `);
        }

        fragment = document.importNode(MozMenu.normalIconicFrag, true);
      }
      if (!isMenubarChild && !isIconic) {
        if (!MozMenu.normalFrag) {
          MozMenu.normalFrag = MozXULElement.parseXULToFragment(`
          <label class="menu-text" crop="end" aria-hidden="true"/>
          <hbox class="menu-accel-container" anonid="accel" aria-hidden="true">
            <label class="menu-accel"/>
          </hbox>
          <hbox align="center" class="menu-right" aria-hidden="true">
            <image/>
          </hbox>
       `);
        }

        fragment = document.importNode(MozMenu.normalFrag, true);
      }
      return fragment;
    }

    render() {
      // There are 2 main types of menus:
      //  (1) direct descendant of a menubar
      //  (2) all other menus
      // There is also an "iconic" variation of (1) and (2) based on the class.
      // To make this as simple as possible, we don't support menus being changed from one
      // of these types to another after the initial DOM connection. It'd be possible to make
      // this work by keeping track of the markup we prepend and then removing / re-prepending
      // during a change, but it's not a feature we use anywhere currently.
      if (this.renderedOnce) {
        return;
      }
      this.renderedOnce = true;

      // There will be a <menupopup /> already. Don't clear it out, just put our markup before it.
      this.prepend(this.fragment);
      this.initializeAttributeInheritance();
    }

    connectedCallback() {
      // On OSX we will have a bunch of menus in the hidden window. They get converted
      // into native menus based on the host attributes, so the inner DOM doesn't need
      // to be created.
      if (isHiddenWindow) {
        return;
      }

      if (this.delayConnectedCallback()) {
        return;
      }

      // Wait until we are going to be visible or required for sizing a popup.
      if (!this.needsEagerRender) {
        return;
      }

      this.render();
    }
  }

  customElements.define("menu", MozMenu);
}
