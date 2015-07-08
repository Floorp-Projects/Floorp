#!/usr/bin/env python
# Stolen from taskcluster-vcs
# https://github.com/taskcluster/taskcluster-vcs/blob/master/src/vcs/detect_remote.js

from urllib2 import Request, urlopen
from urlparse import urlsplit, urlunsplit
from os.path import exists, join

def first(seq):
    return next(iter(filter(lambda x: x, seq)), '')

def all_first(*sequences):
    return map(lambda x: first(x), sequences)

# http://codereview.stackexchange.com/questions/13027/joining-url-path-components-intelligently
# I wonder why this is not a builtin feature in Python
def urljoin(*parts):
    schemes, netlocs, paths, queries, fragments = zip(*(urlsplit(part) for part in parts))
    scheme, netloc, query, fragment = all_first(schemes, netlocs, queries, fragments)
    path = '/'.join(p.strip('/') for p in paths if p)
    return urlunsplit((scheme, netloc, path, query, fragment))

def _detect_remote(url, content):
    try:
        response = urlopen(url)
    except Exception:
        return False

    if response.getcode() != 200:
        return False

    content_type = response.headers.get('content-type', '')
    return True if content in content_type else False

def detect_git(url):
    location = urljoin(url, '/info/refs?service=git-upload-pack')
    req = Request(location, headers={'User-Agent':'git/2.0.1'})
    return _detect_remote(req, 'x-git')

def detect_hg(url):
    location = urljoin(url, '?cmd=lookup&key=0')
    return _detect_remote(location, 'mercurial')

def detect_local(url):
    if exists(join(url, '.git')):
        return 'git'

    if exists(join(url, '.hg')):
        return 'hg'

    return ''

