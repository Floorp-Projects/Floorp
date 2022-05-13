import pytest

pytestmark = pytest.mark.asyncio

PAGE_ABOUT_BLANK = "about:blank"
PAGE_EMPTY = "/webdriver/tests/bidi/browsing_context/navigate/support/empty.html"

async def test_navigate_from_single_page(bidi_session, new_tab, url):
    url_before = url(PAGE_EMPTY)

    result = await bidi_session.browsing_context.navigate(
        context=new_tab["context"], url=url_before, wait="complete"
    )
    contexts = await bidi_session.browsing_context.get_tree(
        root=new_tab["context"], max_depth=0
    )
    assert contexts[0]["url"] == url_before

    result = await bidi_session.browsing_context.navigate(
        context=new_tab["context"], url=PAGE_ABOUT_BLANK, wait="complete"
    )
    assert result["url"] == PAGE_ABOUT_BLANK

    contexts = await bidi_session.browsing_context.get_tree(
        root=new_tab["context"], max_depth=0
    )
    assert contexts[0]["url"] == PAGE_ABOUT_BLANK

async def test_navigate_from_frameset(bidi_session, inline, new_tab, url):
    frame_url = url(PAGE_EMPTY)
    url_before = inline(f"<frameset><frame src='{frame_url}'/></frameset")
    result = await bidi_session.browsing_context.navigate(
        context=new_tab["context"], url=url_before, wait="complete"
    )
    contexts = await bidi_session.browsing_context.get_tree(
        root=new_tab["context"], max_depth=0
    )
    assert contexts[0]["url"] == url_before

    result = await bidi_session.browsing_context.navigate(
        context=new_tab["context"], url=PAGE_ABOUT_BLANK, wait="complete"
    )
    assert result["url"] == PAGE_ABOUT_BLANK

    contexts = await bidi_session.browsing_context.get_tree(
        root=new_tab["context"], max_depth=0
    )
    assert contexts[0]["url"] == PAGE_ABOUT_BLANK


async def test_navigate_in_iframe(bidi_session, inline, new_tab):
    frame_start_url = inline("frame")
    url_before = inline(
        f"<iframe src='{frame_start_url}'></iframe>"
    )
    result = await bidi_session.browsing_context.navigate(
        context=new_tab["context"], url=url_before, wait="complete"
    )
    assert result["url"] == url_before

    contexts = await bidi_session.browsing_context.get_tree(root=new_tab["context"])
    assert contexts[0]["url"] == url_before

    assert len(contexts[0]["children"]) == 1
    frame = contexts[0]["children"][0]
    frame_result = await bidi_session.browsing_context.navigate(
        context=frame["context"], url=PAGE_ABOUT_BLANK, wait="complete"
    )
    assert frame_result["url"] == PAGE_ABOUT_BLANK

    contexts = await bidi_session.browsing_context.get_tree(root=frame["context"])
    assert len(contexts) == 1
    assert contexts[0]["url"] == PAGE_ABOUT_BLANK
