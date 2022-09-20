from .parser import parse as _parse
from .result import GitUrlParsed

__author__ = "Iacopo Spalletti"
__email__ = "i.spalletti@nephila.it"
__version__ = "0.10.0"


def parse(url, check_domain=True):
    return GitUrlParsed(_parse(url, check_domain))


def validate(url, check_domain=True):
    return parse(url, check_domain).valid
