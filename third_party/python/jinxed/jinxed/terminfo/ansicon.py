"""
Ansicon virtual terminal codes

Information sourced from:
    https://github.com/adoxa/ansicon/blob/master/sequences.txt

A best effort has been made, but not all information was available
"""

from .xterm_256color import BOOL_CAPS, NUM_CAPS, STR_CAPS

BOOL_CAPS = BOOL_CAPS[:]
NUM_CAPS = NUM_CAPS.copy()
STR_CAPS = STR_CAPS.copy()


# Added
STR_CAPS['cht'] = b'\x1b[%p1%dI'
STR_CAPS['cnl'] = b'\x1b[%p1%dE'
STR_CAPS['cpl'] = b'\x1b[%p1%dF'
STR_CAPS['da1'] = b'\x1b[0c'
STR_CAPS['dsr'] = b'\x1b[5n'
STR_CAPS['hvp'] = b'\x1b[%i%p1%d;%p2%df'  # Same as cup
STR_CAPS['setb'] = b'\x1b[48;5;%p1%dm'
STR_CAPS['setf'] = b'\x1b[38;5;%p1%dm'

# Removed - These do not appear to be supported
del STR_CAPS['dim']
del STR_CAPS['flash']
del STR_CAPS['invis']
del STR_CAPS['kcbt']
del STR_CAPS['kEND']
del STR_CAPS['kf37']
del STR_CAPS['kf38']
del STR_CAPS['kf39']
del STR_CAPS['kf40']
del STR_CAPS['kf41']
del STR_CAPS['kf42']
del STR_CAPS['kf43']
del STR_CAPS['kf44']
del STR_CAPS['kf45']
del STR_CAPS['kf46']
del STR_CAPS['kf47']
del STR_CAPS['kf48']
del STR_CAPS['kf61']
del STR_CAPS['kf62']
del STR_CAPS['kf63']
del STR_CAPS['kIC']
del STR_CAPS['kind']
del STR_CAPS['kLFT']
del STR_CAPS['kmous']
del STR_CAPS['kNXT']
del STR_CAPS['kPRV']
del STR_CAPS['kri']
del STR_CAPS['kRIT']
del STR_CAPS['meml']
del STR_CAPS['memu']
del STR_CAPS['ritm']
del STR_CAPS['rmam']
del STR_CAPS['rmcup']
del STR_CAPS['rmir']
del STR_CAPS['rmkx']
del STR_CAPS['rmm']
del STR_CAPS['sitm']
del STR_CAPS['smam']
del STR_CAPS['smcup']
del STR_CAPS['smir']
del STR_CAPS['smkx']
del STR_CAPS['smm']

