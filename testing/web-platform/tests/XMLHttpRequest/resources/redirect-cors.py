def main(request, response):
    response.status = 302
    location = request.GET.first("location")
    response.headers.set("Location", location)

    if "allow_origin" in request.GET:
        response.headers.set("Access-Control-Allow-Origin", request.headers.get("origin"))
