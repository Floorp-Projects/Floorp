"""Main module (invoked by "python -m cram")"""

import sys

import cram

try:
    sys.exit(cram.main(sys.argv[1:]))
except KeyboardInterrupt:
    pass
