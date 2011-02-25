function test() {
  document.popupNode = document;
  isnot(document.popupNode, null, "document.popupNode has been correctly set");
}
