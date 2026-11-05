function getScrollTargetContext(selected, focused) {
  return focused && focused.top === selected ? focused : selected;
}
