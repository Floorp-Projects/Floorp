#!/usr/bin/python
"""
 svn.py
 Script for BuildBot to monitor a remote Subversion repository.
 Copyright (C) 2006 John Pye
"""
# This script is free software; you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public
# License as published by the Free Software Foundation; either
# version 2.1 of the License, or (at your option) any later version.
#
# This library is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public
# License along with this library; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
# USA

import commands
import xml.dom.minidom
import ConfigParser
import os.path
import codecs

# change these settings to match your project
svnurl = "https://pse.cheme.cmu.edu/svn/ascend/code/trunk"
statefilename = "~/changemonitor/config.ini"
buildmaster = "buildbot.example.org:9989" # connects to a PBChangeSource

xml1 = commands.getoutput("svn log --non-interactive --verbose --xml --limit=1 " + svnurl)
#print "XML\n-----------\n"+xml1+"\n\n"

try:
	doc = xml.dom.minidom.parseString(xml1)
	el = doc.getElementsByTagName("logentry")[0]
	revision = el.getAttribute("revision")
	author = "".join([t.data for t in el.getElementsByTagName("author")[0].childNodes])
	comments = "".join([t.data for t in el.getElementsByTagName("msg")[0].childNodes])

	pathlist = el.getElementsByTagName("paths")[0]
	paths = []
	for p in pathlist.getElementsByTagName("path"):
		paths.append("".join([t.data for t in p.childNodes]))
	#print "PATHS"
	#print paths
except xml.parsers.expat.ExpatError, e:
	print "FAILED TO PARSE 'svn log' XML:"
	print str(e)
	print "----"
	print "RECEIVED TEXT:"
	print xml1
	import sys
	sys.exit(1)

fname = statefilename
fname = os.path.expanduser(fname)
ini = ConfigParser.SafeConfigParser()

try:
	ini.read(fname)
except:
	print "Creating changemonitor config.ini:",fname
	ini.add_section("CurrentRevision")
	ini.set("CurrentRevision",-1)

try:
	lastrevision = ini.get("CurrentRevision","changeset")
except ConfigParser.NoOptionError:
	print "NO OPTION FOUND"
	lastrevision = -1
except ConfigParser.NoSectionError:
	print "NO SECTION FOUND"
	lastrevision = -1

if lastrevision != revision:

	#comments = codecs.encodings.unicode_escape.encode(comments)
	cmd = "buildbot sendchange --master="+buildmaster+" --branch=trunk --revision=\""+revision+"\" --username=\""+author+"\" --comments=\""+comments+"\" "+" ".join(paths)

	#print cmd
	res = commands.getoutput(cmd)

	print "SUBMITTING NEW REVISION",revision
	if not ini.has_section("CurrentRevision"):
		ini.add_section("CurrentRevision")
	try:
		ini.set("CurrentRevision","changeset",revision)
		f = open(fname,"w")
		ini.write(f)
		#print "WROTE CHANGES TO",fname
	except:
		print "FAILED TO RECORD INI FILE"
