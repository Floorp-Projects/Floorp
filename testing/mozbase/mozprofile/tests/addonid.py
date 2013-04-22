#!/usr/bin/env python

import os
import tempfile
import unittest
import shutil
from mozprofile import addons


here = os.path.dirname(os.path.abspath(__file__))


class AddonIDTest(unittest.TestCase):
    """ Test finding the addon id in a variety of install.rdf styles """

    def make_install_rdf(self, filecontents):
        path = tempfile.mkdtemp()
        f = open(os.path.join(path, "install.rdf"), "w")
        f.write(filecontents)
        f.close()
        return path

    def test_addonID(self):
        testlist = self.get_test_list()
        for t in testlist:
            try:
                p = self.make_install_rdf(t)
                a = addons.AddonManager(os.path.join(p, "profile"))
                addon_id = a.addon_details(p)['id']
                self.assertEqual(addon_id, "winning", "We got the addon id")
            finally:
                shutil.rmtree(p)

    def test_addonID_xpi(self):
        a = addons.AddonManager("profile")
        addon = a.addon_details(os.path.join(here, "addons", "empty.xpi"))
        self.assertEqual(addon['id'], "test-empty@quality.mozilla.org", "We got the addon id")

    def get_test_list(self):
        """ This just returns a hardcoded list of install.rdf snippets for testing.
            When adding snippets for testing, remember that the id we're looking for
            is "winning" (no quotes).  So, make sure you have that id in your snippet
            if you want it to pass.
        """
        tests = [
"""<?xml version="1.0"?>
<RDF xmlns="http://www.w3.org/1999/02/22-rdf-syntax-ns#"
     xmlns:em="http://www.mozilla.org/2004/em-rdf#">
   <Description about="urn:mozilla:install-manifest">
     <em:id>winning</em:id>
     <em:name>MozMill</em:name>
     <em:version>2.0a</em:version>
     <em:creator>Adam Christian</em:creator>
     <em:description>A testing extension based on the Windmill Testing Framework client source</em:description>
     <em:unpack>true</em:unpack>
     <em:targetApplication>
       <!-- Firefox -->
       <Description>
         <em:id>{ec8030f7-c20a-464f-9b0e-13a3a9e97384}</em:id>
         <em:minVersion>3.5</em:minVersion>
         <em:maxVersion>8.*</em:maxVersion>
       </Description>
     </em:targetApplication>
     <em:targetApplication>
       <!-- Thunderbird -->
       <Description>
         <em:id>{3550f703-e582-4d05-9a08-453d09bdfdc6}</em:id>
         <em:minVersion>3.0a1pre</em:minVersion>
         <em:maxVersion>3.2*</em:maxVersion>
       </Description>
     </em:targetApplication>
     <em:targetApplication>
       <!-- Sunbird -->
       <Description>
         <em:id>{718e30fb-e89b-41dd-9da7-e25a45638b28}</em:id>
         <em:minVersion>0.6a1</em:minVersion>
         <em:maxVersion>1.0pre</em:maxVersion>
       </Description>
     </em:targetApplication>
     <em:targetApplication>
       <!-- SeaMonkey -->
       <Description>
         <em:id>{92650c4d-4b8e-4d2a-b7eb-24ecf4f6b63a}</em:id>
         <em:minVersion>2.0a1</em:minVersion>
         <em:maxVersion>2.1*</em:maxVersion>
       </Description>
     </em:targetApplication>
    <em:targetApplication>
       <!-- Songbird -->
       <Description>
         <em:id>songbird@songbirdnest.com</em:id>
         <em:minVersion>0.3pre</em:minVersion>
         <em:maxVersion>1.3.0a</em:maxVersion>
       </Description>
    </em:targetApplication>
    <em:targetApplication>
         <Description>
          <em:id>toolkit@mozilla.org</em:id>
          <em:minVersion>1.9.1</em:minVersion>
          <em:maxVersion>2.0*</em:maxVersion>
         </Description>
     </em:targetApplication>
   </Description>
</RDF>""",
"""<RDF xmlns="http://www.w3.org/1999/02/22-rdf-syntax-ns#"
     xmlns:em="http://www.mozilla.org/2004/em-rdf#">
   <Description about="urn:mozilla:install-manifest">
     <em:targetApplication>
       <!-- Firefox -->
       <Description>
         <em:id>{ec8030f7-c20a-464f-9b0e-13a3a9e97384}</em:id>
         <em:minVersion>3.5</em:minVersion>
         <em:maxVersion>8.*</em:maxVersion>
       </Description>
     </em:targetApplication>
     <em:id>winning</em:id>
     <em:name>MozMill</em:name>
     <em:version>2.0a</em:version>
     <em:creator>Adam Christian</em:creator>
     <em:description>A testing extension based on the Windmill Testing Framework client source</em:description>
     <em:unpack>true</em:unpack>
    </Description>
 </RDF>""",
"""<RDF xmlns="http://www.mozilla.org/2004/em-rdf#"
        xmlns:rdf="http://www.w3.org/1999/02/22-rdf-syntax-ns#">
   <rdf:Description about="urn:mozilla:install-manifest">
     <id>winning</id>
     <name>foo</name>
     <version>42</version>
     <description>A testing extension based on the Windmill Testing Framework client source</description>
 </rdf:Description>
</RDF>""",
"""<RDF xmlns="http://www.w3.org/1999/02/22-rdf-syntax-ns#"
     xmlns:foobar="http://www.mozilla.org/2004/em-rdf#">
   <Description about="urn:mozilla:install-manifest">
     <foobar:targetApplication>
       <!-- Firefox -->
       <Description>
         <foobar:id>{ec8030f7-c20a-464f-9b0e-13a3a9e97384}</foobar:id>
         <foobar:minVersion>3.5</foobar:minVersion>
         <foobar:maxVersion>8.*</foobar:maxVersion>
       </Description>
     </foobar:targetApplication>
     <foobar:id>winning</foobar:id>
     <foobar:name>MozMill</foobar:name>
     <foobar:version>2.0a</foobar:version>
     <foobar:creator>Adam Christian</foobar:creator>
     <foobar:description>A testing extension based on the Windmill Testing Framework client source</foobar:description>
     <foobar:unpack>true</foobar:unpack>
    </Description>
 </RDF>"""]
        return tests

if __name__ == '__main__':
    unittest.main()
