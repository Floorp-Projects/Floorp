# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

""" deki.py - Access the wiki pages on a MindTouch Deki server via the API.

Here's what this code can do:

  wiki = deki.Deki("http://developer.mozilla.org/@api/deki/", username, password)
  page = wiki.get_page("Sheep")
  print page.title
  print page.doc.toxml()

  page.title = "Bananas"
  page.save()

There are also some additional methods:
  wiki.create_page(path, content, title=, override=)
  wiki.move_page(old, new)
  wiki.get_subpages(page)

This module does not try to mimic the MindTouch "Plug" API.  It's meant to be
higher-level than that.
"""

import sys
import urllib2, cookielib, httplib
import xml.dom.minidom as dom
from urllib import quote as _urllib_quote
from urllib import urlencode as _urlencode
import urlparse
from datetime import datetime
import re

__all__ = ['Deki']


# === Utils

def _check(fact):
    if not fact:
        raise AssertionError('check failed')

def _urlquote(s, *args):
    return _urllib_quote(s.encode('utf-8'), *args)

def _make_url(*dirs, **params):
    """ dirs must already be url-encoded, params must not """
    url = '/'.join(dirs)
    if params:
        url += '?' + _urlencode(params)
    return url

class PutRequest(urllib2.Request):
    def get_method(self):
        return "PUT"

# === Dream framework client code

# This handler causes python to "always be logged in" when it's talking to the
# server.  If you're just accessing public pages, it generates more requests
# than are strictly needed, but this is the behavior you want for a bot.
#
# The users/authenticate request is sent twice: once without any basic auth and
# once with.  Dumb.  Feel free to fix.
#
class _LoginHandler(urllib2.HTTPCookieProcessor):
    def __init__(self, server):
        policy = cookielib.DefaultCookiePolicy(rfc2965=True)
        cookiejar = cookielib.CookieJar(policy)
        urllib2.HTTPCookieProcessor.__init__(self, cookiejar)
        self.server = server

    def http_request(self, req):
        #print "DEBUG- Requesting " + req.get_full_url()
        s = self.server
        req = urllib2.HTTPCookieProcessor.http_request(self, req)
        if ('Cookie' not in req.unredirected_hdrs
              and req.get_full_url() != s.base + 'users/authenticate'):
            s.login()
            # Retry - should have a new cookie.
            req = urllib2.HTTPCookieProcessor.http_request(self, req)
            _check('Cookie' in req.unredirected_hdrs)
        return req

class DreamClient:
    def __init__(self, base, user, password):
        """ 
        base - The base URI of the Deki API, with trailing slash.
               Typically, 'http://wiki.example.org/@api/deki/'.
        user, password - Your Deki login information.
        """
        self.base = base
        pm = urllib2.HTTPPasswordMgrWithDefaultRealm()
        pm.add_password(None, self.base, user, password)
        ah = urllib2.HTTPBasicAuthHandler(pm)
        lh = _LoginHandler(self)
        self._opener = urllib2.build_opener(ah, lh)

    def login(self):
        response = self._opener.open(self.base + 'users/authenticate')
        response.close()

    def open(self, url):
        return self._opener.open(self.base + url)

    def _handleResponse(self, req):
        """Helper method shared between post() and put()"""
        resp = self._opener.open(req)
        try:
            ct = resp.headers.get('Content-Type', '(none)')
            if '/xml' in ct or '+xml' in ct:
                return dom.parse(resp)
            else:
                #print "DEBUG- Content-Type:", ct
                crud = resp.read()
                #print 'DEBUG- crud:\n---\n%s\n---' % re.sub(r'(?m)^', '    ', crud)
                return None
        finally:
            resp.close()


    def post(self, url, data, type):
        #print "DEBUG- posting to:", self.base + url
        req = urllib2.Request(self.base + url, data, {'Content-Type': type})
        return self._handleResponse(req)

    def put(self, url, data, type):
        #print "DEBUG- putting to:", self.base + url
        req = PutRequest(self.base + url, data, {'Content-Type': type})
        return self._handleResponse(req)

    def get_xml(self, url):
        resp = self.open(url)
        try:
            return dom.parse(resp)
        finally:
            resp.close()


# === DOM

def _text_of(node):
    if node.nodeType == node.ELEMENT_NODE:
        return u''.join(_text_of(n) for n in node.childNodes)
    elif node.nodeType == node.TEXT_NODE:
        return node.nodeValue
    else:
        return u''

def _the_element_by_name(doc, tagName):
    elts = doc.getElementsByTagName(tagName)
    if len(elts) != 1:
        raise ValueError("Expected exactly one <%s> tag, got %d." % (tagName, len(elts)))
    return elts[0]

def _first_element(node):
    n = node.firstChild
    while n is not None:
        if n.nodeType == n.ELEMENT_NODE:
            return n
        n = node.nextSibling
    return None

def _find_elements(node, path):
    if u'/' in path:
        [first, rest] = path.split(u'/', 1)
        for child in _find_elements(node, first):
            for desc in _find_elements(child, rest):
                yield desc
    else:
        for n in node.childNodes:
            if n.nodeType == node.ELEMENT_NODE and n.nodeName == path:
                yield n


# === Deki

