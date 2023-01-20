import pytest

from .common import verify_redirectors

URLS = {
    "https://x.adsafeprotected.com/x.gif?1": "image",
    "https://x.adsafeprotected.com/x.png?1": "image",
    "https://x.adsafeprotected.com/x/x": "image",
    "https://x.adsafeprotected.com/img": "image",
    "https://x.adsafeprotected.com/x.js?1": "js",
    "https://x.adsafeprotected.com/x/adj?1": "js",
    "https://x.adsafeprotected.com/x/imp/1": "js",
    "https://x.adsafeprotected.com/x/Serving/1": "js",
    "https://x.adsafeprotected.com/x/unit/1": "js",
    "https://x.adsafeprotected.com/jload": "js",
    "https://x.adsafeprotected.com/jload?1": "js",
    "https://x.adsafeprotected.com/jsvid": "js",
    "https://x.adsafeprotected.com/jsvid?1": "js",
    "https://x.adsafeprotected.com/mon?1": "js",
    "https://x.adsafeprotected.com/tpl": "js",
    "https://x.adsafeprotected.com/tpl?1": "js",
    "https://x.adsafeprotected.com/services/pub?1": "js",
}


@pytest.mark.asyncio
@pytest.mark.with_strict_etp
@pytest.mark.with_shims
async def test_works_with_shims(client):
    await verify_redirectors(client, URLS, "REDIRECTED")


@pytest.mark.asyncio
@pytest.mark.with_strict_etp
@pytest.mark.without_shims
async def test_works_without_etp(client):
    await verify_redirectors(client, URLS, "BLOCKED")
