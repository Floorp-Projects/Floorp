What is this and what is it for?
--------------------------------

cookies.py is a Python module for working with HTTP cookies: parsing and
rendering 'Cookie:' request headers and 'Set-Cookie:' response headers,
and exposing a convenient API for creating and modifying cookies. It can be
used as a replacement of Python's Cookie.py (aka http.cookies). 

Features
--------

* Rendering according to the excellent new RFC 6265 
  (rather than using a unique ad hoc format inconsistently relating to
  unrealistic, very old RFCs which everyone ignored). Uses URL encoding to
  represent non-ASCII by default, like many other languages' libraries
* Liberal parsing, incorporating many complaints about Cookie.py barfing
  on common cookie formats which can be reliably parsed (e.g. search 'cookie'
  on the Python issue tracker)
* Well-documented code, with chapter and verse from RFCs
  (rather than arbitrary, undocumented decisions and huge tables of magic 
  values, as you see in Cookie.py). 
* Test coverage at 100%, with a much more comprehensive test suite
  than Cookie.py
* Single-source compatible with the following Python versions: 
  2.6, 2.7, 3.2, 3.3 and PyPy (2.7).
* Cleaner, less surprising API::

    # old Cookie.py - this code is all directly from its docstring
    >>> from Cookie import SmartCookie
    >>> C = SmartCookie()
    >>> # n.b. it's "smart" because it automatically pickles Python objects,
    >>> # which is actually quite stupid for security reasons!
    >>> C["rocky"] = "road"
    >>> C["rocky"]["path"] = "/cookie"
    >>> # So C["rocky"] is a string, except when it's a dict...
    >>> # and why do I have to write [""] to access a fixed set of attrs?
    >>> # Look at the atrocious way I render out a request header:
    >>> C.output(attrs=[], header="Cookie:")
    'Cookie: rocky=road'

    # new cookies.py
    >>> from cookies import Cookies, Cookie
    >>> cookies = Cookies(rocky='road')
    >>> # Can also write explicitly: cookies['rocky'] = Cookie['road']
    >>> cookies['rocky'].path = "/cookie" 
    >>> cookies.render_request()
    'rocky=road'
* Friendly to customization, extension, and reuse of its parts. 
  Unlike Cookie.py, it doesn't lock all implementation inside its own classes
  (forcing you to write ugly wrappers as Django, Trac, Werkzeug/Flask, web.py
  and Tornado had to do). You can suppress minor parse exceptions with
  parameters rather than subclass wrappers. You can plug in your own parsers,
  renderers and validators for new or existing cookie attributes. You can
  render the data out in a dict. You can easily use the underlying imperative
  API or even lift the parser's regexps for your own parser or project. They
  are very well documented and relate directly to RFCs, so you know exactly
  what you are getting and why. It's MIT-licensed so do
  what you want (but I'd love to know what use you are getting from it!)
* One file, so you can just drop cookies.py into your project if you like
* MIT license, so you can use it in whatever you want with no strings

Things this is not meant to do
------------------------------
While this is intended to be a good module for handling cookies, it does not
even try to do any of the following:

* Maintain backward compatibility with Cookie.py, which would mean
  inheriting its confusions and bugs
* Implement RFCs 2109 or 2965, which have always been ignored by almost
  everyone and are now obsolete as well
* Handle every conceivable output from terrible legacy apps, which is not
  possible to do without lots of silent data loss and corruption (the
  parser does try to be liberal as possible otherwise, though)
* Provide a means to store pickled Python objects in cookie values
  (that's a big security hole)

This doesn't compete with the cookielib (http.cookiejar) module in the Python
standard library, which is specifically for implementing cookie storage and
similar behavior in an HTTP client such as a browser. Things cookielib does
that this doesn't:

* Write to or read from browsers' cookie stores or other proprietary
  formats for storing cookie data in files 
* Handle the browser/client logic like deciding which cookies to send or
  discard, etc. 

If you are looking for a cookie library but neither this one nor cookielib
will help, you might also consider the implementations in WebOb or Bottle.


