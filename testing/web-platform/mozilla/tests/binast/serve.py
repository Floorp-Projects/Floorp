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

def get_mine_type_and_extension(request, params):
    accept_binast = clientAcceptsBinAST(request)

    if 'expect_accept' in params:
        expect_accept = params['expect_accept'][0]
        if expect_accept == 'false':
            if accept_binast:
                raise Exception("Expect not accept binast")
        elif expect_accept == 'true':
            if not accept_binast:
                raise Exception("Expect accept binast")
        else:
            raise Exception("Bad check_header parameter: " + check_header)

    if 'force_binast' in params and params['force_binast'][0] == 'true':
        return BinASTMimeType, BinASTExtension

    if accept_binast:
        return BinASTMimeType, BinASTExtension

    return JavaScriptMimeType, SourceTextExtension


def main(request, response):
    params = urlparse.parse_qs(request.url_parts[3])
    name = params['name'][0]
    if not re.match(r'\w+$', name):
        raise Exception("Bad name parameter: " + name)

    mimeType, extension = get_mine_type_and_extension(request, params)

    scriptDir = os.path.dirname(__file__)
    contentPath = os.path.join(scriptDir, name + extension)
    return [("Content-Type", mimeType)], open(contentPath, "rb")
