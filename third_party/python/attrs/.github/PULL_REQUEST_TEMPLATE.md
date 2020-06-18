# Pull Request Check List

This is just a reminder about the most common mistakes.  Please make sure that you tick all *appropriate* boxes.  But please read our [contribution guide](https://www.attrs.org/en/latest/contributing.html) at least once, it will save you unnecessary review cycles!

If an item doesn't apply to your pull request, **check it anyway** to make it apparent that there's nothing to do.

- [ ] Added **tests** for changed code.
- [ ] New features have been added to our [Hypothesis testing strategy](https://github.com/python-attrs/attrs/blob/master/tests/strategies.py).
- [ ] Changes or additions to public APIs are reflected in our type stubs (files ending in ``.pyi``).
    - [ ] ...and used in the stub test file `tests/typing_example.py`.
- [ ] Updated **documentation** for changed code.
    - [ ] New functions/classes have to be added to `docs/api.rst` by hand.
    - [ ] Changes to the signature of `@attr.s()` have to be added by hand too.
    - [ ] Changed/added classes/methods/functions have appropriate `versionadded`, `versionchanged`, or `deprecated` [directives](http://www.sphinx-doc.org/en/stable/markup/para.html#directive-versionadded).
- [ ] Documentation in `.rst` files is written using [semantic newlines](https://rhodesmill.org/brandon/2012/one-sentence-per-line/).
- [ ] Changes (and possible deprecations) have news fragments in [`changelog.d`](https://github.com/python-attrs/attrs/blob/master/changelog.d).

If you have *any* questions to *any* of the points above, just **submit and ask**!  This checklist is here to *help* you, not to deter you from contributing!
