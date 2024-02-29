import pytest
from tests.bidi import recursive_compare
from tests.support.helpers import get_origin_from_url
from webdriver.bidi.modules.network import NetworkStringValue
from webdriver.bidi.modules.storage import (
    BrowsingContextPartitionDescriptor,
    PartialCookie,
)

pytestmark = pytest.mark.asyncio


@pytest.mark.parametrize(
    "with_document_cookie",
    [True, False],
    ids=["with document cookie", "with set cookie"],
)
async def test_partition_context(
    bidi_session,
    new_tab,
    test_page,
    domain_value,
    add_cookie,
    set_cookie,
    with_document_cookie,
):
    await bidi_session.browsing_context.navigate(
        context=new_tab["context"], url=test_page, wait="complete"
    )

    cookie_name = "foo"
    cookie_value = "bar"
    source_origin = get_origin_from_url(test_page)
    partition = BrowsingContextPartitionDescriptor(new_tab["context"])
    if with_document_cookie:
        await add_cookie(new_tab["context"], cookie_name, cookie_value)
    else:
        await set_cookie(
            cookie=PartialCookie(
                domain=domain_value(),
                name=cookie_name,
                value=NetworkStringValue(cookie_value),
            ),
            partition=partition,
        )

    result = await bidi_session.storage.delete_cookies(partition=partition)
    recursive_compare({"partitionKey": {"sourceOrigin": source_origin}}, result)

    result = await bidi_session.storage.get_cookies(partition=partition)
    assert result["cookies"] == []


@pytest.mark.parametrize("domain", ["", "alt"], ids=["same_origin", "cross_origin"])
async def test_partition_context_iframe_with_set_cookie(
    bidi_session, new_tab, inline, domain_value, domain, set_cookie
):
    iframe_url = inline("<div id='in-iframe'>foo</div>", domain=domain)
    page_url = inline(f"<iframe src='{iframe_url}'></iframe>")
    await bidi_session.browsing_context.navigate(
        context=new_tab["context"], url=page_url, wait="complete"
    )
    source_origin = get_origin_from_url(iframe_url)

    contexts = await bidi_session.browsing_context.get_tree(root=new_tab["context"])
    iframe_context = contexts[0]["children"][0]

    cookie_name = "foo"
    cookie_value = "bar"
    frame_partition = BrowsingContextPartitionDescriptor(iframe_context["context"])
    await set_cookie(
        cookie=PartialCookie(
            domain=domain_value(domain),
            name=cookie_name,
            value=NetworkStringValue(cookie_value),
        ),
        partition=frame_partition,
    )

    result = await bidi_session.storage.delete_cookies(partition=frame_partition)
    recursive_compare({"partitionKey": {"sourceOrigin": source_origin}}, result)

    result = await bidi_session.storage.get_cookies(partition=frame_partition)
    assert result["cookies"] == []


# Because of Dynamic First-Party Isolation, adding the cookie with `document.cookie`
# works only with same-origin iframes.
async def test_partition_context_same_origin_iframe_with_document_cookie(
    bidi_session,
    new_tab,
    inline,
    add_cookie,
):
    iframe_url = inline("<div id='in-iframe'>foo</div>")
    page_url = inline(f"<iframe src='{iframe_url}'></iframe>")
    await bidi_session.browsing_context.navigate(
        context=new_tab["context"], url=page_url, wait="complete"
    )
    source_origin = get_origin_from_url(iframe_url)

    contexts = await bidi_session.browsing_context.get_tree(root=new_tab["context"])
    iframe_context = contexts[0]["children"][0]

    cookie_name = "foo"
    cookie_value = "bar"
    frame_partition = BrowsingContextPartitionDescriptor(iframe_context["context"])
    await add_cookie(iframe_context["context"], cookie_name, cookie_value)

    result = await bidi_session.storage.delete_cookies(partition=frame_partition)
    recursive_compare({"partitionKey": {"sourceOrigin": source_origin}}, result)

    result = await bidi_session.storage.get_cookies(partition=frame_partition)
    assert result["cookies"] == []
