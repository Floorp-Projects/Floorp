# Internal MathML tests

The web-platform-tests Project provides [MathML tests](https://github.com/web-platform-tests/wpt/tree/master/mathml/)
for the [MathML Core](https://w3c.github.io/mathml-core/) specification. This
directory contains tests for [MathML3](https://www.w3.org/TR/MathML3/) features
implemented in Gecko or for Gecko-specific behaviors that are not described in
any specification:

- `disabled/`: Tests for MathML handling when support is disabled. This is
  mostly used for Tor browser's "high security" mode, see
  [bug 1173199](https://bugzilla.mozilla.org/1173199).

- `fonts`: font-related tests, such as OpenType features not handled yet in
  MathML Core or other Gecko-specific behavior.

- `mathml-console-messages.html`: Tests for Gecko-specific console warning and
  error messages triggered by MathML markup.

- `mathspaces_names`: Tests for
  [MathML3 namedspaces](https://www.w3.org/TR/MathML3/chapter2.html#type.namedspace)
  which are removed from MathML Core. See
  [bug 1793549](https://bugzilla.mozilla.org/1173199).

- `mathvariant`: Tests for the
   [mathvariant attribute](https://www.w3.org/TR/MathML3/chapter3.html#presm.commatt),
   which is reduced to the case `<mi mathvariant="normal">` in MathML
   Core. See [bug 1821980](https://bugzilla.mozilla.org/1821980).

- `menclose`: Tests for the
   [menclose](https://www.w3.org/TR/MathML3/chapter3.html#presm.menclose)
   element, which is not in the initial version of MathML Core.
   See [issue 216](https://github.com/w3c/mathml/issues/216).

- `tables`: Tests for
   [table features](https://www.w3.org/TR/MathML3/chapter3.html#presm.tabmat)
   that are in the initial version of MathML Core.
   See [issue 125](https://github.com/w3c/mathml-core/issues/125).

- `whitespace-trimming`: Tests for
   [whitespace trimming in token elements](https://www.w3.org/TR/MathML3/chapter2.html#fund.collapse)
   which is not described in the initial version of MathML Core.
   See [issue 149](https://github.com/w3c/mathml-core/issues/149).
