/*
 * $_
 *
 * Returns the element with the specified |name| attribute.
 */
function $_(name) {
  var elements = document.getElementsByName(name);

  if (!elements || elements.length < 1) { return null; }
  if (elements.length > 2) {
    logWarning("found multiple elements with name="+name);
  }

  return elements[0];
}
