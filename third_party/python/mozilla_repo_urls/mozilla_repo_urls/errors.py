class RepoUrlsBaseError(Exception):
    pass


class InvalidRepoUrlError(RepoUrlsBaseError):
    def __init__(self, url_string) -> None:
        super().__init__(f"Could not parse URL: {url_string}")


class UnsupportedPlatformError(RepoUrlsBaseError):
    def __init__(self, url_string, platform, supported_platforms) -> None:
        super().__init__(
            f"Unsupported platform. Got: {platform}. "
            f"Expected: {supported_platforms}. URL: {url_string}"
        )
