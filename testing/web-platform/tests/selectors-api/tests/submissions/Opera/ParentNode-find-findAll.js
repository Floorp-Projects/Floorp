/*
 * Check that the find and findAll methods exist on the given Node
 */
function interfaceCheckFind(type, obj) {
  test(function() {
    var q = typeof obj.find === "function";
    assert_true(q, type + " supports find.");
  }, type + " supports find")

  test(function() {
    var qa = typeof obj.findAll === "function";
    assert_true( qa, type + " supports findAll.");
  }, type + " supports findAll")
}

/*
 * Verify that the NodeList returned by findAll is static and and that a new list is created after
 * each call. A static list should not be affected by subsequent changes to the DOM.
 */
function verifyStaticList(type, root) {
  var pre, post, preLength;

  test(function() {
    pre = root.findAll("div");
    preLength = pre.length;

    var div = doc.createElement("div");
    (root.body || root).appendChild(div);

    assert_equals(pre.length, preLength, "The length of the NodeList should not change.")
  }, type + ": static NodeList")

  test(function() {
    post = root.findAll("div"),
    assert_equals(post.length, preLength + 1, "The length of the new NodeList should be 1 more than the previous list.")
  }, type + ": new NodeList")
}

/*
 * Verify handling of special values for the selector parameter, including stringification of
 * null and undefined, and the handling of the empty string.
 */
function runSpecialSelectorTests(type, root) {
  test(function() { // 1
    assert_equals(root.findAll(null).length, 1, "This should find one element with the tag name 'NULL'.");
  }, type + ".findAll null")

  test(function() { // 2
    assert_equals(root.findAll(undefined).length, 1, "This should find one elements with the tag name 'UNDEFINED'.");
  }, type + ".findAll undefined")

  test(function() { // 3
    assert_throws(TypeError(), function() {
      root.findAll();
    }, "This should throw a TypeError.")
  }, type + ".findAll no parameter")

  test(function() { // 4
    var elm = root.find(null)
    assert_not_equals(elm, null, "This should find an element.");
    assert_equals(elm.tagName.toUpperCase(), "NULL", "The tag name should be 'NULL'.")
  }, type + ".find null")

  test(function() { // 5
    var elm = root.find(undefined)
    assert_not_equals(elm, undefined, "This should find an element.");
    assert_equals(elm.tagName.toUpperCase(), "UNDEFINED", "The tag name should be 'UNDEFINED'.")
  }, type + ".find undefined")

  test(function() { // 6
    assert_throws(TypeError(), function() {
      root.find();
    }, "This should throw a TypeError.")
  }, type + ".find no parameter.")

  test(function() { // 7
    result = root.findAll("*");
    var i = 0;
    traverse(root, function(elem) {
      if (elem !== root) {
        assert_equals(elem, result[i++], "The result in index " + i + " should be in tree order.")
      }
    })
  }, type + ".findAll tree order");
}

/*
 * Execute queries with the specified valid selectors for both find() and findAll()
 * Only run these tests when results are expected. Don't run for syntax error tests.
 *
 * context.findAll(selector, refNodes)
 * context.findAll(selector)        // Only if refNodes is not specified
 * root.findAll(selector, context)  // Only if refNodes is not specified
 * root.findAll(selector, refNodes) // Only if context is not specified
 * root.findAll(selector)           // Only if neither context nor refNodes is specified
 *
 * Equivalent tests will be run for .find() as well.
 */
