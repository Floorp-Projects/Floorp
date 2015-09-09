# http://aspn.activestate.com/ASPN/Cookbook/Python/Recipe/146306
#   Submitter: Wade Leftwich
# Licensing:
#   according to http://aspn.activestate.com/ASPN/Cookbook/Python
#   "Except where otherwise noted, recipes in the Python Cookbook
#    are published under the Python license ."
#   This recipe is covered under the Python license:
#   http://www.python.org/license

import httplib
import mimetypes
import urlparse
import socket
from socket import error, herror, gaierror, timeout
socket.setdefaulttimeout(None)


def link_exists(host, selector, scheme='http'):
    url = "%s://%s%s" % (scheme, host, selector)
    host, path = urlparse.urlsplit(url)[1:3]
    found = 0
    msg = "ping"
    try:
        h = httplib.HTTP(host)  # Make HTTPConnection Object
        h.putrequest('HEAD', selector)
        h.putheader('content-type', "text/plain")
        h.putheader('content-length', str(len(msg)))
        h.putheader('Host', host)
        h.endheaders()
        h.send(msg)

        errcode, errmsg, headers = h.getreply()
        if errcode == 200:
            found = 1
        else:
            print ("WARNING: graph server Status %d %s : %s"
                   % (errcode, errmsg, url))
    except Exception, e:
        print "WARNING: graph server ", e.__class__,  e, url
    return found


def test_links(*urls):
    """ensure urls exist"""
    # should this function return True/False, etc?

    for url in urls:
        url_split = urlparse.urlsplit(url)
        scheme, server, path, _, _ = url_split
        if (scheme in ('http', 'https') and
            not link_exists(server, path, scheme)):  # noqa

            print 'WARNING: graph server link does not exist: %s' % url


def post_multipart(host, selector, fields=(), files=()):
    """
    Post fields and files to an http host as multipart/form-data.
    fields is a sequence of (name, value) elements for regular form fields.
    files is a sequence of (name, filename, value) elements for data to be
    uploaded as files
    Return the server's response page.
    """
    try:
        host = host.replace('http://', '')
        index = host.find('/')
        if index > 0:
            selector = '/'.join([host[index:], selector.lstrip('/')])
            host = host[0:index]

        # Summarized results to the official graph server
        content_type, body = encode_multipart_formdata(fields, files)

        headers = {
            "Content-Type": content_type,
            "Content-length": str(len(body)),
            "Host": host,
            "Accept": "text/plain"
        }
        conn = httplib.HTTPConnection(host)
        conn.request("POST", selector, body, headers)
        response = conn.getresponse()
        return response.read()
    except (httplib.HTTPException, error, herror, gaierror, timeout), e:
        print "WARNING: graph server unreachable"
        print "WARNING: " + str(e)
        raise
    except:
        print "WARNING: graph server unreachable"
        raise


def encode_multipart_formdata(fields, files):
    """
    fields is a sequence of (name, value) elements for regular form fields.
    files is a sequence of (name, filename, value) elements for data to be
    uploaded as files
    Return (content_type, body) ready for httplib.HTTP instance
    """
    BOUNDARY = '----------ThIs_Is_tHe_bouNdaRY_$'
    CRLF = '\r\n'
    L = []
    for (key, value) in fields:
        L.append('--' + BOUNDARY)
        L.append('Content-Disposition: form-data; name="%s"' % key)
        L.append('')
        L.append(value)
    for (key, filename, value) in files:
        L.append('--' + BOUNDARY)
        L.append('Content-Disposition: form-data; name="%s"; filename="%s"'
                 % (key, filename))
        L.append('Content-Type: %s' % get_content_type(filename))
        L.append('')
        L.append(value)
    L.append('--' + BOUNDARY + '--')
    L.append('')
    body = CRLF.join(L)
    content_type = 'multipart/form-data; boundary=%s' % BOUNDARY
    return content_type, body


def get_content_type(filename):
    return mimetypes.guess_type(filename)[0] or 'application/octet-stream'
