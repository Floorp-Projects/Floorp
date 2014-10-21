A reftest is a test that compares the visual output of one file (the
test case) with the output of one or more other files (the
references). The test and the reference must be carefully written so
that when the test passes they have identical rendering, but different
rendering when the test fails.

## How to Run Reftests

Reftests can be run manually simply by opening the test and the
reference file in multiple windows or tabs and either placing them
side-by side or flipping between the two. In automation the comparison
is done in an automated fashion, which can lead to differences too
small for the human eye to notice causing tests to fail.

## Components of a Reftest

A reftest has three parts:

### 1. The Reftest Test File ###

The test file uses the technology to be tested. This file should
follow the [format][format] and [style][style] guidelines.

It is preferable that a reftest is [self-describing][selfdesc],
since it allows for both machine comparison and manual
verification. Having the expected rendering described on the page
lets the tester check that the test and the reference are not
_both_ being rendered incorrectly and triggering a false pass.
Designing it for an obvious fail makes it easier to find what went
wrong when the reftest does fail.

### 2. The Reftest Reference File ###

This is a different, usually simpler, file that results in the same
rendering as the test. The reference file must not use the same
features that are being tested, but uses a different method to
produce the same rendering as a test file. Sometimes more than one
reference file is required. Multiple tests can (and often do) share
the same reference file. If it is possible to reuse an existing
reference for a test this should always be preferred as it enables
substantial speedups when running a large number of tests.

In some cases when creating the reference file, it is necessary to
use features that, although different from the tested features,
may themselves fail in such a manner as to cause the reference to
render identically to a failed test. When this is the case, in
order to reduce the possibility of false positive testing
outcomes, multiple reference files should be used, each using a
different technique to render the reference. One possibility is to
create one or more references that must **not** match the test
file, i.e.: a file that renders in the same manner as a failed
test.

### 3. The Reftest Comparison ###

In a reftest the test is required to either match or mismatch the
rendering of the reference file. In order to specify this
relationship there are two possible options:

* Give the test file and the reference file the same name but with the
  reference file having the suffix `-ref` before the file extension
  e.g. `test.html` and `test-ref.html`. This only works where the test
  and the ref must match.

* Use a `link` element with `rel="match"` or `rel="mismatch"` and
  `href` attribute pointing to the reference file e.g. `<link
  rel=match href=references/green-box.html>`.

In general the second form is more expressive and so preferred. In
particular it is more conducive to sharing the same reference amongst
multiple files.

## Controlling When Comparison Occurs

By default reftest screenshots are taken in response to the `load`
event firing. In some cases it is necessary to delay the screenshot
later than this, for example becase some DOM manipulation is
required to set up the desired test conditions. To enable this, the
test may have a `class="reftest-wait"` attribute specified on the root
element. This will cause the screenshot to be delayed until the load
event has fired and the reftest-wait class has been removed from the
root element (technical note: the implementation in wptrunner uses
mutation observers so the screenshot will be triggered in the
microtask checkpoint after the class is removed. Because the harness
isn't synchronized with the browser event loop it is dangerous to rely
on precise timing here).

## Matching Multiple References

Sometimes it is desirable for a file to match multiple references or,
in rare cases, to allow it to match more than one possible
reference. Note: *this is not currently supported by test runners and
so best avoided if possible until that support improves*.

Multiple references linked from a single file are interpreted as
multiple possible renderings for that file. `<link rel=[mis]match>`
elements in a reference create further conditions that must be met in
order for the test to pass. For example, consider a situation where
a.html has `<link rel=match href=b.html>` and `<link rel=match
href=c.html>`, b.html has `<link rel=match href=b1.html>` and c.html
has `<link rel=mismatch href=c1.html>`. In this case, to pass we must
either have a.html, b.html and b1.html all rendering identically, or
a.html and c.html rendering identically, but c.html rendering
differently from c1.html.

## Fuzzy Matching

In some situations a test may have subtle differences in rendering
compared to the reference due to e.g. antialiasing. This may cause the
test to pass on some platforms but fail on others. In this case some
affordance for subtle discrepancies is desirable. However no mechanism
to allow this has yet been standardized.

## Limitations

In some cases, a test cannot be a reftest. For example, there is no
way to create a reference for underlining, since the position and
thickness of the underline depends on the UA, the font, and/or the
platform. However, once it's established that underlining an inline
element works, it's possible to construct a reftest for underlining
a block element, by constructing a reference using underlines on a
```<span>``` that wraps all the content inside the block.

## Example Reftests

These examples are all [self-describing][selfdesc] tests as they
each have a simple statement on the page describing how it should
render to pass the tests.

### HTML example

### Test File

This test verifies that a right-to-left rendering of **SAW** within a
```<bdo>``` element displays as **WAS**.

([view page rendering][html-reftest-example])

```html
<!DOCTYPE html>
<meta charset="utf-8">
<title>BDO element dir=rtl</title>
<link rel="help" href="http://www.whatwg.org/specs/web-apps/current-work/#the-bdo-element">
<meta name="assert" content="BDO element's DIR content attribute renders corrently given value of 'rtl'.">
<link rel="match" href="test-bdo-001.html">
<p>Pass if you see WAS displayed below.</p>
<bdo dir="rtl">SAW</bdo>
```

### Reference File

The reference file must look exactly like the test file,
except that the code behind it is different.

* All metadata is removed.
* The ```title``` need not match.
* The markup that created the actual test data is
  different: here, the same effect is created with
  very mundane, dependable technology.

([view page rendering][html-reffile-example])

```html
<!DOCTYPE html>
<meta charset="utf-8">
<title>HTML Reference File</title>
<p>Pass if you see WAS displayed below.</p>
<p>WAS</p>
```

[testharness]: ./testharness-documentation.html
[format]: ./test-format-guidelines.html
[style]: ./test-style-guidelines.html
[selfdesc]: ./test-style-guidelines.html#self-describing-tests
[reference-links]: ./test-templates.html#reference-links
[html-reftest-example]: ./html-reftest-example.html
[html-reffile-example]: ./html-reffile-example.html
[css-reftest-example]: http://test.csswg.org/source/css21/borders/border-bottom-applies-to-009.xht
[css-reffile-example]: http://test.csswg.org/source/css21/borders/border-bottom-applies-to-001-ref.xht
[svg-reftest-example]: http://test.csswg.org/source/css-transforms-1/translate/svg-translate-001.html
[svg-reffile-example]: http://test.csswg.org/source/css-transforms-1/translate/reference/svg-translate-ref.html
[indicating-failure]: ./test-style-guidelines.html#failure
