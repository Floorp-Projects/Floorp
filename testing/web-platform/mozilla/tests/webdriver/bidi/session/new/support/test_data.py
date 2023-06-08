def product(a, b):
    return [(a, item) for item in b]


def flatten(l):
    return [item for x in l for item in x]


# Note that we can only test things here all implementations must support
valid_data = [
    (
        "acceptInsecureCerts",
        [
            False,
            None,
        ],
    ),
    (
        "browserName",
        [
            None,
        ],
    ),
    (
        "browserVersion",
        [
            None,
        ],
    ),
    (
        "platformName",
        [
            None,
        ],
    ),
    (
        "proxy",
        [
            None,
        ],
    ),
    (
        "test:extension",
        [
            None,
            False,
            "abc",
            123,
            [],
            {"key": "value"},
        ],
    ),
]

flat_valid_data = flatten(product(*item) for item in valid_data)

invalid_data = [
    (
        "acceptInsecureCerts",
        [
            1,
            [],
            {},
            "false",
        ],
    ),
    (
        "browserName",
        [
            1,
            [],
            {},
            False,
        ],
    ),
    (
        "browserVersion",
        [
            1,
            [],
            {},
            False,
        ],
    ),
    (
        "platformName",
        [
            1,
            [],
            {},
            False,
        ],
    ),
    (
        "proxy",
        [
            1,
            [],
            "{}",
            {"proxyType": "SYSTEM"},
            {"proxyType": "systemSomething"},
            {"proxy type": "pac"},
            {"proxy-Type": "system"},
            {"proxy_type": "system"},
            {"proxytype": "system"},
            {"PROXYTYPE": "system"},
            {"proxyType": None},
            {"proxyType": 1},
            {"proxyType": []},
            {"proxyType": {"value": "system"}},
            {" proxyType": "system"},
            {"proxyType ": "system"},
            {"proxyType ": " system"},
            {"proxyType": "system "},
        ],
    ),
]

flat_invalid_data = flatten(product(*item) for item in invalid_data)

invalid_extensions = [
    "automaticInspection",
    "automaticProfiling",
    "browser",
    "chromeOptions",
    "ensureCleanSession",
    "firefox",
    "firefox_binary",
    "firefoxOptions",
    "initialBrowserUrl",
    "javascriptEnabled",
    "logFile",
    "logLevel",
    "nativeEvents",
    "platform",
    "platformVersion",
    "profile",
    "requireWindowFocus",
    "safari.options",
    "seleniumProtocol",
    "trustAllSSLCertificates",
    "version",
]
