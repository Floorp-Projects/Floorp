class RepoUrlsBaseError(Exception):
    pass


class InvalidRepoUrlError(RepoUrlsBaseError):
    def __init__(self, url_string) -> None:
        super().__init__(f"Could not parse URL: {url_string}")


class UnsupportedPlatformError(RepoUrlsBaseError):
    def __init__(self, url_string, host, supported_hosts) -> None:
        super().__init__(
            f"Unsupported version control host. Got: {host}. "
            f"Expected one of: {supported_hosts}. URL: {url_string}"
        )
