def assert_browsing_context(info, children, context, url, parent=None):
    assert "children" in info
    if children is not None:
        assert isinstance(info["children"], list)
    assert info["children"] == children

    assert "context" in info
    assert isinstance(info["context"], str)
    # TODO: Always check for context once we can can get the context for frames
    # by using browsingContext.getTree.
    if context is not None:
        assert info["context"] == context

    # parent context is optional but will always be present because emitted
    # events only contain the current browsing context and no childs.
    assert "parent" in info
    if children is not None:
        assert isinstance(info["parent"], str)
    assert info["parent"] == parent

    assert "url" in info
    assert isinstance(info["url"], str)
    assert info["url"] == url
