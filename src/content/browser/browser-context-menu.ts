/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*---------------------------------------------------------------- Context Menu ----------------------------------------------------------------*/

const checkItems = [];
const contextMenuObserver = new MutationObserver(contextMenuObserverFunc);

// export function addContextBox(
//   id: string,
//   l10n: string,
//   insert: string,
//   runFunction: string,
//   checkID: string,
//   checkedFunction: () => void,
// ) {
//   const contextMenu = document.createXULElement("menuitem");
//   contextMenu.setAttribute("data-l10n-id", l10n);
//   contextMenu.id = id;
//   contextMenu.setAttribute("oncommand", runFunction);

//   const contentAreaContextMenu = document.getElementById(
//     "contentAreaContextMenu",
//   );
//   contentAreaContextMenu.insertBefore(
//     contextMenu,
//     document.getElementById(insert),
//   );

//   contextMenuObserver.observe(document.getElementById(checkID), {
//     attributes: true,
//   });
//   checkItems.push(checkedFunction);

//   contextMenuObserverFunc();
// }

function contextMenuObserverFunc() {
  for (const elem of checkItems) {
    elem();
  }
}

window.SessionStore.promiseInitialized.then(() => {
  const contentAreaContextMenu = document.getElementById(
    "contentAreaContextMenu",
  );

  contentAreaContextMenu.addEventListener("popupshowing", (event) => {
    const menuSeparators = document.querySelectorAll(
      "#contentAreaContextMenu > menuseparator",
    );

    const screenShot = document.getElementById("context-take-screenshot");
    if (!screenShot.hidden) {
      screenShot.nextSibling.hidden = false;
    }

    if (!document.getElementById("context-take-screenshot").hidden) {
      document.getElementById("context-sep-pdfjs-selectall").hidden = false;
    }

    window.setTimeout(() => {
      for (let i = 0; i < menuSeparators.length; i++) {
        if (
          menuSeparators[i].nextSibling.hidden &&
          menuSeparators[i].previousSibling.hidden &&
          menuSeparators[i].id !== "context-sep-navigation" &&
          menuSeparators[i].id !== "context-sep-pdfjs-selectall"
        ) {
          menuSeparators[i].hidden = true;
        }
      }
    }, 0);
  });
});
/********************* Share mode *********************************/

const beforeElem = document.getElementById("menu_openFirefoxView");
const addElem = window.MozXULElement.parseXULToFragment(`
    <menuitem data-l10n-id="sharemode-menuitem" type="checkbox" id="toggle_sharemode" checked="false"
          oncommand="addOrRemoveShareModeCSS();" accesskey="S">
    </menuitem>
`);
beforeElem.after(addElem);

export function addOrRemoveShareModeCSS() {
  const cssExist = document.getElementById("sharemode");

  if (!cssExist) {
    const css = document.createElement("style");
    css.id = "sharemode";
    css.textContent =
      "@import url(chrome://browser/skin/options/sharemode.css);";
    document.head.appendChild(css);
  } else {
    cssExist.remove();
  }
}
