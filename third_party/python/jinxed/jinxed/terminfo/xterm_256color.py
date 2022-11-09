"""
xterm-256color terminal info

The values are as reported by infocmp on Fedora 30 with ncurses 6.1
"""

from .xterm import BOOL_CAPS, NUM_CAPS, STR_CAPS

BOOL_CAPS = BOOL_CAPS[:]
NUM_CAPS = NUM_CAPS.copy()
STR_CAPS = STR_CAPS.copy()

# Added
BOOL_CAPS.append('ccc')
STR_CAPS['initc'] = b'\x1b]4;%p1%d;rgb\x5c:%p2%{255}%*%{1000}%/%2.2X/' \
                    b'%p3%{255}%*%{1000}%/%2.2X/%p4%{255}%*%{1000}%/%2.2X\x1b\x5c'
STR_CAPS['oc'] = b'\x1b]104\007'

# Removed
del STR_CAPS['setb']
del STR_CAPS['setf']

# Modified
NUM_CAPS['colors'] = 256
NUM_CAPS['pairs'] = 65536
STR_CAPS['rs1'] = b'\x1bc\x1b]104\007'
STR_CAPS['setab'] = b'\x1b[%?%p1%{8}%<%t4%p1%d%e%p1%{16}%<%t10%p1%{8}%-%d%e48;5;%p1%d%;m'
STR_CAPS['setaf'] = b'\x1b[%?%p1%{8}%<%t3%p1%d%e%p1%{16}%<%t9%p1%{8}%-%d%e38;5;%p1%d%;m'
