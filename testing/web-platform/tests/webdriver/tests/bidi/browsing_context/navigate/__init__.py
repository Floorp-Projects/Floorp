async def navigate_and_assert(bidi_session, context, url, wait="complete"):
    result = await bidi_session.browsing_context.navigate(
        context=context['context'], url=url, wait=wait
    )
    assert result["url"] == url

    contexts = await bidi_session.browsing_context.get_tree(
        root=context['context']
    )
    assert len(contexts) == 1
    assert contexts[0]["url"] == url

    return contexts
