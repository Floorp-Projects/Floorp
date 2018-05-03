To use the view_gecko_profile tool:

cd testing/tools/view_gecko_profile

virtualenv venv

source venv/bin/activate

pip install -r requirements.txt

Then the command line:

python view_gecko_profile.py -b <path to browser binary> -p <path to gecko_profile.zip>

i.e. For Firefox:

python view_gecko_profile.py -b "/Users/rwood/mozilla-unified/obj-x86_64-apple-darwin17.4.0/dist/Nightly.app/Contents/MacOS/firefox" -p /Users/rwood/mozilla-unified/testing/mozharness/build/blobber_upload_dir/profile_damp.zip

i.e. For Chrome:

python view_gecko_profile.py -b "/Applications/Google Chrome.app/Contents/MacOS/Google Chrome" -p /Users/rwood/mozilla-unified/testing/mozharness/build/blobber_upload_dir/profile_damp.zip
