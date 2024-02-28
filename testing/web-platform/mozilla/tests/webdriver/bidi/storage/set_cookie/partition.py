import pytest
from tests.bidi import recursive_compare
from tests.support.helpers import get_origin_from_url
from webdriver.bidi.modules.network import NetworkStringValue
from webdriver.bidi.modules.storage import (
    BrowsingContextPartitionDescriptor,
    PartialCookie,
)

pytestmark = pytest.mark.asyncio


async def test_partition_context(
    bidi_session,
    set_cookie,
    top_context,
    new_tab,
    test_page,
    test_page_cross_origin,
    domain_value,
):
    await bidi_session.browsing_context.navigate(
        context=new_tab["context"], url=test_page, wait="complete"
    )
    source_origin_1 = get_origin_from_url(test_page)
    await bidi_session.browsing_context.navigate(
        context=top_context["context"], url=test_page_cross_origin, wait="complete"
    )
    source_origin_2 = get_origin_from_url(test_page_cross_origin)

    cookie_name = "foo"
    cookie_value = "bar"
    cookie_domain = domain_value()
    new_tab_partition = BrowsingContextPartitionDescriptor(new_tab["context"])

    set_cookie_result = await set_cookie(
        cookie=PartialCookie(
            domain=cookie_domain,
            name=cookie_name,
            value=NetworkStringValue(cookie_value),
        ),
        partition=new_tab_partition,
    )

    assert set_cookie_result == {
        "partitionKey": {"sourceOrigin": source_origin_1, "userContext": "default"}
    }

    # Check that added cookies are present on the right context.
    cookies = await bidi_session.storage.get_cookies(partition=new_tab_partition)

    recursive_compare(
        {
            "cookies": [
                {
                    "domain": cookie_domain,
                    "httpOnly": False,
                    "name": cookie_name,
                    "path": "/",
                    "sameSite": "none",
                    "secure": False,
                    "size": 6,
                    "value": {"type": "string", "value": cookie_value},
                }
            ],
            "partitionKey": {"sourceOrigin": source_origin_1},
        },
        cookies,
    )

    # Check that added cookies are not present on the other context.
    cookies = await bidi_session.storage.get_cookies(
        partition=BrowsingContextPartitionDescriptor(top_context["context"])
    )

    recursive_compare(
        {
            "cookies": [],
            "partitionKey": {"sourceOrigin": source_origin_2, "userContext": "default"},
        },
        cookies,
    )


@pytest.mark.parametrize("domain", ["", "alt"], ids=["same_origin", "cross_origin"])
async def test_partition_context_iframe(
    bidi_session, new_tab, inline, domain_value, domain, set_cookie
):
    iframe_url = inline("<div id='in-iframe'>foo</div>", domain=domain)
    source_origin_for_iframe = get_origin_from_url(iframe_url)
    page_url = inline(f"<iframe src='{iframe_url}'></iframe>")
    source_origin_for_page = get_origin_from_url(page_url)
    await bidi_session.browsing_context.navigate(
        context=new_tab["context"], url=page_url, wait="complete"
    )

    contexts = await bidi_session.browsing_context.get_tree(root=new_tab["context"])
    iframe_context = contexts[0]["children"][0]
    iframe_partition = BrowsingContextPartitionDescriptor(iframe_context["context"])

    cookie_name = "foo"
    cookie_value = "bar"
    await set_cookie(
        cookie=PartialCookie(
            domain=domain_value(domain),
            name=cookie_name,
            value=NetworkStringValue(cookie_value),
        ),
        partition=iframe_partition,
    )

    # Check that added cookies are present on the right context
    cookies = await bidi_session.storage.get_cookies(partition=iframe_partition)

    expected_cookies = [
        {
            "domain": domain_value(domain=domain),
            "httpOnly": False,
            "name": cookie_name,
            "path": "/",
            "sameSite": "none",
            "secure": False,
            "size": 6,
            "value": {"type": "string", "value": cookie_value},
        }
    ]
    recursive_compare(
        {
            "cookies": expected_cookies,
            "partitionKey": {
                "sourceOrigin": source_origin_for_iframe,
                "userContext": "default",
            },
        },
        cookies,
    )

    cookies = await bidi_session.storage.get_cookies(
        partition=BrowsingContextPartitionDescriptor(new_tab["context"])
    )
    # When the iframe is on the different domain we can verify that top context has no iframe cookie.
    if domain == "alt":
        recursive_compare(
            {
                "cookies": [],
                "partitionKey": {
                    "sourceOrigin": source_origin_for_page,
                    "userContext": "default",
                },
            },
            cookies,
        )
    else:
        # When the iframe is on the same domain, since the browsing context partition is defined by user context and origin,
        # which will be the same for the page, we get the same cookies as for the iframe
        recursive_compare(
            {
                "cookies": expected_cookies,
                "partitionKey": {
                    "sourceOrigin": source_origin_for_page,
                    "userContext": "default",
                },
            },
            cookies,
        )
