import pytest

URL = "https://open.toutiao.com/a6966170645738783269/?__publisher_id__=4221768740&channel_id=88805669586&dt=R7Plusm&fr=normal&from_gid=6956432767005491719&gy=2cb247169b873bc0f78107b5a569f282d129ede38a56330ac9edc00743af6ba6f301248cddcb5b2376b2b286f219a9938e69c1c33941e7892a56b16c8617ebb0b1cd50cb401ece07ea7107a158f0b3b0cb95539be68ebda39413081f8dfe1d7ef83abb830d081642aa72639dc2c3e34109c846be7df23727854e76248ce909576f29134ee85086a27255c0745783a0b2f39d213d1a0ee09adb787da51569e05f525c3ad201d6638c&item_id=6966170645738783269&label=related_news&oppo_anchor=&react_gray=1&req_id=2021070203235101015120108107B22076&utm_campaign=open&utm_medium=webview&utm_source=o_llq_api"
API_MISSING_MSG = "navigator.connection is undefined"
GOOD_CSS = "#pageletArticleContent"


@pytest.mark.only_platforms("android")
@pytest.mark.asyncio
@pytest.mark.with_interventions
async def test_enabled(client):
    await client.navigate(URL)
    assert client.await_css(GOOD_CSS, is_displayed=True)


@pytest.mark.only_platforms("android")
@pytest.mark.asyncio
@pytest.mark.without_interventions
async def test_disabled(client):
    await client.navigate(URL, wait="load", await_console_message=API_MISSING_MSG)
