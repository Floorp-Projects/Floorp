def main(request, response):
    mime = request.GET.first("mime", "text/event-stream")
    message = request.GET.first("message", "data: data");
    newline = "" if request.GET.first("newline", None) == "none" else "\n\n";

    headers = [("Content-Type", mime)]
    body = message + newline + "\n"

    return headers, body
