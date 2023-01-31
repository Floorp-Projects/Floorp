from mozilla_repo_urls import parser


def test_supported_hosts():
    assert parser.SUPPORTED_HOSTS == (
        "gist.github.com",
        "github.com",
        "hg.mozilla.org",
    )
