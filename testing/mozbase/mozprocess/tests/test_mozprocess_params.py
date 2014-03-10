#!/usr/bin/env python

# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

import unittest
from mozprocess import processhandler

class ParamTests(unittest.TestCase):


    def test_process_outputline_handler(self):
        """Parameter processOutputLine is accepted with a single function"""
        def output(line):
            print("output " + str(line))
        err = None
        try:
            p = processhandler.ProcessHandler(['ls','-l'], processOutputLine=output)
        except (TypeError, AttributeError) as e:
            err = e
        self.assertFalse(err)

    def test_process_outputline_handler_list(self):
        """Parameter processOutputLine is accepted with a list of functions"""
        def output(line):
            print("output " + str(line))
        err = None
        try:
            p = processhandler.ProcessHandler(['ls','-l'], processOutputLine=[output])
        except (TypeError, AttributeError) as e:
            err = e
        self.assertFalse(err)


    def test_process_ontimeout_handler(self):
        """Parameter onTimeout is accepted with a single function"""
        def timeout():
            print("timeout!")
        err = None
        try:
            p = processhandler.ProcessHandler(['sleep', '2'], onTimeout=timeout)
        except (TypeError, AttributeError) as e:
            err = e
        self.assertFalse(err)

    def test_process_ontimeout_handler_list(self):
        """Parameter onTimeout is accepted with a list of functions"""
        def timeout():
            print("timeout!")
        err = None
        try:
            p = processhandler.ProcessHandler(['sleep', '2'], onTimeout=[timeout])
        except (TypeError, AttributeError) as e:
            err = e
        self.assertFalse(err)

    def test_process_onfinish_handler(self):
        """Parameter onFinish is accepted with a single function"""
        def finish():
            print("finished!")
        err = None
        try:
            p = processhandler.ProcessHandler(['sleep', '1'], onFinish=finish)
        except (TypeError, AttributeError) as e:
            err = e
        self.assertFalse(err)

    def test_process_onfinish_handler_list(self):
        """Parameter onFinish is accepted with a list of functions"""
        def finish():
            print("finished!")
        err = None
        try:
            p = processhandler.ProcessHandler(['sleep', '1'], onFinish=[finish])
        except (TypeError, AttributeError) as e:
            err = e
        self.assertFalse(err)


def main():
    unittest.main()

if __name__ == '__main__':
    main()

