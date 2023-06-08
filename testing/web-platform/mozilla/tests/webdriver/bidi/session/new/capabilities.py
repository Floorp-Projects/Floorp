import pytest
from tests.support import platform_name
from webdriver.bidi.modules.script import ContextTarget

pytestmark = pytest.mark.asyncio


@pytest.mark.skipif(
    platform_name is None, reason="Unsupported platform {}".format(platform_name)
)
@pytest.mark.parametrize("match_type", ["alwaysMatch", "firstMatch"])
async def test_platform_name(new_session, match_capabilities, match_type):
    capabilities = match_capabilities(match_type, "platformName", platform_name)

    bidi_session = await new_session(capabilities=capabilities)

    assert bidi_session.capabilities["platformName"] == platform_name


@pytest.mark.parametrize("match_type", ["alwaysMatch", "firstMatch"])
async def test_proxy(
    new_session, match_capabilities, server_config, inline, match_type
):
    domain = server_config["domains"][""][""]
    port = server_config["ports"]["http"][0]
    proxy_url = f"{domain}:{port}"
    proxy_capability = {"proxyType": "manual", "httpProxy": proxy_url}
    capabilities = match_capabilities(match_type, "proxy", proxy_capability)

    bidi_session = await new_session(capabilities=capabilities)

    assert bidi_session.capabilities["proxy"] == proxy_capability

    page_content = "proxy"
    page_url = inline(f"<div>{page_content}</div>")
    test_url = page_url.replace(proxy_url, "example.com")

    contexts = await bidi_session.browsing_context.get_tree()

    await bidi_session.browsing_context.navigate(
        context=contexts[0]["context"], url=test_url, wait="complete"
    )

    # Check that content is expected
    response = await bidi_session.script.evaluate(
        expression="""document.querySelector('div').textContent""",
        target=ContextTarget(contexts[0]["context"]),
        await_promise=False,
    )

    assert response == {"type": "string", "value": page_content}
