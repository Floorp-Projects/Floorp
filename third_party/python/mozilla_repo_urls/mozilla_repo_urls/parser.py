import giturlparse

from mozilla_repo_urls.platforms import ADDITIONAL_PLATFORMS

from .errors import InvalidRepoUrlError, UnsupportedPlatformError
from .result import RepoUrlParsed

for i, platform in enumerate(ADDITIONAL_PLATFORMS):
    giturlparse.platforms.PLATFORMS.insert(i, platform)


_SUPPORTED_PLAFORMS = ("hgmo", "github")


def parse(url_string):
    # Workaround for https://github.com/nephila/giturlparse/issues/43
    url_string = url_string.rstrip("/")
    parsed_info = giturlparse.parser.parse(url_string)
    parsed_url = RepoUrlParsed(parsed_info)

    if not parsed_url.valid:
        raise InvalidRepoUrlError(url_string)

    if parsed_url.platform not in _SUPPORTED_PLAFORMS:
        raise UnsupportedPlatformError(url_string, platform, _SUPPORTED_PLAFORMS)

    return parsed_url
