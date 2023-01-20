import pytest

from .common import verify_redirectors

URLS = {
    "https://ads.advertising.com/x.js?1": "js",
    "https://ads.advertising.com/x?1": "image",
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