def _format_page_id(id):
    if isinstance(id, int):
        return str(id)
    elif id is Deki.HOME:
        return 'home'
    elif isinstance(id, basestring):
        # Double-encoded, per the Deki API reference.
        return '=' + _urlquote(_urlquote(id, ''))

class Deki(DreamClient):
    HOME = object()

    def get_page(self, page_id):
        """ Get the content of a page from the wiki.

        The page_id argument must be one of:
          an int - The page id (an arbitrary number assigned by Deki)
          a str - The page name (not the title, the full path that shows up in the URL)
          Deki.HOME - Refers to the main page of the wiki.

        Returns a Page object.
        """
        p = Page(self)
        p._load(page_id)
        return p

    def create_page(self, path, content, title=None, overwrite=False):
        """ Create a new wiki page.

        Parameters:
          path - str - The page id.
          content - str - The XML content to put in the new page.
            The document element must be a <body>.
          title - str - The page title.  Keyword argument only.
            Defaults to the last path-segment of path.
          overwrite - bool - Whether to overwrite an existing page. If false,
            and the page already exists, the method will throw an error.
        """
        if title is None:
            title = path.split('/')[-1]
        doc = dom.parseString(content)
        _check(doc.documentElement.tagName == 'body')
        p = Page(self)
        p._create(path, title, doc, overwrite)

    def attach_file(self, page, name, data, mimetype, description=None):
        """Create or update a file attachment.

        Parameters:
          page - str - the page ID this file is related to
          name - str - the name of the file
          data - str - the file data
          mimetype - str - the MIME type of the file
          description - str - a description of the file
        """

        p = {}
        if description is not None:
            p['description'] = description

        url = _make_url('pages', _format_page_id(page),
                        'files', _format_page_id(name), **p)

        r = self.put(url, data, mimetype)
        _check(r.documentElement.nodeName == u'file')

    def get_subpages(self, page_id):
        """ Return the ids of all subpages of the given page. """
        doc = self.get_xml(_make_url("pages", _format_page_id(page_id),
                                     "files,subpages"))
        for elt in _find_elements(doc, u'page/subpages/page.subpage/path'):
            yield _text_of(elt)

    def move_page(self, page_id, new_title, redirects=True):
        """ Move an existing page to a new location.

        A page cannot be moved to a destination that already exists, is a
        descendant, or has a protected title (ex.  Special:xxx, User:,
        Template:).

        When a page is moved, subpages under the specified page are also moved.
        For each moved page, the system automatically creates an alias page
        that redirects from the old to the new destination.
        """
        self.post(_make_url("pages", _format_page_id(page_id), "move",
                            to=new_title,
                            redirects=redirects and "1" or "0"),
                  "", "text/plain")

class Page:
    """ A Deki wiki page.

    To obtain a page, call wiki.get_page(id).
    Attributes:
        title : unicode - The page title.
        doc : Document - The content of the page as a DOM Document.
          The root element of this document is a <body>.
        path : unicode - The path.  Use this to detect redirects, as otherwise
          page.save() will overwrite the redirect with a copy of the content!
        deki : Deki - The Deki object from which the page was loaded.
        page_id : str/id/Deki.HOME - The page id used to load the page.
        load_time : datetime - The time the page was loaded,
          according to the clock on the client machine.
    Methods:
        save() - Save the modified document back to the server.
          Only the page.title and the contents of page.doc are saved.
    """

    def __init__(self, deki):
        self.deki = deki

    def _create(self, path, title, doc, overwrite):
        self.title = title
        self.doc = doc
        self.page_id = path
        if overwrite:
            self.load_time = datetime(2500, 1, 1)
        else:
            self.load_time = datetime(1900, 1, 1)
        self.path = path
        self.save()

    def _load(self, page_id):
        """ page_id - See comment near the definition of `HOME`. """
        load_time = datetime.utcnow()

        # Getting the title is a whole separate query!
        url = 'pages/%s/info' % _format_page_id(page_id)
        doc = self.deki.get_xml(url)
        title = _text_of(_the_element_by_name(doc, 'title'))
        path = _text_of(_the_element_by_name(doc, 'path'))

        # If you prefer to sling regexes, you can request format=raw instead.
        # The result is an XML document with one big fat text node in the body.
        url = _make_url('pages', _format_page_id(page_id), 'contents',
                        format='xhtml', mode='edit')
        doc = self.deki.get_xml(url)

        content = doc.documentElement
        _check(content.tagName == u'content')
        body = _first_element(content)
        _check(body is not None)
        _check(body.tagName == u'body')

        doc.removeChild(content)
        doc.appendChild(body)

        self.page_id = page_id
        self.load_time = load_time
        self.title = title
        self.path = path
        self.doc = doc

    def save(self):
        p = {'edittime': _urlquote(self.load_time.strftime('%Y%m%d%H%M%S')),
             'abort': 'modified'}

        if self.title is not None:
            p['title'] = _urlquote(self.title)

        url = _make_url('pages', _format_page_id(self.page_id), 'contents', **p)

        body = self.doc.documentElement
        bodyInnerXML = ''.join(n.toxml('utf-8') for n in body.childNodes)

        reply = self.deki.post(url, bodyInnerXML, 'text/plain; charset=utf-8')
        _check(reply.documentElement.nodeName == u'edit')
        _check(reply.documentElement.getAttribute(u'status') == u'success')
