# Serve BinAST encoded JS if the client requests it.

import re
import os.path
import urlparse

JavaScriptMimeType = "application/javascript"
BinASTMimeType = "application/javascript-binast"

SourceTextExtension = ".js"
BinASTExtension = ".binjs"

def clientAcceptsBinAST(request):
    if "accept" not in request.headers:
        return False

    encodings = [s.strip().lower() for s in request.headers["accept"].split(",")]
    return BinASTMimeType in encodings

def main(request, response):
    params = urlparse.parse_qs(request.url_parts[3])
    name = params['name'][0]
    if not re.match(r'\w+$', name):
        raise Exception("Bad name parameter: " + name)

    if clientAcceptsBinAST(request):
        mimeType = BinASTMimeType
        extension = BinASTExtension
    else:
        mimeType = JavaScriptMimeType
        extension = SourceTextExtension

    scriptDir = os.path.dirname(__file__)
    contentPath = os.path.join(scriptDir, name + extension)
    return [("Content-Type", mimeType)], open(contentPath, "rb")
