from .base import BasePlatform


class AssemblaPlatform(BasePlatform):
    DOMAINS = ("git.assembla.com",)
    PATTERNS = {
        "ssh": r"(?P<protocols>(git\+)?(?P<protocol>ssh))?(://)?git@(?P<domain>.+?):(?P<pathname>(?P<repo>.+)).git",
        "git": r"(?P<protocols>(?P<protocol>git))://(?P<domain>.+?)/(?P<pathname>(?P<repo>.+)).git",
    }
    FORMATS = {
        "ssh": r"git@%(domain)s:%(repo)s.git",
        "git": r"git://%(domain)s/%(repo)s.git",
    }
    DEFAULTS = {"_user": "git"}
