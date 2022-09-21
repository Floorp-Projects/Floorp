from giturlparse.platforms.base import BasePlatform


class HgmoPlatform(BasePlatform):
    PATTERNS = {
        "hg": (
            r"(?P<protocols>(?P<protocol>hg))://"
            r"(?P<domain>[^/]+?)"
            r"(?P<transport_protocol>(|:https|:ssh))"
            r"(?P<pathname>/"
            r"(?P<repo>(([^/]+?)(/)?){1,2}))/?$"
        ),
        "https": (
            r"(?P<protocols>(hg::)?(?P<protocol>https))://"
            r"(?P<domain>[^/]+?)"
            r"(?P<pathname>/"
            r"(?P<repo>(([^/]+?)(/)?){1,2}))"
            r"(?P<path_raw>(/raw-file/|/file/).+)?$"
        ),
        "ssh": (
            r"(?P<protocols>(hg::)?(?P<protocol>ssh))(://)?"
            r"(?P<domain>.+?)"
            r"(?P<pathname>(:|/)"
            r"(?P<repo>(([^/]+?)(/)?){1,2}))/?$"
        ),
    }
    FORMATS = {
        "hg": r"hg://%(domain)s:%(transport_protocol)s/%(repo)s",
        "https": r"https://%(domain)s/%(repo)s%(path_raw)s",
        "ssh": r"ssh://%(domain)s/%(repo)s",
    }
    DOMAINS = ("hg.mozilla.org",)
    DEFAULTS = {"_user": ""}

    @staticmethod
    def clean_data(data):
        data = BasePlatform.clean_data(data)
        if "path_raw" in data and data["path_raw"].startswith(("/raw-file/", "/file")):
            data["path"] = (
                data["path_raw"].replace("/raw-file/", "").replace("/file/", "")
            )

        # git-cinnabar fixtures
        if "transport_protocol" in data:
            if not data["transport_protocol"]:
                data["transport_protocol"] = "https"
            if data["transport_protocol"].startswith(":"):
                data["transport_protocol"] = data["transport_protocol"][1:]
            data["protocols"][
                0
            ] = f"{data['protocols'][0]}::{data['transport_protocol']}"
        else:
            data["transport_protocol"] = data["protocol"]

        return data
