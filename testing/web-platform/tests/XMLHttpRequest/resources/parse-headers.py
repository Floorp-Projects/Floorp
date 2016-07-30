import json

def main(request, response):

    content = ""
    if "my-custom-header" in request.GET:
        val = request.GET.first("my-custom-header")
        print "header is about to be set to '{}'".format(val)
        response.headers.set("My-Custom-Header", val)
        print "header was set"

    return content
