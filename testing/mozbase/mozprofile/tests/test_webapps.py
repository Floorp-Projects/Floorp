#!/usr/bin/env python

"""
test installing and managing webapps in a profile
"""

import os
import shutil
import unittest
from tempfile import mkdtemp

from mozprofile.webapps import WebappCollection, Webapp, WebappFormatException

here = os.path.dirname(os.path.abspath(__file__))

class WebappTest(unittest.TestCase):
    """Tests reading, installing and cleaning webapps
    from a profile.
    """
    manifest_path_1 = os.path.join(here, 'files', 'webapps1.json')
    manifest_path_2 = os.path.join(here, 'files', 'webapps2.json')

    def setUp(self):
        self.profile = mkdtemp(prefix='test_webapp')
        self.webapps_dir = os.path.join(self.profile, 'webapps')
        self.webapps_json_path = os.path.join(self.webapps_dir, 'webapps.json')

    def tearDown(self):
        shutil.rmtree(self.profile)

    def test_read_json_manifest(self):
        """Tests WebappCollection.read_json"""
        # Parse a list of webapp objects and verify it worked
        manifest_json_1 = WebappCollection.read_json(self.manifest_path_1)
        self.assertEqual(len(manifest_json_1), 7)
        for app in manifest_json_1:
            self.assertIsInstance(app, Webapp)
            for key in Webapp.required_keys:
                self.assertIn(key, app)

        # Parse a dictionary of webapp objects and verify it worked
        manifest_json_2 = WebappCollection.read_json(self.manifest_path_2)
        self.assertEqual(len(manifest_json_2), 5)
        for app in manifest_json_2:
            self.assertIsInstance(app, Webapp)
            for key in Webapp.required_keys:
                self.assertIn(key, app)

    def test_invalid_webapp(self):
        """Tests a webapp with a missing required key"""
        webapps = WebappCollection(self.profile)
        # Missing the required key "description", exception should be raised
        self.assertRaises(WebappFormatException, webapps.append, { 'name': 'foo' })

    def test_webapp_collection(self):
        """Tests the methods of the WebappCollection object"""
        webapp_1 = { 'name': 'test_app_1',
                     'description': 'a description',
                     'manifestURL': 'http://example.com/1/manifest.webapp',
                     'appStatus': 1 }

        webapp_2 = { 'name': 'test_app_2',
                     'description': 'another description',
                     'manifestURL': 'http://example.com/2/manifest.webapp',
                     'appStatus': 2 }

        webapp_3 = { 'name': 'test_app_2',
                     'description': 'a third description',
                     'manifestURL': 'http://example.com/3/manifest.webapp',
                     'appStatus': 3 }

        webapps = WebappCollection(self.profile)
        self.assertEqual(len(webapps), 0)

        # WebappCollection should behave like a list
        def invalid_index():
            webapps[0]
        self.assertRaises(IndexError, invalid_index)

        # Append a webapp object
        webapps.append(webapp_1)
        self.assertTrue(len(webapps), 1)
        self.assertIsInstance(webapps[0], Webapp)
        self.assertEqual(len(webapps[0]), len(webapp_1))
        self.assertEqual(len(set(webapps[0].items()) & set(webapp_1.items())), len(webapp_1))

        # Remove a webapp object
        webapps.remove(webapp_1)
        self.assertEqual(len(webapps), 0)

        # Extend a list of webapp objects
        webapps.extend([webapp_1, webapp_2])
        self.assertEqual(len(webapps), 2)
        self.assertTrue(webapp_1 in webapps)
        self.assertTrue(webapp_2 in webapps)
        self.assertNotEquals(webapps[0], webapps[1])

        # Insert a webapp object
        webapps.insert(1, webapp_3)
        self.assertEqual(len(webapps), 3)
        self.assertEqual(webapps[1], webapps[2])
        for app in webapps:
            self.assertIsInstance(app, Webapp)

        # Assigning an invalid type (must be accepted by the dict() constructor) should throw
        def invalid_type():
            webapps[2] = 1
        self.assertRaises(WebappFormatException, invalid_type)

    def test_install_webapps(self):
        """Test installing webapps into a profile that has no prior webapps"""
        webapps = WebappCollection(self.profile, apps=self.manifest_path_1)
        self.assertFalse(os.path.exists(self.webapps_dir))

        # update the webapp manifests for the first time
        webapps.update_manifests()
        self.assertFalse(os.path.isdir(os.path.join(self.profile, webapps.backup_dir)))
        self.assertTrue(os.path.isfile(self.webapps_json_path))

        webapps_json = webapps.read_json(self.webapps_json_path, description="fake description")
        self.assertEqual(len(webapps_json), 7)
        for app in webapps_json:
            self.assertIsInstance(app, Webapp)

        manifest_json_1 = webapps.read_json(self.manifest_path_1)
        manifest_json_2 = webapps.read_json(self.manifest_path_2)
        self.assertEqual(len(webapps_json), len(manifest_json_1))
        for app in webapps_json:
            self.assertTrue(app in manifest_json_1)

        # Remove one of the webapps from WebappCollection after it got installed
        removed_app = manifest_json_1[2]
        webapps.remove(removed_app)
        # Add new webapps to the collection
        webapps.extend(manifest_json_2)

        # update the webapp manifests a second time
        webapps.update_manifests()
        self.assertFalse(os.path.isdir(os.path.join(self.profile, webapps.backup_dir)))
        self.assertTrue(os.path.isfile(self.webapps_json_path))

        webapps_json = webapps.read_json(self.webapps_json_path, description="a description")
        self.assertEqual(len(webapps_json), 11)

        # The new apps should be added
        for app in webapps_json:
            self.assertIsInstance(app, Webapp)
            self.assertTrue(os.path.isfile(os.path.join(self.webapps_dir, app['name'], 'manifest.webapp')))
        # The removed app should not exist in the manifest
        self.assertNotIn(removed_app, webapps_json)
        self.assertFalse(os.path.exists(os.path.join(self.webapps_dir, removed_app['name'])))

        # Cleaning should delete the webapps directory entirely since there was nothing there before
        webapps.clean()
        self.assertFalse(os.path.isdir(self.webapps_dir))

    def test_install_webapps_preexisting(self):
        """Tests installing webapps when the webapps directory already exists"""
        manifest_json_2 = WebappCollection.read_json(self.manifest_path_2)

        # Synthesize a pre-existing webapps directory
        os.mkdir(self.webapps_dir)
        shutil.copyfile(self.manifest_path_2, self.webapps_json_path)
        for app in manifest_json_2:
            app_path = os.path.join(self.webapps_dir, app['name'])
            os.mkdir(app_path)
            f = open(os.path.join(app_path, 'manifest.webapp'), 'w')
            f.close()

        webapps = WebappCollection(self.profile, apps=self.manifest_path_1)
        self.assertTrue(os.path.exists(self.webapps_dir))

        # update webapp manifests for the first time
        webapps.update_manifests()
        # A backup should be created
        self.assertTrue(os.path.isdir(os.path.join(self.profile, webapps.backup_dir)))

        # Both manifests should remain installed
        webapps_json = webapps.read_json(self.webapps_json_path, description='a fake description')
        self.assertEqual(len(webapps_json), 12)
        for app in webapps_json:
            self.assertIsInstance(app, Webapp)
            self.assertTrue(os.path.isfile(os.path.join(self.webapps_dir, app['name'], 'manifest.webapp')))

        # Upon cleaning the backup should be restored
        webapps.clean()
        self.assertFalse(os.path.isdir(os.path.join(self.profile, webapps.backup_dir)))

        # The original webapps should still be installed
        webapps_json = webapps.read_json(self.webapps_json_path)
        for app in webapps_json:
            self.assertIsInstance(app, Webapp)
            self.assertTrue(os.path.isfile(os.path.join(self.webapps_dir, app['name'], 'manifest.webapp')))
        self.assertEqual(webapps_json, manifest_json_2)

if __name__ == '__main__':
    unittest.main()
