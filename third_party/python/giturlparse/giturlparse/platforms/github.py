from .base import BasePlatform


class GitHubPlatform(BasePlatform):
    PATTERNS = {
        "https": (
            r"(?P<protocols>(git\+)?(?P<protocol>https))://(?P<domain>[^/]+?)"
            r"(?P<pathname>/(?P<owner>[^/]+?)/(?P<repo>[^/]+?)(?:\.git)?(?P<path_raw>(/blob/|/tree/).+)?)$"
        ),
        "ssh": (
            r"(?P<protocols>(git\+)?(?P<protocol>ssh))?(://)?git@(?P<domain>.+?)(?P<pathname>(:|/)"
            r"(?P<owner>[^/]+)/(?P<repo>[^/]+?)(?:\.git)"
            r"(?P<path_raw>(/blob/|/tree/).+)?)$"
        ),
        "git": (
            r"(?P<protocols>(?P<protocol>git))://(?P<domain>.+?)"
            r"(?P<pathname>/(?P<owner>[^/]+)/(?P<repo>[^/]+?)(?:\.git)?"
            r"(?P<path_raw>(/blob/|/tree/).+)?)$"
        ),
    }
    FORMATS = {
        "https": r"https://%(domain)s/%(owner)s/%(repo)s.git%(path_raw)s",
        "ssh": r"git@%(domain)s:%(owner)s/%(repo)s.git%(path_raw)s",
        "git": r"git://%(domain)s/%(owner)s/%(repo)s.git%(path_raw)s",
    }
    DOMAINS = (
        "github.com",
        "gist.github.com",
    )
    DEFAULTS = {"_user": "git"}

    @staticmethod
    def clean_data(data):
        data = BasePlatform.clean_data(data)
        if data["path_raw"].startswith("/blob/"):
            data["path"] = data["path_raw"].replace("/blob/", "")
        if data["path_raw"].startswith("/tree/"):
            data["branch"] = data["path_raw"].replace("/tree/", "")
        return data
