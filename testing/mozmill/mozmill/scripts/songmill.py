import os, sys

import jsbridge
import mozmill
from jsbridge import global_settings
from mozrunner import runner

this_dir = os.path.abspath(os.path.dirname(__file__))

def cli():
    global_settings.MOZILLA_BINARY = '/Applications/Songbird.app/Contents/MacOS/songbird'
    global_settings.MOZILLA_DEFAULT_PROFILE = '/Applications/Songbird.app/Contents/Resources/defaults/profile'
    runner.Firefox.name = 'songbird'
    mozmill.main()
    
if __name__ == "__main__":
    cli()
