#!/usr/bin/env python
# -*- coding: utf-8 -*-


import unittest2
from rsa._compat import b
from rsa.pem import _markers


class Test__markers(unittest2.TestCase):
    def test_values(self):
        self.assertEqual(_markers('RSA PRIVATE KEY'),
            (b('-----BEGIN RSA PRIVATE KEY-----'),
             b('-----END RSA PRIVATE KEY-----')))
