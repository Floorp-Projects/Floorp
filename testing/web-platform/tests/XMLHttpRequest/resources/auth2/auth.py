import imp
import os

def main(request, response):
    auth = imp.load_source("", os.path.join(os.path.abspath(os.curdir),
                                            "XMLHttpRequest",
                                            "resources",
                                            "authentication.py"))
    return auth.main(request, response)