function runValidSelectorTest(type, root, selectors, docType) {
  var nodeType = getNodeType(root);

  for (var i = 0; i < selectors.length; i++) {
    var s = selectors[i];
    var n = s["name"];
    var q = s["selector"];
    var e = s["expect"];

    var ctx = s["ctx"];
    var ref = s["ref"];

    if (!s["exclude"] || (s["exclude"].indexOf(nodeType) === -1 && s["exclude"].indexOf(docType) === -1)) {
      //console.log("Running tests " + nodeType + ": " + s["testType"] + "&" + testType + "=" + (s["testType"] & testType) + ": " + JSON.stringify(s))
      var foundall, found, context, refNodes, refArray;

      if (s["testType"] & TEST_FIND) {


        /*
         * If ctx and ref are specified:
         * context.findAll(selector, refNodes)
         * context.find(selector, refNodes)
         */
        if (ctx && ref) {
          context = root.querySelector(ctx);
          refNodes = root.querySelectorAll(ref);
          refArray = Array.prototype.slice.call(refNodes, 0);

          test(function() {
            foundall = context.findAll(q, refNodes);
            verifyNodeList(foundall, expect);
          }, type + " [Context Element].findAll: " + n + " (with refNodes NodeList): " + q);

          test(function() {
            foundall = context.findAll(q, refArray);
            verifyNodeList(foundall, expect);
          }, type + " [Context Element].findAll: " + n + " (with refNodes Array): " + q);

          test(function() {
            found = context.find(q, refNodes);
            verifyElement(found, foundall, expect)
          }, type + " [Context Element].find: " + n + " (with refNodes NodeList): " + q);

          test(function() {
            found = context.find(q, refArray);
            verifyElement(found, foundall, expect)
          }, type + " [Context Element].find: " + n + " (with refNodes Array): " + q);
        }


        /*
         * If ctx is specified, ref is not:
         * context.findAll(selector)
         * context.find(selector)
         * root.findAll(selector, context)
         * root.find(selector, context)
         */
        if (ctx && !ref) {
          context = root.querySelector(ctx);

          test(function() {
            foundall = context.findAll(q);
            verifyNodeList(foundall, expect);
          }, type + " [Context Element].findAll: " + n + " (with no refNodes): " + q);

          test(function() {
            found = context.find(q);
            verifyElement(found, foundall, expect)
          }, type + " [Context Element].find: " + n + " (with no refNodes): " + q);

          test(function() {
            foundall = root.findAll(q, context);
            verifyNodeList(foundall, expect);
          }, type + " [Root Node].findAll: " + n + " (with refNode Element): " + q);

          test(function() {
            foundall = root.find(q, context);
            verifyElement(found, foundall, expect);
          }, type + " [Root Node].find: " + n + " (with refNode Element): " + q);
        }

        /*
         * If ref is specified, ctx is not:
         * root.findAll(selector, refNodes)
         * root.find(selector, refNodes)
         */
        if (!ctx && ref) {
          refNodes = root.querySelectorAll(ref);
          refArray = Array.prototype.slice.call(refNodes, 0);

          test(function() {
            foundall = root.findAll(q, refNodes);
            verifyNodeList(foundall, expect);
          }, type + " [Root Node].findAll: " + n + " (with refNodes NodeList): " + q);

          test(function() {
            foundall = root.findAll(q, refArray);
            verifyNodeList(foundall, expect);
          }, type + " [Root Node].findAll: " + n + " (with refNodes Array): " + q);

          test(function() {
            found = root.find(q, refNodes);
            verifyElement(found, foundall, expect);
          }, type + " [Root Node].find: " + n + " (with refNodes NodeList): " + q);

          test(function() {
            found = root.find(q, refArray);
            verifyElement(found, foundall, expect);
          }, type + " [Root Node].find: " + n + " (with refNodes Array): " + q);
        }

        /*
         * If neither ctx nor ref is specified:
         * root.findAll(selector)
         * root.find(selector)
         */
        if (!ctx && !ref) {
          test(function() {
            foundall = root.findAll(q);
            verifyNodeList(foundall, expect);
          }, type + ".findAll: " + n + " (with no refNodes): " + q);

          test(function() {
            found = root.find(q);
            verifyElement(found, foundall, expect);
          }, type + ".find: " + n + " (with no refNodes): " + q);
        }
      }
    } else {
      //console.log("Excluding for " + nodeType + ": " + s["testType"] + "&" + testType + "=" + (s["testType"] & testType) + ": " + JSON.stringify(s))
    }
  }
}

/*
 * Execute queries with the specified invalid selectors for both find() and findAll()
 * Only run these tests when errors are expected. Don't run for valid selector tests.
 */
function runInvalidSelectorTestFind(type, root, selectors) {
  for (var i = 0; i < selectors.length; i++) {
    var s = selectors[i];
    var n = s["name"];
    var q = s["selector"];

    test(function() {
      assert_throws("SyntaxError", function() {
        root.find(q)
      })
    }, type + ".find: " + n + ": " + q);

    test(function() {
      assert_throws("SyntaxError", function() {
        root.findAll(q)
      })
    }, type + ".findAll: " + n + ": " + q);
  }
}

function verifyNodeList(resultAll, expect) {
  assert_not_equals(resultAll, null, "The method should not return null.");
  assert_equals(resultAll.length, e.length, "The method should return the expected number of matches.");

  for (var i = 0; i < e.length; i++) {
    assert_not_equals(resultAll[i], null, "The item in index " + i + " should not be null.")
    assert_equals(resultAll[i].getAttribute("id"), e[i], "The item in index " + i + " should have the expected ID.");
    assert_false(resultAll[i].hasAttribute("data-clone"), "This should not be a cloned element.");
  }
}

function verifyElement(result, resultAll, expect) {
  if (expect.length > 0) {
    assert_not_equals(result, null, "The method should return a match.")
    assert_equals(found.getAttribute("id"), e[0], "The method should return the first match.");
    assert_equals(result, resultAll[0], "The result should match the first item from querySelectorAll.");
    assert_false(found.hasAttribute("data-clone"), "This should not be annotated as a cloned element.");
  } else {
    assert_equals(result, null, "The method should not match anything.");
  }
}
