#!/usr/bin/python

import sys
import os
import re
sys.path.append('/home/al/sites')
os.environ['DJANGO_SETTINGS_MODULE'] = '__main__'

DEFAULT_CHARSET = "utf-8"
TEMPLATE_DEBUG = False
LANGUAGE_CODE = "en"

INSTALLED_APPS = (
    'django.contrib.markup',
)    

TEMPLATE_DIRS = (
    '/home/al/sites/liquidx/templates',
    '.'
)

from django.template import Template, Context, loader

def make(src, dst):
    print '%s -> %s' % (src, dst)
    c = Context({})
    filled = loader.render_to_string(src, {})
    open(dst, 'w').write(filled)
    
if __name__ == "__main__":
    for dirname, dirs, files in os.walk('.'):
        if re.search('/\.svn', dirname):
            continue
        for f in files:
            if f[-4:] == ".txt":
                newname = f.replace('.txt', '.html')
                make(os.path.join(dirname, f), os.path.join(dirname, newname))
