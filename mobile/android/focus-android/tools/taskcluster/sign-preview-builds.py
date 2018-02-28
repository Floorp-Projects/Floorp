# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

"""
This script talks to the taskcluster secrets service to obtain a
key store for signing test builds. This key store does NOT contain
the official release key for release builds that are published on
Google Play. Builds signed with a key from this store are for
testing purposes and should not be published on any official
channels like app stores.
"""

import base64
import glob
import os
import subprocess
import taskcluster

def write_secret_to_file(path, data, key, base64decode=False):
	path = os.path.join(os.path.dirname(__file__), '../../' + path)
	with open(path, 'w') as file:
		value = data['secret'][key]
		if (base64decode):
			value = base64.b64decode(value)
		file.write(value)


# Get JSON data from taskcluster secrets service
secrets = taskcluster.Secrets({'baseUrl': 'http://taskcluster/secrets/v1'})
data = secrets.get('project/focus/preview-key-store')

# Write keystore file to locale file system
write_secret_to_file('preview-key-store.jks', data, key='keyStoreFile', base64decode=True)

# Write passwords to local file system
write_secret_to_file('keystore_password', data, key='keyStorePassword')
write_secret_to_file('key_password', data, key='keyPassword')

build_output_path = os.path.join(os.path.dirname(__file__), '../../app/build/outputs/apk')

# Run zipalign
for apk in glob.glob(build_output_path + "/*/*/*-unsigned.apk"):
	split = os.path.splitext(apk)
	print subprocess.check_output(["zipalign", "-f", "-v", "-p", "4", apk, split[0] + "-aligned" + split[1]])

# Sign APKs
for apk in glob.glob(build_output_path + "/*/*/*-aligned.apk"):
	##	"for apk in %s/*-aligned.apk;do     -v \"$apk\";done" % (build_output_path),
	print "Signing", apk
	print subprocess.check_output([
		"apksigner", "sign",
		"--ks", "preview-key-store.jks",
		"--ks-key-alias", "preview-key",
		"--ks-pass", "file:keystore_password",
		"--key-pass", "file:key_password",
		"-v",
		"--out", apk.replace('unsigned', 'signed'),
		apk])

# Create folder for saving build artifacts
artifacts_path = os.path.join(os.path.dirname(__file__), '../../builds')
if not os.path.exists(artifacts_path):
    os.makedirs(artifacts_path)

# Verify signature and move APK to artifact path
for apk in glob.glob(build_output_path + "/*/*/*-signed-*.apk"):
	print "Verifying", apk
	print subprocess.check_output(['apksigner', 'verify', apk])

	print "Archiving", apk
	os.rename(apk, artifacts_path + "/" + os.path.basename(apk))

print("Zipaligned and signed APKs")
