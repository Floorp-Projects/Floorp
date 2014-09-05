import cgi
import json
import logging
import os
import traceback
import urllib
import urlparse

from constants import content_types
from pipes import Pipeline, template
from ranges import RangeParser
from response import MultipartContent
from utils import HTTPException

logger = logging.getLogger("wptserve")

__all__ = ["file_handler", "python_script_handler",
           "FunctionHandler", "handler", "json_handler",
           "as_is_handler", "ErrorHandler"]


def guess_content_type(path):
    ext = os.path.splitext(path)[1].lstrip(".")
    if ext in content_types:
        return content_types[ext]

    return "application/octet-stream"


class DirectoryHandler(object):
    def __call__(self, request, response):
        path = request.filesystem_path

        assert os.path.isdir(path)

        response.headers = [("Content-Type", "text/html")]
        response.content = """<!doctype html>
<meta name="viewport" content="width=device-width">
<title>Directory listing for %(path)s</title>
<h1>Directory listing for %(path)s</h1>
<ul>
%(items)s
</li>
""" % {"path": cgi.escape(request.url_parts.path),
       "items": "\n".join(self.list_items(request, path))}

    def list_items(self, request, path):
        base_path = request.url_parts.path
        filesystem_base = request.filesystem_path
        if not base_path.endswith("/"):
            base_path += "/"
        if base_path != "/":
            link = urlparse.urljoin(base_path, "..")
            yield ("""<li class="dir"><a href="%(link)s">%(name)s</a>""" %
                   {"link": link, "name": ".."})
        for item in sorted(os.listdir(path)):
            link = cgi.escape(urllib.quote(item))
            if os.path.isdir(os.path.join(filesystem_base, item)):
                link += "/"
                class_ = "dir"
            else:
                class_ = "file"
            yield ("""<li class="%(class)s"><a href="%(link)s">%(name)s</a>""" %
                   {"link": link, "name": cgi.escape(item), "class": class_})


directory_handler = DirectoryHandler()


class FileHandler(object):
    def __call__(self, request, response):
        path = request.filesystem_path

        if os.path.isdir(path):
            return directory_handler(request, response)
        try:
            #This is probably racy with some other process trying to change the file
            file_size = os.stat(path).st_size
            response.headers.update(self.get_headers(request, path))
            if "Range" in request.headers:
                try:
                    byte_ranges = RangeParser()(request.headers['Range'], file_size)
                except HTTPException as e:
                    if e.code == 416:
                        response.headers.set("Content-Range", "bytes */%i" % file_size)
                        raise
            else:
                byte_ranges = None
            data = self.get_data(response, path, byte_ranges)
            response.content = data
            query = urlparse.parse_qs(request.url_parts.query)

            pipeline = None
            if "pipe" in query:
                pipeline = Pipeline(query["pipe"][-1])
            elif os.path.splitext(path)[0].endswith(".sub"):
                pipeline = Pipeline("sub")
            if pipeline is not None:
                response = pipeline(request, response)

            return response

        except (OSError, IOError):
            raise HTTPException(404)

    def get_headers(self, request, path):
        rv = self.default_headers(path)
        rv.extend(self.load_headers(request, os.path.join(os.path.split(path)[0], "__dir__")))
        rv.extend(self.load_headers(request, path))
        return rv

    def load_headers(self, request, path):
        headers_path = path + ".sub.headers"
        if os.path.exists(headers_path):
            use_sub = True
        else:
            headers_path = path + ".headers"
            use_sub = False

        try:
            with open(headers_path) as headers_file:
                data = headers_file.read()
        except IOError:
            return []
        else:
            data = template(request, data)
            return [tuple(item.strip() for item in line.split(":", 1))
                    for line in data.splitlines() if line]

    def get_data(self, response, path, byte_ranges):
        with open(path, 'rb') as f:
            if byte_ranges is None:
                return f.read()
            else:
                response.status = 206
                if len(byte_ranges) > 1:
                    parts_content_type, content = self.set_response_multipart(response,
                                                                              byte_ranges,
                                                                              f)
                    for byte_range in byte_ranges:
                        content.append_part(self.get_range_data(f, byte_range),
                                            parts_content_type,
                                            [("Content-Range", byte_range.header_value())])
                    return content
                else:
                    response.headers.set("Content-Range", byte_ranges[0].header_value())
                    return self.get_range_data(f, byte_ranges[0])

    def set_response_multipart(self, response, ranges, f):
        parts_content_type = response.headers.get("Content-Type")
        if parts_content_type:
            parts_content_type = parts_content_type[-1]
        else:
            parts_content_type = None
        content = MultipartContent()
        response.headers.set("Content-Type", "multipart/byteranges; boundary=%s" % content.boundary)
        return parts_content_type, content

    def get_range_data(self, f, byte_range):
        f.seek(byte_range.lower)
        return f.read(byte_range.upper - byte_range.lower)

    def default_headers(self, path):
        return [("Content-Type", guess_content_type(path))]


file_handler = FileHandler()


def python_script_handler(request, response):
    path = request.filesystem_path

    try:
        environ = {"__file__": path}
        execfile(path, environ, environ)
        if "main" in environ:
            handler = FunctionHandler(environ["main"])
            handler(request, response)
        else:
            raise HTTPException(500)
    except IOError:
        raise HTTPException(404)


def FunctionHandler(func):
    def inner(request, response):
        try:
            rv = func(request, response)
        except:
            msg = traceback.format_exc()
            raise HTTPException(500, message=msg)
        if rv is not None:
            if isinstance(rv, tuple):
                if len(rv) == 3:
                    status, headers, content = rv
                    response.status = status
                elif len(rv) == 2:
                    headers, content = rv
                else:
                    raise HTTPException(500)
                response.headers.update(headers)
            else:
                content = rv
            response.content = content
    return inner


#The generic name here is so that this can be used as a decorator
handler = FunctionHandler


def json_handler(func):
    def inner(request, response):
        rv = func(request, response)
        response.headers.set("Content-Type", "application/json")
        enc = json.dumps
        if isinstance(rv, tuple):
            rv = list(rv)
            value = tuple(rv[:-1] + [enc(rv[-1])])
            length = len(value[-1])
        else:
            value = enc(rv)
            length = len(value)
        response.headers.set("Content-Length", length)
        return value
    return FunctionHandler(inner)


def as_is_handler(request, response):
    path = request.filesystem_path
    try:
        with open(path) as f:
            response.writer.write_content(f.read())
        response.close_connection = True
    except IOError:
        raise HTTPException(404)


class ErrorHandler(object):
    def __init__(self, status):
        self.status = status

    def __call__(self, request, response):
        response.set_error(self.status)
