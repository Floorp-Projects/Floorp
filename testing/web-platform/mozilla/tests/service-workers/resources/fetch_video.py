import os


def main(request, response):
    filename = os.path.join(request.doc_root, "media", "2x2-green.ogv")
    body = open(filename, "rb").read()
    length = len(body)
    headers = [
        (b"Content-Type", b"video/ogg"),
        (b"Accept-Ranges", b"bytes"),
        (b"Content-Length", b"%d" % length),
        (b"Content-Range", b"bytes 0-%d/%d" % (length - 1, length)),
    ]
    return headers, body
