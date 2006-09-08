"""
Python translation of fit..
which is copyright (c) 2002 Cunningham & Cunningham, Inc.
Released under the terms of the GNU General Public License version 2 or later.
"""

import urllib2
import feedparser # http://feedparser.org/
from fit.ColumnFixture import ColumnFixture

auth_handler = urllib2.HTTPBasicAuthHandler()
auth_handler.add_password('khan!', 'khan.mozilla.org', 'morgamic', 'carebears')
opener = urllib2.build_opener(auth_handler)
urllib2.install_opener(opener)

class Verify(ColumnFixture):

    _typeDict={"host": "String",
               "product": "String",
               "version": "String",
               "build": "String",
               "platform": "String",
               "locale": "String",
               "channel": "String",
               "complete": "String",
               "partial": "String",
               "updateType": "String",
               "osVersion": "String",
               "lastURI": "String",
               "newURI": "String",
               "hasUpdate": "Boolean",
               "hasComplete": "Boolean",
               "hasPartial":  "Boolean",
               "isValidXml": "Boolean",
               "isMinorUpdate": "Boolean",
               "isMajorUpdate": "Boolean"}

    def __init__(self):
        self.osVersion = "osVersion"
        self.lastURI = ""
        self.lastXML = ""

    # Checks if an update element exists.
    def hasUpdate(self):
        return ('</update>' in self.getXml())

    # Checks if the expected complete patch exists.
    def hasComplete(self):
        return (self.complete in self.getXml())

    # Check if the expected partial patch exists.
    def hasPartial(self):
        return (self.partial in self.getXml())

    # Check if the update type is "minor".
    def isMinorUpdate(self):
        return ("type=\"minor\"" in self.getXml())

    # Check if the update type is "major".
    def isMajorUpdate(self):
        return ("type=\"major\"" in self.getXml())

    # Check if the AUS XML document is well-formed.
    def isValidXml(self):
        return (feedparser.parse(self.getXml()).bozo==0)

    # Gets and returns an AUS XML document.
    def getXml(self):
        newURI = self.buildUri();

        print newURI

        if (self.lastURI == newURI):
            return self.lastXML
        
        newXML = urllib2.urlopen(newURI).read()

        self.lastURI = newURI
        self.lastXML = newXML

        return newXML

    # Builds an AUS URI based on our test data.
    def buildUri(self):
        return self.host + "/" +        \
               self.product + "/" +     \
               self.version + "/" +     \
               self.build + "/" +       \
               self.platform + "/" +    \
               self.locale + "/" +      \
               self.channel + "/" +     \
               self.osVersion + "/update.xml"
