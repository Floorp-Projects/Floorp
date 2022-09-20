from .base import BasePlatform


class GitLabPlatform(BasePlatform):
    PATTERNS = {
        "https": (
            r"(?P<protocols>(git\+)?(?P<protocol>https))://(?P<domain>.+?)(?P<port>:[0-9]+)?"
            r"(?P<pathname>/(?P<owner>[^/]+?)/"
            r"(?P<groups_path>.*?)?(?(groups_path)/)?(?P<repo>[^/]+?)(?:\.git)?"
            r"(?P<path_raw>(/blob/|/-/tree/).+)?)$"
        ),
        "ssh": (
            r"(?P<protocols>(git\+)?(?P<protocol>ssh))?(://)?git@(?P<domain>.+?):(?P<port>[0-9]+)?(?(port))?"
            r"(?P<pathname>/?(?P<owner>[^/]+)/"
            r"(?P<groups_path>.*?)?(?(groups_path)/)?(?P<repo>[^/]+?)(?:\.git)?"
            r"(?P<path_raw>(/blob/|/-/tree/).+)?)$"
        ),
        "git": (
            r"(?P<protocols>(?P<protocol>git))://(?P<domain>.+?):(?P<port>[0-9]+)?(?(port))?"
            r"(?P<pathname>/?(?P<owner>[^/]+)/"
            r"(?P<groups_path>.*?)?(?(groups_path)/)?(?P<repo>[^/]+?)(?:\.git)?"
            r"(?P<path_raw>(/blob/|/-/tree/).+)?)$"
        ),
    }
    FORMATS = {
        "https": r"https://%(domain)s/%(owner)s/%(groups_slash)s%(repo)s.git%(path_raw)s",
        "ssh": r"git@%(domain)s:%(port_slash)s%(owner)s/%(groups_slash)s%(repo)s.git%(path_raw)s",
        "git": r"git://%(domain)s%(port)s/%(owner)s/%(groups_slash)s%(repo)s.git%(path_raw)s",
    }
    SKIP_DOMAINS = (
        "github.com",
        "gist.github.com",
    )
    DEFAULTS = {"_user": "git", "port": ""}

    @staticmethod
    def clean_data(data):
        data = BasePlatform.clean_data(data)
        if data["path_raw"].startswith("/blob/"):
            data["path"] = data["path_raw"].replace("/blob/", "")
        if data["path_raw"].startswith("/-/tree/"):
            data["branch"] = data["path_raw"].replace("/-/tree/", "")
        return data
