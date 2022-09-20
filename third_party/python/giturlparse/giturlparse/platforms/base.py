import itertools
import re


class BasePlatform:
    FORMATS = {
        "ssh": r"(?P<protocols>(git\+)?(?P<protocol>ssh))?(://)?%(_user)s@%(host)s:%(repo)s.git",
        "http": r"(?P<protocols>(git\+)?(?P<protocol>http))://%(host)s/%(repo)s.git",
        "https": r"(?P<protocols>(git\+)?(?P<protocol>https))://%(host)s/%(repo)s.git",
        "git": r"(?P<protocols>(?P<protocol>git))://%(host)s/%(repo)s.git",
    }

    PATTERNS = {
        "ssh": r"(?P<_user>.+)@(?P<domain>[^/]+?):(?P<repo>.+).git",
        "http": r"http://(?P<domain>[^/]+?)/(?P<repo>.+).git",
        "https": r"https://(?P<domain>[^/]+?)/(?P<repo>.+).git",
        "git": r"git://(?P<domain>[^/]+?)/(?P<repo>.+).git",
    }

    # None means it matches all domains
    DOMAINS = None
    SKIP_DOMAINS = None
    DEFAULTS = {}

    def __init__(self):
        # Precompile PATTERNS
        self.COMPILED_PATTERNS = {proto: re.compile(regex, re.IGNORECASE) for proto, regex in self.PATTERNS.items()}

        # Supported protocols
        self.PROTOCOLS = self.PATTERNS.keys()

        if self.__class__ == BasePlatform:
            sub = [subclass.SKIP_DOMAINS for subclass in self.__class__.__subclasses__() if subclass.SKIP_DOMAINS]
            if sub:
                self.SKIP_DOMAINS = list(itertools.chain.from_iterable(sub))

    @staticmethod
    def clean_data(data):
        data["path"] = ""
        data["branch"] = ""
        data["protocols"] = list(filter(lambda x: x, data["protocols"].split("+")))
        data["pathname"] = data["pathname"].strip(":")
        return data
