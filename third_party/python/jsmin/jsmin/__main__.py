import sys, os, glob
from jsmin import JavascriptMinify

for f in sys.argv[1:]:
    with open(f, 'r') as js:
        minifier = JavascriptMinify(js, sys.stdout)
        minifier.minify()
    sys.stdout.write('\n')
    
    
