import logging

__version__ = "0.1.1"


try:  # Python 2.7+
    from logging import NullHandler
except ImportError:
    class NullHandler(logging.Handler):
        def emit(self, record):
            pass

# Set default logging handler to avoid "No handler found" warnings.
logging.getLogger(__name__).addHandler(NullHandler())

# exported api
from dlmanager.manager import Download, DownloadInterrupt, DownloadManager  # noqa
from dlmanager.persist_limit import PersistLimit  # noqa
