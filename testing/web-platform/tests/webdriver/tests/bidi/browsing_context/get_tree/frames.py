import pytest

from .. import assert_browsing_context

pytestmark = pytest.mark.asyncio


async def test_multiple_frames(
    bidi_session,
    current_session,
    test_page,
    test_page2,
    test_page_multiple_frames,
):
    current_session.url = test_page_multiple_frames

    # First retrieve all browsing contexts of the current tab
    top_level_context_id = current_session.window_handle
    all_contexts = await bidi_session.browsing_context.get_tree(parent=top_level_context_id)

    assert len(all_contexts) == 1
    parent_info = all_contexts[0]
    assert_browsing_context(
        parent_info,
        top_level_context_id,
        children=2,
        parent=None,
        url=test_page_multiple_frames,
    )

    child1_info = parent_info["children"][0]
    assert_browsing_context(
        child1_info,
        context=None,
        children=0,
        is_root=False,
        parent=None,
        url=test_page,
    )
    assert child1_info["context"] != parent_info["context"]

    child2_info = parent_info["children"][1]
    assert_browsing_context(
        child2_info,
        context=None,
        children=0,
        is_root=False,
        parent=None,
        url=test_page2,
    )
    assert child2_info["context"] != parent_info["context"]
    assert child2_info["context"] != child1_info["context"]


async def test_cross_origin(
    bidi_session,
    current_session,
    test_page_cross_origin,
    test_page_cross_origin_frame,
):
    current_session.url = test_page_cross_origin_frame

    # First retrieve all browsing contexts of the current tab
    top_level_context_id = current_session.window_handle
    all_contexts = await bidi_session.browsing_context.get_tree(parent=top_level_context_id)

    assert len(all_contexts) == 1
    parent_info = all_contexts[0]
    assert_browsing_context(
        parent_info,
        top_level_context_id,
        children=1,
        parent=None,
        url=test_page_cross_origin_frame,
    )

    child1_info = parent_info["children"][0]
    assert_browsing_context(
        child1_info,
        context=None,
        children=0,
        is_root=False,
        parent=None,
        url=test_page_cross_origin,
    )
    assert child1_info["context"] != parent_info["context"]
