#
# This file is part of pyasn1 software.
#
# Copyright (c) 2005-2017, Ilya Etingof <etingof@gmail.com>
# License: http://pyasn1.sf.net/license.html
#
from sys import version_info
from datetime import datetime
import time

__all__ = ['strptime']


if version_info[:2] <= (2, 4):

    def strptime(text, dateFormat):
        return datetime(*(time.strptime(text, dateFormat)[0:6]))

else:

    def strptime(text, dateFormat):
        return datetime.strptime(text, dateFormat)
