#!/usr/bin/env python -u
"""action_config_script.py

Demonstrate actions and config.
"""

import os
import sys
import time

sys.path.insert(1, os.path.dirname(sys.path[0]))

from mozharness.base.script import BaseScript


# ActionsConfigExample {{{1
class ActionsConfigExample(BaseScript):
    config_options = [[
        ['--beverage', ],
        {"action": "store",
         "dest": "beverage",
         "type": "string",
         "help": "Specify your beverage of choice",
         }
    ], [
        ['--ship-style', ],
        {"action": "store",
         "dest": "ship_style",
         "type": "choice",
         "choices": ["1", "2", "3"],
         "help": "Specify the type of ship",
         }
    ], [
        ['--long-sleep-time', ],
        {"action": "store",
         "dest": "long_sleep_time",
         "type": "int",
         "help": "Specify how long to sleep",
         }
    ]]

    def __init__(self, require_config_file=False):
        super(ActionsConfigExample, self).__init__(
            config_options=self.config_options,
            all_actions=[
                'clobber',
                'nap',
                'ship-it',
            ],
            default_actions=[
                'clobber',
                'nap',
                'ship-it',
            ],
            require_config_file=require_config_file,
            config={
                'beverage': "kool-aid",
                'long_sleep_time': 3600,
                'ship_style': "1",
            }
        )

    def _sleep(self, sleep_length, interval=5):
        self.info("Sleeping %d seconds..." % sleep_length)
        counter = 0
        while counter + interval <= sleep_length:
            sys.stdout.write(".")
            try:
                time.sleep(interval)
            except:
                print
                self.error("Impatient, are we?")
                sys.exit(1)
            counter += interval
        print
        self.info("Ok, done.")

    def _ship1(self):
        self.info("""
     _~
  _~ )_)_~
  )_))_))_)
  _!__!__!_
  \______t/
~~~~~~~~~~~~~
""")

    def _ship2(self):
        self.info("""
    _4 _4
   _)_))_)
  _)_)_)_)
 _)_))_))_)_
 \_=__=__=_/
~~~~~~~~~~~~~
""")

    def _ship3(self):
        self.info("""
 ,;;:;,
   ;;;;;
  ,:;;:;    ,'=.
  ;:;:;' .=" ,'_\\
  ':;:;,/  ,__:=@
   ';;:;  =./)_
     `"=\\_  )_"`
          ``'"`
""")

    def nap(self):
        for var_name in self.config.keys():
            if var_name.startswith("random_config_key"):
                self.info("This is going to be %s!" % self.config[var_name])
        sleep_time = self.config['long_sleep_time']
        if sleep_time > 60:
            self.info("Ok, grab a %s. This is going to take a while." % self.config['beverage'])
        else:
            self.info("This will be quick, but grab a %s anyway." % self.config['beverage'])
        self._sleep(self.config['long_sleep_time'])

    def ship_it(self):
        name = "_ship%s" % self.config['ship_style']
        if hasattr(self, name):
            getattr(self, name)()


# __main__ {{{1
if __name__ == '__main__':
    actions_config_example = ActionsConfigExample()
    actions_config_example.run_and_exit()
