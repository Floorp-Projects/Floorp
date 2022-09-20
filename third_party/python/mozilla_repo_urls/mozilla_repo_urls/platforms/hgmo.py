from giturlparse.platforms.base import BasePlatform


class HgmoPlatform(BasePlatform):
    PATTERNS = {
        "https": (
            r"(?P<protocols>(?P<protocol>https))://"
            r"(?P<domain>[^/]+?)"
            r"(?P<pathname>/"
            r"(?P<repo>(([^/]+?)(/)?){1,2}))"
            r"(?P<path_raw>(/raw-file/|/file/).+)?$"
        ),
        "ssh": (
            r"(?P<protocols>(?P<protocol>ssh))(://)?"
            r"(?P<domain>.+?)"
            r"(?P<pathname>(:|/))"
            r"(?P<repo>(([^/]+?)(/)?){1,2})/?$"
        ),
    }
    FORMATS = {
        "https": r"https://%(domain)s/%(repo)s%(path_raw)s",
        "ssh": r"ssh://%(domain)s/%(repo)s",
    }
    DOMAINS = ("hg.mozilla.org",)
    DEFAULTS = {"_user": ""}

    @staticmethod
    def clean_data(data):
        data = BasePlatform.clean_data(data)
        if data["path_raw"].startswith(("/raw-file/", "/file")):
            data["path"] = (
                data["path_raw"].replace("/raw-file/", "").replace("/file/", "")
            )
        return data
