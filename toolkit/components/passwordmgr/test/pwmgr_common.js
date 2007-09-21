/*
 * $_
 *
 * Returns the element with the specified |name| attribute.
 */
function $_(formNum, name) {
  var form = document.getElementById("form" + formNum);
  if (!form) {
    logWarning("$_ couldn't find requested form " + formNum);
    return null;
  }

  var element = form.elements.namedItem(name);
  if (!element) {
    logWarning("$_ couldn't find requested element " + name);
    return null;
  }

  // Note that namedItem is a bit stupid, and will prefer an
  // |id| attribute over a |name| attribute when looking for
  // the element. Login Mananger happens to use .namedItem
  // anyway, but let's rigorously check it here anyway so
  // that we don't end up with tests that mistakenly pass.

  if (element.getAttribute("name") != name) {
    logWarning("$_ got confused.");
    return null;
  }

  return element;
}


/*
 * checkForm
 *
 * Check a form for expected values. If an argument is null, a field's
 * expected value will be the default value.
 *
 * <form id="form#">
 * checkForm(#, "foo");
 */
function checkForm(formNum, val1, val2, val3) {
    var e, form = document.getElementById("form" + formNum);
    ok(form, "Locating form " + formNum);

    var numToCheck = arguments.length - 1;
    
    if (!numToCheck--)
        return;
    e = form.elements[0];
    if (val1 == null)
        is(e.value, e.defaultValue, "Test default value of field " + e.name +
            " in form " + formNum);
    else
        is(e.value, val1, "Test value of field " + e.name +
            " in form " + formNum);


    if (!numToCheck--)
        return;
    e = form.elements[1];
    if (val2 == null)
        is(e.value, e.defaultValue, "Test default value of field " + e.name +
            " in form " + formNum);
    else
        is(e.value, val2, "Test value of field " + e.name +
            " in form " + formNum);


    if (!numToCheck--)
        return;
    e = form.elements[2];
    if (val3 == null)
        is(e.value, e.defaultValue, "Test default value of field " + e.name +
            " in form " + formNum);
    else
        is(e.value, val3, "Test value of field " + e.name +
            " in form " + formNum);

}


/*
 * checkUnmodifiedForm
 *
 * Check a form for unmodified values from when page was loaded.
 *
 * <form id="form#">
 * checkUnmodifiedForm(#);
 */
function checkUnmodifiedForm(formNum) {
    var form = document.getElementById("form" + formNum);
    ok(form, "Locating form " + formNum);

    for (var i = 0; i < form.elements.length; i++) {
        var ele = form.elements[i];

        // No point in checking form submit/reset buttons.
        if (ele.type == "submit" || ele.type == "reset")
            continue;

        is(ele.value, ele.defaultValue, "Test to default value of field " +
            ele.name + " in form " + formNum);
    }
}
