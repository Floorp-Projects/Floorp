import pytest
from tests.bidi import recursive_compare
from tests.support.helpers import get_origin_from_url
from webdriver.bidi.modules.storage import BrowsingContextPartitionDescriptor

pytestmark = pytest.mark.asyncio


async def test_partition_context(
    bidi_session,
    new_tab,
    test_page,
    domain_value,
    add_cookie,
    top_context,
    test_page_cross_origin,
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
    await add_cookie(new_tab["context"], cookie_name, cookie_value)

    # Check that added cookies are present on the right context.
    cookies = await bidi_session.storage.get_cookies(
        partition=BrowsingContextPartitionDescriptor(new_tab["context"])
    )

    recursive_compare(
        {
            "cookies": [
                {
                    "domain": domain_value(),
                    "httpOnly": False,
                    "name": cookie_name,
                    "path": "/webdriver/tests/support",
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
    bidi_session, new_tab, inline, domain_value, domain, add_cookie
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

    cookie_name = "foo"
    cookie_value = "bar"
    await add_cookie(iframe_context["context"], cookie_name, cookie_value)

    # Check that added cookies are present on the right context
    cookies = await bidi_session.storage.get_cookies(
        partition=BrowsingContextPartitionDescriptor(iframe_context["context"])
    )

    expected_cookies = [
        {
            "domain": domain_value(domain=domain),
            "httpOnly": False,
            "name": cookie_name,
            "path": "/webdriver/tests/support",
            "sameSite": "none",
            "secure": False,
            "size": 6,
            "value": {"type": "string", "value": cookie_value},
        }
    ]
    recursive_compare(
        {
            "cookies": expected_cookies,
            "partitionKey": {"sourceOrigin": source_origin_for_iframe},
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
                "partitionKey": {"sourceOrigin": source_origin_for_page},
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
