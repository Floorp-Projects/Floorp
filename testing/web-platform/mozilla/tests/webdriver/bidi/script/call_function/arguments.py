import pytest
from webdriver.bidi import error
from webdriver.bidi.modules.script import ContextTarget


@pytest.mark.asyncio
async def test_remote_reference_node_argument_different_browsing_context(
    bidi_session, get_test_page, top_context
):
    await bidi_session.browsing_context.navigate(
        context=top_context["context"], url=get_test_page(), wait="complete"
    )

    remote_reference = await bidi_session.script.evaluate(
        expression="""document.querySelector("#button")""",
        await_promise=False,
        target=ContextTarget(top_context["context"]),
    )

    # Retrieve the first child browsing context
    all_contexts = await bidi_session.browsing_context.get_tree(
        root=top_context["context"]
    )
    assert len(all_contexts) == 1
    child_context = all_contexts[0]["children"][0]["context"]

    with pytest.raises(error.NoSuchNodeException):
        await bidi_session.script.call_function(
            function_declaration="(node) => node.nodeType",
            arguments=[remote_reference],
            await_promise=False,
            target=ContextTarget(child_context),
        )
