function test() {
  document.popupNode = document.documentElement;
  isnot(document.popupNode, null, "document.popupNode has been correctly set");
}
