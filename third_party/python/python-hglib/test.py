#!/usr/bin/env python

import nose
from tests import with_hg

if __name__ == '__main__':
    nose.main(addplugins=[with_hg.WithHgPlugin()])
