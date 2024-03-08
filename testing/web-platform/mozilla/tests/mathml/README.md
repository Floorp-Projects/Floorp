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

- `mpadded`: Tests for some
   [mpadded](https://www.w3.org/TR/MathML3/chapter3.html#presm.mpadded)
   features, which are not in the initial version of MathML Core.

- `menclose`: Tests for the
   [menclose](https://www.w3.org/TR/MathML3/chapter3.html#presm.menclose)
   element, which is not in the initial version of MathML Core.
   See [issue 216](https://github.com/w3c/mathml/issues/216).

- `mo-accent`: Tests for the
   [mo@accent attribute](https://www.w3.org/TR/MathML3/chapter3.html#presm.mo.dict.attrs),
   and the corresponding accent property from the dictionary,
   which are removed from MathML Core.
   See [bug 1790548](https://bugzilla.mozilla.org/1790548)
   and [bug 1636428](https://bugzilla.mozilla.org/1636428).

- `negative-lengths`: Tests for negative lengths, for which support or
   interpretation is unclear in current version of MathML Core.
   See [issue 132](https://github.com/w3c/mathml-core/issues/132).

- `operator-stretching`: Tests for operator stretching, using Gecko-specific
   methods that are not part of the current version of MathML Core.

- `rtl`: Tests for RTL MathML, for aspects not completely defined in
   MathML Core or for which we use things like scale transform for
   mirroring.
   See [issue 67](https://github.com/w3c/mathml-core/issues/67).

- `scripts`: Tests for MathML scripted elements, for Gecko features
   that are not defined in MathML Core or in contradiction with the spec.

- `tables`: Tests for
   [table features](https://www.w3.org/TR/MathML3/chapter3.html#presm.tabmat)
   that are in the initial version of MathML Core.
   See [issue 125](https://github.com/w3c/mathml-core/issues/125).

- `whitespace-trimming`: Tests for
   [whitespace trimming in token elements](https://www.w3.org/TR/MathML3/chapter2.html#fund.collapse)
   which is not described in the initial version of MathML Core.
   See [issue 149](https://github.com/w3c/mathml-core/issues/149).

- `zoom`: Tests to check MathML rendering at different zoom levels.
