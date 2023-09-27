import pytest

URL = (
    "https://www.saxoinvestor.fr/login/?adobe_mc="
    "MCORGID%3D173338B35278510F0A490D4C%2540AdobeOrg%7CTS%3D1621688498"
)


@pytest.mark.only_platforms("android")
@pytest.mark.asyncio
@pytest.mark.with_interventions
async def test_enabled(client):
    await client.navigate(URL)
    userid = client.find_css("input#field_userid")
    password = client.find_css("input#field_password")
    submit = client.find_css("input#button_login")
    assert client.is_displayed(userid)
    assert client.is_displayed(password)
    assert client.is_displayed(submit)


@pytest.mark.only_platforms("android")
@pytest.mark.asyncio
@pytest.mark.without_interventions
async def test_disabled(client):
    await client.navigate(URL)
    warning = client.find_css("#browser_support_section")
    assert client.is_displayed(warning)