# Modified
NUM_CAPS['colors'] = 16
NUM_CAPS['cols'] = 80
NUM_CAPS['lines'] = 30
NUM_CAPS['pairs'] = 256
STR_CAPS['cbt'] = b'\x1b[%p1%dZ'
STR_CAPS['cnorm'] = b'\x1b[?25h'
STR_CAPS['csr'] = b'\x1b[%p1%{1}%+%d;%?%p2%t%p2%{1}%+%dr'
STR_CAPS['cub1'] = b'\x1b[D'
STR_CAPS['cud1'] = b'\x1b[B'
STR_CAPS['cvvis'] = b'\x1b[?25h'
STR_CAPS['initc'] = b'\x1b]4;%p1%d;rgb] =%p2%d/%p3%d/%p4%d\x1b'
STR_CAPS['is2'] = b'\x1b[!p\x1b>'
STR_CAPS['ka1'] = b'\x00G'   # upper left of keypad
STR_CAPS['ka3'] = b'\x00I'   # lower right of keypad
STR_CAPS['kbs'] = b'\x08'
STR_CAPS['kc1'] = b'\x00O'   # lower left of keypad
STR_CAPS['kc3'] = b'\x00Q'   # lower right of keypad
STR_CAPS['kcub1'] = b'\xe0K'
STR_CAPS['kcud1'] = b'\xe0P'
STR_CAPS['kcuf1'] = b'\xe0M'
STR_CAPS['kcuu1'] = b'\xe0H'
STR_CAPS['kDC'] = b'\xe0S'
STR_CAPS['kdch1'] = b'\x0eQ'
STR_CAPS['kend'] = b'\xe0O'
STR_CAPS['kent'] = b'\r'
STR_CAPS['kf1'] = b'\x00;'
STR_CAPS['kf2'] = b'\x00<'
STR_CAPS['kf3'] = b'\x00='
STR_CAPS['kf4'] = b'\x00>'
STR_CAPS['kf5'] = b'\x00?'
STR_CAPS['kf6'] = b'\x00@'
STR_CAPS['kf7'] = b'\x00A'
STR_CAPS['kf8'] = b'\x00B'
STR_CAPS['kf9'] = b'\x00C'
STR_CAPS['kf10'] = b'\x00D'
STR_CAPS['kf11'] = b'\xe0\x85'
STR_CAPS['kf12'] = b'\xe0\x86'
STR_CAPS['kf13'] = b'\x00T'
STR_CAPS['kf14'] = b'\x00U'
STR_CAPS['kf15'] = b'\x00V'
STR_CAPS['kf16'] = b'\x00W'
STR_CAPS['kf17'] = b'\x00X'
STR_CAPS['kf18'] = b'\x00Y'
STR_CAPS['kf19'] = b'\x00Z'
STR_CAPS['kf20'] = b'\x00['
STR_CAPS['kf21'] = b'\x00\\'
STR_CAPS['kf22'] = b'\x00]'
STR_CAPS['kf23'] = b'\xe0\x87'
STR_CAPS['kf24'] = b'\xe0\x88'
STR_CAPS['kf25'] = b'\x00^'
STR_CAPS['kf26'] = b'\x00_'
STR_CAPS['kf27'] = b'\x00`'
STR_CAPS['kf28'] = b'\x00a'
STR_CAPS['kf29'] = b'\x00b'
STR_CAPS['kf30'] = b'\x00c'
STR_CAPS['kf31'] = b'\x00d'
STR_CAPS['kf32'] = b'\x00e'
STR_CAPS['kf33'] = b'\x00f'
STR_CAPS['kf34'] = b'\x00g'
STR_CAPS['kf35'] = b'\xe0\x89'
STR_CAPS['kf36'] = b'\xe0\x8a'
# Missing F37 - F48
STR_CAPS['kf49'] = b'\x00h'
STR_CAPS['kf50'] = b'\x00i'
STR_CAPS['kf51'] = b'\x00j'
STR_CAPS['kf52'] = b'\x00k'
STR_CAPS['kf53'] = b'\x00l'
STR_CAPS['kf54'] = b'\x00m'
STR_CAPS['kf55'] = b'\x00n'
STR_CAPS['kf56'] = b'\x00o'
STR_CAPS['kf57'] = b'\x00p'
STR_CAPS['kf58'] = b'\x00q'
STR_CAPS['kf59'] = b'\xe0\x8b'
STR_CAPS['kf60'] = b'\xe0\x8b'
# Missing F61 - F63
STR_CAPS['khome'] = b'\xe0G'
STR_CAPS['kich1'] = b'\xe0R'
STR_CAPS['knp'] = b'\xe0Q'
STR_CAPS['kpp'] = b'\xe0I'
STR_CAPS['rs1'] = b'\x1bc\x1b]104ST'
STR_CAPS['rs2'] = b'\x1b[!p'
STR_CAPS['sgr'] = b'\x1b[%p1%d%?%p2%t;%p2%d%;%?%p3%t;%p3%d%;%?%p4%t;%p4%d%;%?%p5%t;%p5%d%;' \
                  b'%?%p6%t;%p6%d%;%?%p7%t;%p7%d%;%?%p8%t;%p8%d%;%?%p9%t;%p9%d%;m'

# Need info - Left in, but unsure
# acsc (covers some, but maybe not all)
# mc0/mc4/mc5 (print screen/off/on)
