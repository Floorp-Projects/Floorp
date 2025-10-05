export function getTabsToolbar(): XULElement | null {
  return document?.getElementById("TabsToolbar") as XULElement | null;
}

export function resolveTabsContainer(): XULElement | null {
  let arrowScrollbox = document?.querySelector(
    "#tabbrowser-tabs > arrowscrollbox",
  );
  const newScrollbox = document?.getElementById("tabbrowser-arrowscrollbox");
  if (newScrollbox) {
    arrowScrollbox = newScrollbox;
  }

  return arrowScrollbox as XULElement | null;
}

export function findChildIndex(container: Element, child: Element): number {
  return Array.prototype.indexOf.call(container.childNodes, child);
}
