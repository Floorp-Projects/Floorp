import type {} from "../../solid-xul/jsx-runtime";
import { insertNode, render } from "../../solid-xul/solid-xul";

export function webpanel(webpanelId: string, src: string) {
  return (
    <xul:browser
      id={webpanelId}
      contextmenu="contentAreaContextMenu"
      message="true"
      messagemanagergroup="browsers"
      tooltip="aHTMLTooltip"
      type="content"
      maychangeremoteness="true"
      initiallyactive="false"
      remote="true"
      autocompletepopup="PopupAutoComplete"
      src={`chrome://browser/content/browser.xhtml?${src}`}
      flex="1"
      disablefullscreen="true"
      disablehistory="true"
      style="width:100px"
      manualactiveness="true"
      //nodefaultsrc="true"
    />
  );
}

export function webpanel2base() {
  return (
    <xul:browser
      type="content"
      style="flex: 1 1 auto; width: 897px; min-width:74px"
      src="chrome://noraneko/content/webpanel/webpanel.xhtml"
    />
  );
}

export function webpanel2browser(
  id: string,
  viewType: string,
  onDidChangeBrowserRemoteness: () => void,
) {
  return (
    // <xul:browser
    //   id={id}
    //   type="content"
    //   flex="1"
    //   disableglobalhistory="true"
    //   messagemanagergroup="noraneko-sidebar"
    //   // webextension-view-type={viewType}
    //   contextmenu="contentAreaContextMenu"
    //   tooltip="aHTMLTooltip"
    //   autocompletepopup="PopupAutoComplete"
    //   remote="true"
    //   maychangeremoteness="true"
    //   src="https://google.com"
    //   // onDidChangeBrowserRemoteness={onDidChangeBrowserRemoteness}
    // />
    <iframe src="https://google.com" title="google" />
    // <xul:browser
    //   id={id}
    //   contextmenu="contentAreaContextMenu"
    //   message="true"
    //   messagemanagergroup="browsers"
    //   tooltip="aHTMLTooltip"
    //   type="content"
    //   maychangeremoteness="true"
    //   initiallyactive="false"
    //   remote="true"
    //   autocompletepopup="PopupAutoComplete"
    //   src="https://www.google.com"
    //   flex="1"
    //   disablefullscreen="true"
    //   disablehistory="true"
    //   style="width:100px"
    //   manualactiveness="true"
    //   //nodefaultsrc="true"
    // />
  );
}

// function addContextBoxTmp(id: string, l10n: string, insert: string, runFunction: string, checkID: string, checkedFunction: () => void) {
//   const contextMenu = document.createXULElement("menuitem");
//   contextMenu.setAttribute("data-l10n-id", l10n);
//   contextMenu.id = id;
//   contextMenu.setAttribute("oncommand", runFunction);

//   const contentAreaContextMenu = document.getElementById("contentAreaContextMenu");
//   contentAreaContextMenu.insertBefore(contextMenu, document.getElementById(insert));

//   // contextMenuObserver.observe(document.getElementById(checkID), {
//   //   attributes: true,
//   // });
//   // checkItems.push(checkedFunction);

//   // contextMenuObserverFunc();
// }

export function addContextBox(
  id: string,
  l10nId: string,
  onclick: (event: MouseEvent) => void,
  anchorId: string,
  hideFunc: () => boolean,
) {
  const contentAreaContextMenu = document.getElementById(
    "contentAreaContextMenu",
  );

  const menuItem = (
    <xul:menuitem id={id} data-l10n-id={l10nId} onClick={onclick} />
  );
  const anchor = document.getElementById(anchorId);

  insertNode(contentAreaContextMenu, menuItem, anchor);

  document
    .getElementById("contentAreaContextMenu")
    .addEventListener("popupshowing", (_ev) => {
      document.getElementById(id).hidden = hideFunc();
    });

  document
    .getElementById("contentAreaContextMenu")
    .addEventListener("popuphiding", (_ev) => {
      document.getElementById(id).hidden = hideFunc();
    });
}
