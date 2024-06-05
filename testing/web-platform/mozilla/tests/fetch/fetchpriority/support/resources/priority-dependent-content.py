import os
import re


def parse_priority(priority):
    # This is an approximate parsing of the priority header that is good enough
    # for testing purpose.
    # https://www.rfc-editor.org/rfc/rfc9218.html#section-5
    # https://www.rfc-editor.org/rfc/rfc9218.html#name-priority-parameters
    # https://www.rfc-editor.org/rfc/rfc8941.html#name-dictionaries
    # https://www.rfc-editor.org/rfc/rfc7230.html#section-3.2.3
    # https://www.rfc-editor.org/rfc/rfc8941.html#section-3.3.1
    urgency = 3
    if priority is not None:
        for key_value in priority.decode("utf-8").split(","):
            try:
                # This server ignores the incremental and unknown parameters.
                # https://www.rfc-editor.org/rfc/rfc9218.html#section-4.2
                key_and_value = key_value.strip().split("=", 1)
                if len(key_and_value) > 1:
                    key, value = key_and_value
                    if key == "u":
                        urgency = min(7, max(0, int(value)))
            except Exception as e:
                print(e)
    return urgency


def get_content_from_file(filename):
    current_dir = os.path.dirname(os.path.realpath(__file__))
    file_path = os.path.join(current_dir, filename)
    return open(file_path, "rb").read()


def get_content(as_type, resource_id, urgency):
    if as_type == "text":
        return "text/plain", "id=%s,u=%d\n%s" % (resource_id, urgency, "A" * urgency)
    if as_type == "script":
        return (
            "text/javascript",
            "/* id=%s,u=%d */\nself.scriptUrgency = self.scriptUrgency || {};\nself.scriptUrgency['%s'] = %d;"
            % (resource_id, urgency, resource_id, urgency),
        )
    size = (urgency + 1) * 10
    if as_type == "style":
        return (
            "text/css",
            "/* id=%s,u=%d */\n#urgency_square_%s { display: inline-block; width: %dpx; height: %dpx; background: black; }"
            % (resource_id, urgency, resource_id, size, size),
        )
    if as_type == "image":
        return (
            "image/svg+xml",
            '<?xml version="1.0" encoding="UTF-8"?>\n\
<!-- id=%s,u=%d -->\n\
<svg xmlns="http://www.w3.org/2000/svg" width="%dpx" height="%dpx">\n\
  <rect width="%dpx" height="%dpx" fill="black"/>\n\
</svg>'
            % (resource_id, urgency, size, size, size, size),
        )
    if as_type == "font":
        return (
            "font/woff2",
            get_content_from_file("font_%dem.woff2" % (urgency + 1)),
        )
    return None, None


def main(request, response):
    as_type = request.GET[b"as-type"].decode("utf-8")
    resource_id = request.GET[b"resource-id"].decode("utf-8")
    if re.match(r"^\w[\w\-]*$", resource_id) is None:
        response.set_error(400, "Bad resource-id!")
        return

    urgency = parse_priority(request.headers.get(b"priority"))
    content_type, content = get_content(as_type, resource_id, urgency)

    if content_type is None:
        response.set_error(400, "Unsupported as-type=%s!" % as_type)
        return

    # The server can reply with a priority, but it's probably better not to do
    # that in case this as any side effect. Instead, WPT tests are expected to
    # rely on the actual content returned to determine which priority was
    # received by the server.
    # https://www.rfc-editor.org/rfc/rfc9218.html#section-5
    response.status = 200
    response.headers[b"content-type"] = content_type
    response.write_status_headers()
    response.writer.write_data(item=content, last=True)
