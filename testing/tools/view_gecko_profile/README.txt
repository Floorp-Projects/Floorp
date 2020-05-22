To use the view_gecko_profile tool:

cd testing/tools/view_gecko_profile

virtualenv venv

source venv/bin/activate

pip install -r requirements.txt

Then the command line:

python view_gecko_profile.py -p <path to gecko_profile.zip>

i.e.:

python view_gecko_profile.py -p /Users/rwood/mozilla-unified/testing/mozharness/build/blobber_upload_dir/profile_damp.zip
