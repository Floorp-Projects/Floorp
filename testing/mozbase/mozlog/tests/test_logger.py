# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

import mozlog
import mozfile
import unittest
import socket
import time
import threading
import json

class ListHandler(mozlog.Handler):
    """Mock handler appends messages to a list for later inspection."""

    def __init__(self):
        mozlog.Handler.__init__(self)
        self.messages = []

    def emit(self, record):
        self.messages.append(self.format(record))

class TestLogging(unittest.TestCase):
    """Tests behavior of basic mozlog api."""

    def test_logger_defaults(self):
        """Tests the default logging format and behavior."""

        default_logger = mozlog.getLogger('default.logger')
        self.assertEqual(default_logger.name, 'default.logger')
        self.assertEqual(len(default_logger.handlers), 1)
        self.assertTrue(isinstance(default_logger.handlers[0],
                                   mozlog.StreamHandler))

        f = mozfile.NamedTemporaryFile()
        list_logger = mozlog.getLogger('file.logger',
                                       handler=mozlog.FileHandler(f.name))
        self.assertEqual(len(list_logger.handlers), 1)
        self.assertTrue(isinstance(list_logger.handlers[0],
                                   mozlog.FileHandler))
        f.close()

        self.assertRaises(ValueError, mozlog.getLogger,
                          'file.logger', handler=ListHandler())

class TestStructuredLogging(unittest.TestCase):
    """Tests structured output in mozlog."""

    def setUp(self):
        self.handler = ListHandler()
        self.handler.setFormatter(mozlog.JSONFormatter())
        self.logger = mozlog.MozLogger('test.Logger')
        self.logger.addHandler(self.handler)
        self.logger.setLevel(mozlog.DEBUG)

    def check_messages(self, expected, actual):
        """Checks actual for equality with corresponding fields in actual.
        The actual message should contain all fields in expected, and
        should be identical, with the exception of the timestamp field.
        The actual message should contain no fields other than the timestamp
        field and those present in expected."""

        self.assertTrue(isinstance(actual['_time'], (int, long)))

        for k, v in expected.items():
            self.assertEqual(v, actual[k])

        for k in actual.keys():
            if k != '_time':
                self.assertTrue(expected.get(k) is not None)

    def test_structured_output(self):
        self.logger.log_structured('test_message',
                                   {'_level': mozlog.INFO,
                                    'message': 'message one'})
        self.logger.log_structured('test_message',
                                   {'_level': mozlog.INFO,
                                    'message': 'message two'})

        message_one_expected = {'_namespace': 'test.Logger',
                                '_level': 'INFO',
                                'message': 'message one',
                                'action': 'test_message'}
        message_two_expected = {'_namespace': 'test.Logger',
                                '_level': 'INFO',
                                'message': 'message two',
                                'action': 'test_message'}

        message_one_actual = json.loads(self.handler.messages[0])
        message_two_actual = json.loads(self.handler.messages[1])

        self.check_messages(message_one_expected, message_one_actual)
        self.check_messages(message_two_expected, message_two_actual)

    def message_callback(self):
        if len(self.handler.messages) == 3:
            message_one_expected = {'_namespace': 'test.Logger',
                                    '_level': 'DEBUG',
                                    'message': 'socket message one',
                                    'action': 'test_message'}
            message_two_expected = {'_namespace': 'test.Logger',
                                    '_level': 'DEBUG',
                                    'message': 'socket message two',
                                    'action': 'test_message'}
            message_three_expected = {'_namespace': 'test.Logger',
                                      '_level': 'DEBUG',
                                      'message': 'socket message three',
                                      'action': 'test_message'}

            message_one_actual = json.loads(self.handler.messages[0])

            message_two_actual = json.loads(self.handler.messages[1])

            message_three_actual = json.loads(self.handler.messages[2])

            self.check_messages(message_one_expected, message_one_actual)
            self.check_messages(message_two_expected, message_two_actual)
            self.check_messages(message_three_expected, message_three_actual)

    def test_log_listener(self):
        connection = '127.0.0.1', 0
        self.log_server = mozlog.LogMessageServer(connection,
                                                  self.logger,
                                                  message_callback=self.message_callback,
                                                  timeout=0.5)

        # The namespace fields of these messages will be overwritten.
        message_string_one = json.dumps({'message': 'socket message one',
                                         'action': 'test_message',
                                         '_level': 'DEBUG',
                                         '_namespace': 'foo.logger'})

        message_string_two = json.dumps({'message': 'socket message two',
                                         'action': 'test_message',
                                         '_level': 'DEBUG',
                                         '_namespace': 'foo.logger'})

        message_string_three = json.dumps({'message': 'socket message three',
                                           'action': 'test_message',
                                           '_level': 'DEBUG',
                                           '_namespace': 'foo.logger'})

        message_string = message_string_one + '\n' + \
                         message_string_two + '\n' + \
                         message_string_three + '\n'

        server_thread = threading.Thread(target=self.log_server.handle_request)
        server_thread.start()

        host, port = self.log_server.server_address

        sock = socket.socket()
        sock.connect((host, port))

        # Sleeps prevent listener from receiving entire message in a single call
        # to recv in order to test reconstruction of partial messages.
        sock.sendall(message_string[:8])
        time.sleep(.01)
        sock.sendall(message_string[8:32])
        time.sleep(.01)
        sock.sendall(message_string[32:64])
        time.sleep(.01)
        sock.sendall(message_string[64:128])
        time.sleep(.01)
        sock.sendall(message_string[128:])

        server_thread.join()

if __name__ == '__main__':
    unittest.main()
