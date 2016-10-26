import logging

logger = logging.getLogger("manifest")
logger.addHandler(logging.StreamHandler())
logger.setLevel(logging.DEBUG)

def get_logger():
    return logger
