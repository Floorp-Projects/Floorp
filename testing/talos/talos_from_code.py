#! /usr/bin/env python
from optparse import OptionParser
try:
    import json
except:
    import simplejson as json
import re
import urllib2
import urlparse
import sys

def main():
    parser = OptionParser()
    parser.add_option("--talos-json-url", dest="talos_json_url", type="string",
                      help="It indicates from where to download the talos.json file.")
    (options, args) = parser.parse_args()

    if options.talos_json_url == None:
        print "You need to specify --talos-json-url."
        sys.exit(1)

    # json file with info on which talos.zip to use
    # the json file URL should look like this:
    #  %(repo_path)s/raw-file/%(revision)s/testing/talos/talos.json
    try:
        jsonFilename = download_file(options.talos_json_url)
    except Exception as e:
        print "ERROR: We have been unable to download the talos.zip indicated " + \
              "in the talos.json file."
        print "ERROR: %s" % str(e)
        sys.exit(1)

    print "INFO: talos.json URL:  %s" % options.talos_json_url
    talos_zip_url = get_value(jsonFilename, "talos_zip")
    print "INFO: talos.zip URL: '%s'" % talos_zip_url
    try:
        if passesRestrictions(options.talos_json_url, talos_zip_url) == False:
            print "ERROR: You have tried to download a talos.zip from a location " + \
                  "different than http://build.mozilla.org/talos/zips"
            print "ERROR: This is only allowed for the 'try' branch."
            sys.exit(1)
        download_file(talos_zip_url, "talos.zip")
    except Exception as e:
        print "ERROR: We have been unable to download the talos.zip indicated " + \
              "in the talos.json file."
        print "ERROR: %s" % str(e)
        sys.exit(1)

def passesRestrictions(talosJsonUrl, fileUrl):
    '''
    Only certain branches are exempted from having to host their downloadable files
    in build.mozilla.org
    '''
    if talosJsonUrl.startswith("http://hg.mozilla.org/try/") == True or \
       talosJsonUrl.startswith("http://hg.mozilla.org/projects/pine/") == True:
        return True
    else:
        p = re.compile('^http://build.mozilla.org/talos/')
        m = p.match(fileUrl)
        if m == None:
            return False
        return True

def get_filename_from_url(url):
    '''
    This returns the filename of the file we're trying to download
    '''
    parsed = urlparse.urlsplit(url.rstrip('/'))
    if parsed.path != '':
        return parsed.path.rsplit('/', 1)[-1]
    else:
        print "ERROR: We were trying to download a file from %s " + \
              "but the URL seems to be incorrect."
        sys.exit(1)

def download_file(url, saveAs=None):
    '''
    It downloads a file from the URL indicated and can be saved locally with
    a different name if needed.
    '''
    req = urllib2.Request(url)
    filename = get_filename_from_url(url)
    f = urllib2.urlopen(req)
    local_file = open(saveAs if saveAs else filename, 'wb')
    local_file.write(f.read())
    local_file.close()
    return filename

def get_value(json_filename, key):
    '''
    It loads up a JSON file and returns the value for the given string
    '''
    f = open(json_filename, 'r')
    return json.load(f)[key]

if __name__ == '__main__':
    main()
