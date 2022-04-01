import pytest

from support.network import http_request


@pytest.mark.parametrize(
    "allow_origins, origin, status",
    [
        # Valid origins
        (["http://web-platform.test"], "http://web-platform.test", 200),
        (["http://web-platform.test"], "http://web-platform.test:80", 200),
        (["https://web-platform.test"], "https://web-platform.test:443", 200),
        # Invalid origins
        (["https://web-platform.test"], "http://web-platform.test", 500),
        (["http://web-platform.test:8000"], "http://web-platform.test", 500),
        (["http://web-platform.test"], "http://www.web-platform.test", 500),
    ],
)
def test_allow_hosts(configuration, geckodriver, allow_origins, origin, status):
    extra_args = ["--allow-origins"] + allow_origins

    driver = geckodriver(extra_args=extra_args)
    response = http_request(driver.hostname, driver.port, origin=origin)

    assert response.status == status
