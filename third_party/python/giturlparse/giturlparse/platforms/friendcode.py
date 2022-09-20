from .base import BasePlatform


class FriendCodePlatform(BasePlatform):
    DOMAINS = ("friendco.de",)
    PATTERNS = {
        "https": (
            r"(?P<protocols>(git\+)?(?P<protocol>https))://(?P<domain>.+?)/"
            r"(?P<pathname>(?P<owner>.+)@user/(?P<repo>.+)).git"
        ),
    }
    FORMATS = {
        "https": r"https://%(domain)s/%(owner)s@user/%(repo)s.git",
    }
