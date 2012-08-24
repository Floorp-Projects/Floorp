#!/usr/bin/python

# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import sys, subprocess, json, argparse

SOURCE = "https://src.chromium.org/viewvc/chrome/trunk/src/net/base/transport_security_state_static.json"
OUTPUT = "nsSTSPreloadList.inc"
PREFIX = """/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*****************************************************************************/
/* This is an automatically generated file. If you're not                    */
/* nsStrictTransportSecurityService.cpp, you shouldn't be #including it.     */
/*****************************************************************************/

#include <prtypes.h>

class nsSTSPreload
{
  public:
    const char *mHost;
    const bool mIncludeSubdomains;
};

static const nsSTSPreload kSTSPreloadList[] = {
"""
POSTFIX = """};
"""

def filterComments(stream):
  lines = []
  for line in stream:
    # each line still has '\n' at the end, so if find returns -1,
    # the newline gets chopped off like we want
    # (and otherwise, comments are filtered out like we want)
    lines.append(line[0:line.find("//")])
  return "".join(lines)

def readFile(source):
  if source != "-":
    f = open(source, 'r')
  else:
    f = sys.stdin
  return filterComments(f)

def download(source):
  download = subprocess.Popen(["wget", "-O", "-", source], stdout = subprocess.PIPE, stderr = subprocess.PIPE)
  contents = filterComments(download.stdout)
  download.wait()
  if download.returncode != 0:
    raise Exception()
  return contents

def output(filename, jsonblob):
  if filename != "-":
    outstream = open(filename, 'w')
  else:
    outstream = sys.stdout
  if not 'entries' in jsonblob:
    raise Exception()
  else:
    outstream.write(PREFIX)
    # use a dictionary to prevent duplicates
    lines = {}
    for entry in jsonblob['entries']:
      if 'name' in entry and 'mode' in entry and entry['mode'] == "force-https":
        line = "  { \"" + entry['name'] + "\", "
        if 'include_subdomains' in entry and entry['include_subdomains']:
          line = line + "true },\n"
        else:
          line = line + "false },\n"
        lines[line] = True
    # The data must be sorted by domain name because we do a binary search to
    # determine if a host is in the preload list.
    keys = lines.keys()
    keys.sort()
    for line in keys:
      outstream.write(line)
    outstream.write(POSTFIX);
  outstream.close()

def main():
  parser = argparse.ArgumentParser(description="Download Chrome's STS preload list and format it for Firefox")
  parser.add_argument("-s", "--source", default=SOURCE, help="Specify source for input list (can be a file, url, or '-' for stdin)")
  parser.add_argument("-o", "--output", default=OUTPUT, help="Specify output file ('-' for stdout)")
  args = parser.parse_args()
  contents = None
  try:
    contents = readFile(args.source)
  except:
    pass
  if not contents:
    try:
      contents = download(args.source)
    except:
      print >> sys.stderr, "Could not read source '%s'" % args.source
      return 1
  try:
    jsonblob = json.loads(contents)
  except:
    print >> sys.stderr, "Could not parse contents of file '%s'" % args.source
    return 1
  try:
    output(args.output, jsonblob)
  except:
    print >> sys.stderr, "Could not write to '%s'" % args.output
    return 1
  return 0

if __name__ == "__main__":
  sys.exit(main())
