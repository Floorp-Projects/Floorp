"""
xterm terminal info

Since most of the Windows virtual processing schemes are based on xterm
This file is intended to be sourced and includes the man page descriptions

Most of this information came from the terminfo man pages, part of ncurses
More information on ncurses can be found at:
https://www.gnu.org/software/ncurses/ncurses.html

The values are as reported by infocmp on Fedora 30 with ncurses 6.1
"""

# pylint: disable=wrong-spelling-in-comment,line-too-long
# flake8: noqa: E501

BOOL_CAPS = [
    'am',  # (auto_right_margin) terminal has automatic margins
    'bce',  # (back_color_erase) screen erased with background color
    # 'bw',  # (auto_left_margin) cub1 wraps from column 0 to last column
    # 'ccc',  # (can_change) terminal can re-define existing colors
    # 'chts',  # (hard_cursor) cursor is hard to see
    # 'cpix',  # (cpi_changes_res) changing character pitch changes resolution
    # 'crxm',  # (cr_cancels_micro_mode) using cr turns off micro mode
    # 'daisy',  # (has_print_wheel) printer needs operator to change character set
    # 'da',  # (memory_above) display may be retained above the screen
    # 'db',  # (memory_below) display may be retained below the screen
    # 'eo',  # (erase_overstrike) can erase overstrikes with a blank
    # 'eslok',  # (status_line_esc_ok) escape can be used on the status line
    # 'gn',  # (generic_type) generic line type
    # 'hc',  # (hard_copy) hardcopy terminal
    # 'hls',  # (hue_lightness_saturation) terminal uses only HLS color notation (Tektronix)
    # 'hs',  # (has_status_line) has extra status line
    # 'hz',  # (tilde_glitch) cannot print ~'s (Hazeltine)
    # 'in',  # (insert_null_glitch) insert mode distinguishes nulls
    'km',  # (has_meta_key) Has a meta key (i.e., sets 8th-bit)
    # 'lpix',  # (lpi_changes_res) changing line pitch changes resolution
    'mc5i',  # (prtr_silent) printer will not echo on screen
    'mir',  # (move_insert_mode) safe to move while in insert mode
    'msgr',  # (move_standout_mode) safe to move while in standout mode
    # 'ndscr',  # (non_dest_scroll_region) scrolling region is non-destructive
    'npc',  # (no_pad_char) pad character does not exist
    # 'nrrmc',  # (non_rev_rmcup) smcup does not reverse rmcup
    # 'nxon',  # (needs_xon_xoff) padding will not work, xon/xoff required
    # 'os',  # (over_strike) terminal can overstrike
    # 'sam',  # (semi_auto_right_margin) printing in last column causes cr
    # 'ul',  # (transparent_underline) underline character overstrikes
    'xenl',  # (eat_newline_glitch) newline ignored after 80 cols (concept)
    # 'xhpa',  # (col_addr_glitch) only positive motion for hpa/mhpa caps
    # 'xhp',  # (ceol_standout_glitch) standout not erased by overwriting (hp)
    # 'xon',  # (xon_xoff) terminal uses xon/xoff handshaking
    # 'xsb',  # (no_esc_ctlc) beehive (f1=escape, f2=ctrl C)
    # 'xt',  # (dest_tabs_magic_smso) tabs destructive, magic so char (t1061)
    # 'xvpa',  # (row_addr_glitch) only positive motion for vpa/mvpa caps
]

NUM_CAPS = {
    # 'bitwin': 0,  # (bit_image_entwining) number of passes for each bit-image row
    # 'bitype': 0,  # (bit_image_type) type of bit-image device
    # 'btns': 0,  # (buttons) number of buttons on mouse
    # 'bufsz': 0,  # (buffer_capacity) numbers of bytes buffered before printing
    'colors': 8,  # (max_colors) maximum number of colors on screen
    'cols': 80,  # (columns) number of columns in a line
    # 'cps': 0,  # (print_rate) print rate in characters per second
    'it': 8,  # (init_tabs) tabs initially every # spaces
    # 'lh': 0,  # (label_height) rows in each label
    'lines': 24,  # (lines) number of lines on screen or page
    # 'lm': 0,  # (lines_of_memory) lines of memory if > line. 0 means varies
    # 'lw': 0,  # (label_width) columns in each label
    # 'ma': 0,  # (max_attributes) maximum combined attributes terminal can handle
    # 'maddr': 0,  # (max_micro_address) maximum value in micro_..._address
    # 'mcs': 0,  # (micro_col_size) character step size when in micro mode
    # 'mjump': 0,  # (max_micro_jump) maximum value in parm_..._micro
    # 'mls': 0,  # (micro_line_size) line step size when in micro mode
    # 'ncv': 0,  # (no_color_video) video attributes that cannot be used with colors
    # 'nlab': 0,  # (num_labels) number of labels on screen
    # 'npins': 0,  # (number_of_pins) numbers of pins in print-head
    # 'orc': 0,  # (output_res_char) horizontal resolution in units per line
    # 'orhi': 0,  # (output_res_horz_inch) horizontal resolution in units per inch
    # 'orl': 0,  # (output_res_line) vertical resolution in units per line
    # 'orvi': 0,  # (output_res_vert_inch) vertical resolution in units per inch
    'pairs': 64,  # (max_pairs) maximum number of color-pairs on the screen
    # 'pb': 0,  # (padding_baud_rate) lowest baud rate where padding needed
    # 'spinh': 0,  # (dot_horz_spacing) spacing of dots horizontally in dots per inch
    # 'spinv': 0,  # (dot_vert_spacing) spacing of pins vertically in pins per inch
    # 'vt': 0,  # (virtual_terminal) virtual terminal number (CB/unix)
    # 'widcs': 0,  # (wide_char_size) character step size when in double wide mode
    # 'wnum': 0,  # (maximum_windows) maximum number of definable windows
    # 'wsl': 0,  # (width_status_line) number of columns in status line
    # 'xmc': 0,  # (magic_cookie_glitch) number of blank characters left by smso or rmso
}

STR_CAPS = {
    'acsc': b'``aaffggiijjkkllmmnnooppqqrrssttuuvvwwxxyyzz{{||}}~~',  # (acs_chars) graphics charset pairs, based on vt100
    'bel': b'^G',  # (bell) audible signal (bell) (P)
    # 'bicr': b'',  # (bit_image_carriage_return) Move to beginning of same row
    # 'binel': b'',  # (bit_image_newline) Move to next row of the bit image
    # 'birep': b'',  # (bit_image_repeat) Repeat bit image cell #1 #2 times
    'blink': b'\x1b[5m',  # (enter_blink_mode) turn on blinking
    'bold': b'\x1b[1m',  # (enter_bold_mode) turn on bold (extra bright) mode
    'cbt': b'\x1b[Z',  # (back_tab) back tab (P)
    # 'chr': b'',  # (change_res_horz) Change horizontal resolution to #1
    'civis': b'\x1b[?25l',  # (cursor_invisible) make cursor invisible
    'clear': b'\x1b[H\x1b[2J',  # (clear_screen) clear screen and home cursor (P*)
    # 'cmdch': b'',  # (command_character) terminal settable cmd character in prototype !?
    'cnorm': b'\x1b[?12l\x1b[?25h',  # (cursor_normal) make cursor appear normal (undo civis/cvvis)
    # 'colornm': b'',  # (color_names) Give name for color #1
    # 'cpi': b'',  # (change_char_pitch) Change number of characters per inch to #1
    'cr': b'\r',  # (carriage_return) carriage return (P*) (P*)
    # 'csin': b'',  # (code_set_init) Init sequence for multiple codesets
    # 'csnm': b'',  # (char_set_names) Produce #1'th item from list of character set names
    'csr': b'\x1b[%i%p1%d;%p2%dr',  # (change_scroll_region) change region to line #1 to line #2 (P)
    'cub1': b'^H',  # (cursor_left) move left one space
    'cub': b'\x1b[%p1%dD',  # (parm_left_cursor) move #1 characters to the left (P)
    'cud1': b'\n',  # (cursor_down) down one line
    'cud': b'\x1b[%p1%dB',  # (parm_down_cursor) down #1 lines (P*)
    'cuf1': b'\x1b[C',  # (cursor_right) non-destructive space (move right one space)
    'cuf': b'\x1b[%p1%dC',  # (parm_right_cursor) move #1 characters to the right (P*)
    'cup': b'\x1b[%i%p1%d;%p2%dH',  # (cursor_address) move to row #1 columns #2
    'cuu1': b'\x1b[A',  # (cursor_up) up one line
    'cuu': b'\x1b[%p1%dA',  # (parm_up_cursor) up #1 lines (P*)
    # 'cvr': b'',  # (change_res_vert) Change vertical resolution to #1
    'cvvis': b'\x1b[?12;25h',  # (cursor_visible) make cursor very visible
    # 'cwin': b'',  # (create_window) define a window #1 from #2,#3 to #4,#5
    'dch1': b'\x1b[P',  # (delete_character) delete character (P*)
    'dch': b'\x1b[%p1%dP',  # (parm_dch) delete #1 characters (P*)
    # 'dclk': b'',  # (display_clock) display clock
    # 'defbi': b'',  # (define_bit_image_region) Define rectangular bit image region
    # 'defc': b'',  # (define_char) Define a character #1, #2 dots wide, descender #3
    # 'devt': b'',  # (device_type) Indicate language/codeset support
    # 'dial': b'',  # (dial_phone) dial number #1
    'dim': b'\x1b[2m',  # (enter_dim_mode) turn on half-bright mode
    # 'dispc': b'',  # (display_pc_char) Display PC character #1
    'dl1': b'\x1b[M',  # (delete_line) delete line (P*)
    'dl': b'\x1b[%p1%dM',  # (parm_delete_line) delete #1 lines (P*)
    # 'docr': b'',  # (these_cause_cr) Printing any of these characters causes CR
    # 'dsl': b'',  # (dis_status_line) disable status line
    'ech': b'\x1b[%p1%dX',  # (erase_chars) erase #1 characters (P)
    'ed': b'\x1b[J',  # (clr_eos) clear to end of screen (P*)
    'el1': b'\x1b[1K',  # (clr_bol) Clear to beginning of line
    'el': b'\x1b[K',  # (clr_eol) clear to end of line (P)
    # 'enacs': b'',  # (ena_acs) enable alternate char set
    # 'endbi': b'',  # (end_bit_image_region) End a bit-image region
    # 'ff': b'',  # (form_feed) hardcopy terminal page eject (P*)
    'flash': b'\x1b[?5h$<100/>\x1b[?5l',  # (flash_screen) visible bell (may not move cursor)
    # 'fln': b'',  # (label_format) label format
    # 'fsl': b'',  # (from_status_line) return from status line
    # 'getm': b'',  # (get_mouse) Curses should get button events, parameter #1 not documented.
    # 'hd': b'',  # (down_half_line) half a line down
    'home': b'\x1b[H',  # (cursor_home) home cursor (if no cup)
    # 'hook': b'',  # (flash_hook) flash switch hook
    'hpa': b'\x1b[%i%p1%dG',  # (column_address) horizontal position #1, absolute (P)
    'ht': b'^I',  # (tab) tab to next 8-space hardware tab stop
    'hts': b'\x1bH',  # (set_tab) set a tab in every row, current columns
    # 'hu': b'',  # (up_half_line) half a line up
    # 'hup': b'',  # (hangup) hang-up phone
    # 'ich1': b'',  # (insert_character) insert character (P)
    'ich': b'\x1b[%p1%d@',  # (parm_ich) insert #1 characters (P*)
    # 'if': b'',  # (init_file) name of initialization file
    'il1': b'\x1b[L',  # (insert_line) insert line (P*)
    'il': b'\x1b[%p1%dL',  # (parm_insert_line) insert #1 lines (P*)
    'ind': b'\n',  # (scroll_forward) scroll text up (P)
    'indn': b'\x1b[%p1%dS',  # (parm_index) scroll forward #1 lines (P)
    # 'initc': b'',  # (initialize_color) initialize color #1 to (#2,#3,#4)
    # 'initp': b'',  # (initialize_pair) Initialize color pair #1 to fg=(#2,#3,#4), bg=(#5,#6,#7)
    'invis': b'\x1b[8m',  # (enter_secure_mode) turn on blank mode (characters invisible)
    # 'ip': b'',  # (insert_padding) insert padding after inserted character
    # 'iprog': b'',  # (init_prog) path name of program for initialization
    # 'is1': b'',  # (init_1string) initialization string
    'is2': b'\x1b[!p\x1b[?3;4l\x1b[4l\x1b>',  # (init_2string) initialization string
    # 'is3': b'',  # (init_3string) initialization string
    # 'ka1': b'',  # (key_a1) upper left of keypad
    # 'ka3': b'',  # (key_a3) upper right of keypad
    'kb2': b'\x1bOE',  # (key_b2) center of keypad
    # 'kbeg': b'',  # (key_beg) begin key
    # 'kBEG': b'',  # (key_sbeg) shifted begin key
    'kbs': b'^?',  # (key_backspace) backspace key
    # 'kc1': b'',  # (key_c1) lower left of keypad
    # 'kc3': b'',  # (key_c3) lower right of keypad
    # 'kcan': b'',  # (key_cancel) cancel key
    # 'kCAN': b'',  # (key_scancel) shifted cancel key
    'kcbt': b'\x1b[Z',  # (key_btab) back-tab key
    # 'kclo': b'',  # (key_close) close key
    # 'kclr': b'',  # (key_clear) clear-screen or erase key
    # 'kcmd': b'',  # (key_command) command key
    # 'kCMD': b'',  # (key_scommand) shifted command key
    # 'kcpy': b'',  # (key_copy) copy key
    # 'kCPY': b'',  # (key_scopy) shifted copy key
    # 'kcrt': b'',  # (key_create) create key
    # 'kCRT': b'',  # (key_screate) shifted create key
    # 'kctab': b'',  # (key_ctab) clear-tab key
    'kcub1': b'\x1bOD',  # (key_left) left-arrow key
    'kcud1': b'\x1bOB',  # (key_down) down-arrow key
    'kcuf1': b'\x1bOC',  # (key_right) right-arrow key
    'kcuu1': b'\x1bOA',  # (key_up) up-arrow key
    'kDC': b'\x1b[3;2~',  # (key_sdc) shifted delete- character key
    'kdch1': b'\x1b[3~',  # (key_dc) delete-character key
    # 'kdl1': b'',  # (key_dl) delete-line key
    # 'kDL': b'',  # (key_sdl) shifted delete-line key
    # 'ked': b'',  # (key_eos) clear-to-end-of- screen key
    # 'kel': b'',  # (key_eol) clear-to-end-of-line key
    'kEND': b'\x1b[1;2F',  # (key_send) shifted end key
    'kend': b'\x1bOF',  # (key_end) end key
    'kent': b'\x1bOM',  # (key_enter) enter/send key
    # 'kEOL': b'',  # (key_seol) shifted clear-to- end-of-line key
    # 'kext': b'',  # (key_exit) exit key
    # 'kEXT': b'',  # (key_sexit) shifted exit key
    # 'kf0': b'',  # (key_f0) F0 function key
    'kf1': b'\x1bOP',  # (key_f1) F1 function key
    'kf2': b'\x1bOQ',  # (key_f2) F2 function key
    'kf3': b'\x1bOR',  # (key_f3) F3 function key
    'kf4': b'\x1bOS',  # (key_f4) F4 function key
    'kf5': b'\x1b[15~',  # (key_f5) F5 function key
    'kf6': b'\x1b[17~',  # (key_f6) F6 function key
    'kf7': b'\x1b[18~',  # (key_f7) F7 function key
    'kf8': b'\x1b[19~',  # (key_f8) F8 function key
    'kf9': b'\x1b[20~',  # (key_f9) F9 function key
    'kf10': b'\x1b[21~',  # (key_f10) F10 function key
    'kf11': b'\x1b[23~',  # (key_f11) F11 function key
    'kf12': b'\x1b[24~',  # (key_f12) F12 function key
    'kf13': b'\x1b[1;2P',  # (key_f13) F13 function key
    'kf14': b'\x1b[1;2Q',  # (key_f14) F14 function key
    'kf15': b'\x1b[1;2R',  # (key_f15) F15 function key
    'kf16': b'\x1b[1;2S',  # (key_f16) F16 function key
    'kf17': b'\x1b[15;2~',  # (key_f17) F17 function key
    'kf18': b'\x1b[17;2~',  # (key_f18) F18 function key
    'kf19': b'\x1b[18;2~',  # (key_f19) F19 function key
    'kf20': b'\x1b[19;2~',  # (key_f20) F20 function key
    'kf21': b'\x1b[20;2~',  # (key_f21) F21 function key
    'kf22': b'\x1b[21;2~',  # (key_f22) F22 function key
    'kf23': b'\x1b[23;2~',  # (key_f23) F23 function key
    'kf24': b'\x1b[24;2~',  # (key_f24) F24 function key
    'kf25': b'\x1b[1;5P',  # (key_f25) F25 function key
    'kf26': b'\x1b[1;5Q',  # (key_f26) F26 function key
    'kf27': b'\x1b[1;5R',  # (key_f27) F27 function key
    'kf28': b'\x1b[1;5S',  # (key_f28) F28 function key
    'kf29': b'\x1b[15;5~',  # (key_f29) F29 function key
    'kf30': b'\x1b[17;5~',  # (key_f30) F30 function key
    'kf31': b'\x1b[18;5~',  # (key_f31) F31 function key
    'kf32': b'\x1b[19;5~',  # (key_f32) F32 function key
    'kf33': b'\x1b[20;5~',  # (key_f33) F33 function key
    'kf34': b'\x1b[21;5~',  # (key_f34) F34 function key
    'kf35': b'\x1b[23;5~',  # (key_f35) F35 function key
    'kf36': b'\x1b[24;5~',  # (key_f36) F36 function key
    'kf37': b'\x1b[1;6P',  # (key_f37) F37 function key
    'kf38': b'\x1b[1;6Q',  # (key_f38) F38 function key
    'kf39': b'\x1b[1;6R',  # (key_f39) F39 function key
    'kf40': b'\x1b[1;6S',  # (key_f40) F40 function key
    'kf41': b'\x1b[15;6~',  # (key_f41) F41 function key
    'kf42': b'\x1b[17;6~',  # (key_f42) F42 function key
    'kf43': b'\x1b[18;6~',  # (key_f43) F43 function key
    'kf44': b'\x1b[19;6~',  # (key_f44) F44 function key
    'kf45': b'\x1b[20;6~',  # (key_f45) F45 function key
    'kf46': b'\x1b[21;6~',  # (key_f46) F46 function key
    'kf47': b'\x1b[23;6~',  # (key_f47) F47 function key
    'kf48': b'\x1b[24;6~',  # (key_f48) F48 function key
    'kf49': b'\x1b[1;3P',  # (key_f49) F49 function key
    'kf50': b'\x1b[1;3Q',  # (key_f50) F50 function key
    'kf51': b'\x1b[1;3R',  # (key_f51) F51 function key
    'kf52': b'\x1b[1;3S',  # (key_f52) F52 function key
    'kf53': b'\x1b[15;3~',  # (key_f53) F53 function key
    'kf54': b'\x1b[17;3~',  # (key_f54) F54 function key
    'kf55': b'\x1b[18;3~',  # (key_f55) F55 function key
    'kf56': b'\x1b[19;3~',  # (key_f56) F56 function key
    'kf57': b'\x1b[20;3~',  # (key_f57) F57 function key
    'kf58': b'\x1b[21;3~',  # (key_f58) F58 function key
    'kf59': b'\x1b[23;3~',  # (key_f59) F59 function key
    'kf60': b'\x1b[24;3~',  # (key_f60) F60 function key
    'kf61': b'\x1b[1;4P',  # (key_f61) F61 function key
    'kf62': b'\x1b[1;4Q',  # (key_f62) F62 function key
    'kf63': b'\x1b[1;4R',  # (key_f63) F63 function key
    # 'kfnd': b'',  # (key_find) find key
    # 'kFND': b'',  # (key_sfind) shifted find key
    # 'khlp': b'',  # (key_help) help key
    # 'kHLP': b'',  # (key_shelp) shifted help key
    'kHOM': b'\x1b[1;2H',  # (key_shome) shifted home key
    'khome': b'\x1bOH',  # (key_home) home key
    # 'khts': b'',  # (key_stab) set-tab key
    'kIC': b'\x1b[2;2~',  # (key_sic) shifted insert- character key
    'kich1': b'\x1b[2~',  # (key_ic) insert-character key
    # 'kil1': b'',  # (key_il) insert-line key
    'kind': b'\x1b[1;2B',  # (key_sf) scroll-forward key
    'kLFT': b'\x1b[1;2D',  # (key_sleft) shifted left-arrow key
    # 'kll': b'',  # (key_ll) lower-left key (home down)
    'kmous': b'\x1b[<',  # (key_mouse) Mouse event has occurred
    # 'kmov': b'',  # (key_move) move key
    # 'kMOV': b'',  # (key_smove) shifted move key
    # 'kmrk': b'',  # (key_mark) mark key
    # 'kmsg': b'',  # (key_message) message key
    # 'kMSG': b'',  # (key_smessage) shifted message key
    'knp': b'\x1b[6~',  # (key_npage) next-page key
    # 'knxt': b'',  # (key_next) next key
    'kNXT': b'\x1b[6;2~',  # (key_snext) shifted next key
    # 'kopn': b'',  # (key_open) open key
    # 'kopt': b'',  # (key_options) options key
    # 'kOPT': b'',  # (key_soptions) shifted options key
    'kpp': b'\x1b[5~',  # (key_ppage) previous-page key
    # 'kprt': b'',  # (key_print) print key
    # 'kPRT': b'',  # (key_sprint) shifted print key
    # 'kprv': b'',  # (key_previous) previous key
    'kPRV': b'\x1b[5;2~',  # (key_sprevious) shifted previous key
    # 'krdo': b'',  # (key_redo) redo key
    # 'kRDO': b'',  # (key_sredo) shifted redo key
    # 'kref': b'',  # (key_reference) reference key
    # 'kres': b'',  # (key_resume) resume key
    # 'kRES': b'',  # (key_srsume) shifted resume key
    # 'krfr': b'',  # (key_refresh) refresh key
    'kri': b'\x1b[1;2A',  # (key_sr) scroll-backward key
    'kRIT': b'\x1b[1;2C',  # (key_sright) shifted right-arrow key
    # 'krmir': b'',  # (key_eic) sent by rmir or smir in insert mode
    # 'krpl': b'',  # (key_replace) replace key
    # 'kRPL': b'',  # (key_sreplace) shifted replace key
    # 'krst': b'',  # (key_restart) restart key
    # 'ksav': b'',  # (key_save) save key
    # 'kSAV': b'',  # (key_ssave) shifted save key
    # 'kslt': b'',  # (key_select) select key
    # 'kSPD': b'',  # (key_ssuspend) shifted suspend key
    # 'kspd': b'',  # (key_suspend) suspend key
    # 'ktbc': b'',  # (key_catab) clear-all-tabs key
    # 'kUND': b'',  # (key_sundo) shifted undo key
    # 'kund': b'',  # (key_undo) undo key
    # 'lf0': b'',  # (lab_f0) label on function key f0 if not f0
    # 'lf10': b'',  # (lab_f10) label on function key f10 if not f10
    # 'lf1': b'',  # (lab_f1) label on function key f1 if not f1
    # 'lf2': b'',  # (lab_f2) label on function key f2 if not f2
    # 'lf3': b'',  # (lab_f3) label on function key f3 if not f3
    # 'lf4': b'',  # (lab_f4) label on function key f4 if not f4
    # 'lf5': b'',  # (lab_f5) label on function key f5 if not f5
    # 'lf6': b'',  # (lab_f6) label on function key f6 if not f6
    # 'lf7': b'',  # (lab_f7) label on function key f7 if not f7
    # 'lf8': b'',  # (lab_f8) label on function key f8 if not f8
    # 'lf9': b'',  # (lab_f9) label on function key f9 if not f9
    # 'll': b'',  # (cursor_to_ll) last line, first column (if no cup)
    # 'lpi': b'',  # (change_line_pitch) Change number of lines per inch to #1
    'meml': b'\x1bl',  # lock memory above the curser
    'memu': b'\x1bl',  # unlock memory above the curser
    'mc0': b'\x1b[i',  # (print_screen) print contents of screen
    'mc4': b'\x1b[4i',  # (prtr_off) turn off printer
    'mc5': b'\x1b[5i',  # (prtr_on) turn on printer
    # 'mc5p': b'',  # (prtr_non) turn on printer for #1 bytes
    # 'mcub1': b'',  # (micro_left) Like cursor_left in micro mode
    # 'mcub': b'',  # (parm_left_micro) Like parm_left_cursor in micro mode
    # 'mcud1': b'',  # (micro_down) Like cursor_down in micro mode
    # 'mcud': b'',  # (parm_down_micro) Like parm_down_cursor in micro mode
    # 'mcuf1': b'',  # (micro_right) Like cursor_right in micro mode
    # 'mcuf': b'',  # (parm_right_micro) Like parm_right_cursor in micro mode
    # 'mcuu1': b'',  # (micro_up) Like cursor_up in micro mode
    # 'mcuu': b'',  # (parm_up_micro) Like parm_up_cursor in micro mode
    # 'mgc': b'',  # (clear_margins) clear right and left soft margins
    # 'mhpa': b'',  # (micro_column_address) Like column_address in micro mode
    # 'minfo': b'',  # (mouse_info) Mouse status information
    # 'mrcup': b'',  # (cursor_mem_address) memory relative cursor addressing, move to row #1 columns #2
    # 'mvpa': b'',  # (micro_row_address) Like row_address #1 in micro mode
    # 'nel': b'',  # (newline) newline (behave like cr followed by lf)
    # 'oc': b'',  # (orig_colors) Set all color pairs to the original ones
    'op': b'\x1b[39;49m',  # (orig_pair) Set default pair to its original value
    # 'pad': b'',  # (pad_char) padding char (instead of null)
    # 'pause': b'',  # (fixed_pause) pause for 2-3 seconds
    # 'pctrm': b'',  # (pc_term_options) PC terminal options
    # 'pfkey': b'',  # (pkey_key) program function key #1 to type string #2
    # 'pfloc': b'',  # (pkey_local) program function key #1 to execute string #2
    # 'pfx': b'',  # (pkey_xmit) program function key #1 to transmit string #2
    # 'pfxl': b'',  # (pkey_plab) Program function key #1 to type string #2 and show string #3
    # 'pln': b'',  # (plab_norm) program label #1 to show string #2
    # 'porder': b'',  # (order_of_pins) Match software bits to print-head pins
    # 'prot': b'',  # (enter_protected_mode) turn on protected mode
    # 'pulse': b'',  # (pulse) select pulse dialing
    # 'qdial': b'',  # (quick_dial) dial number #1 without checking
    # 'rbim': b'',  # (stop_bit_image) Stop printing bit image graphics
    'rc': b'\x1b8',  # (restore_cursor) restore cursor to position of last save_cursor
    # 'rcsd': b'',  # (stop_char_set_def) End definition of character set #1
    'rep': b'%p1%c\x1b[%p2%{1}%-%db',  # (repeat_char) repeat char #1 #2 times (P*)
    # 'reqmp': b'',  # (req_mouse_pos) Request mouse position
    'rev': b'\x1b[7m',  # (enter_reverse_mode) turn on reverse video mode
    # 'rf': b'',  # (reset_file) name of reset file
    # 'rfi': b'',  # (req_for_input) send next input char (for ptys)
    'ri': b'\x1bM',  # (scroll_reverse) scroll text down (P)
    'rin': b'\x1b[%p1%dT',  # (parm_rindex) scroll back #1 lines (P)
    'ritm': b'\x1b[23m',  # (exit_italics_mode) End italic mode
    # 'rlm': b'',  # (exit_leftward_mode) End left-motion mode
    'rmacs': b'\x1b(B',  # (exit_alt_charset_mode) end alternate character set (P)
    'rmam': b'\x1b[?7l',  # (exit_am_mode) turn off automatic margins
    # 'rmclk': b'',  # (remove_clock) remove clock
    'rmcup': b'\x1b[?1049l\x1b[23;0;0t',  # (exit_ca_mode) strings to end programs using cup
    # 'rmdc': b'',  # (exit_delete_mode) end delete mode
    # 'rmicm': b'',  # (exit_micro_mode) End micro-motion mode
    'rmir': b'\x1b[4l',  # (exit_insert_mode) exit insert mode
    'rmkx': b'\x1b[?1l\x1b>',  # (keypad_local) leave 'keyboard_transmit' mode
    # 'rmln': b'',  # (label_off) turn off soft labels
    'rmm': b'\x1b[?1034l',  # (meta_off) turn off meta mode
    # 'rmp': b'',  # (char_padding) like ip but when in insert mode
    # 'rmpch': b'',  # (exit_pc_charset_mode) Exit PC character display mode
    # 'rmsc': b'',  # (exit_scancode_mode) Exit PC scancode mode
    'rmso': b'\x1b[27m',  # (exit_standout_mode) exit standout mode
    'rmul': b'\x1b[24m',  # (exit_underline_mode) exit underline mode
    # 'rmxon': b'',  # (exit_xon_mode) turn off xon/xoff handshaking
    'rs1': b'\x1bc',  # (reset_1string) reset string
    'rs2': b'\x1b[!p\x1b[?3;4l\x1b[4l\x1b>',  # (reset_2string) reset string
    # 'rs3': b'',  # (reset_3string) reset string
    # 'rshm': b'',  # (exit_shadow_mode) End shadow-print mode
    # 'rsubm': b'',  # (exit_subscript_mode) End subscript mode
    # 'rsupm': b'',  # (exit_superscript_mode) End superscript mode
    # 'rum': b'',  # (exit_upward_mode) End reverse character motion
    # 'rwidm': b'',  # (exit_doublewide_mode) End double-wide mode
    # 's0ds': b'',  # (set0_des_seq) Shift to codeset 0 (EUC set 0, ASCII)
    # 's1ds': b'',  # (set1_des_seq) Shift to codeset 1
    # 's2ds': b'',  # (set2_des_seq) Shift to codeset 2
    # 's3ds': b'',  # (set3_des_seq) Shift to codeset 3
    # 'sbim': b'',  # (start_bit_image) Start printing bit image graphics
    'sc': b'\x1b7',  # (save_cursor) save current cursor position (P)
    # 'scesa': b'',  # (alt_scancode_esc) Alternate escape for scancode emulation
    # 'scesc': b'',  # (scancode_escape) Escape for scancode emulation
    # 'sclk': b'',  # (set_clock) set clock, #1 hrs #2 mins #3 secs
    # 'scp': b'',  # (set_color_pair) Set current color pair to #1
    # 'scs': b'',  # (select_char_set) Select character set, #1
    # 'scsd': b'',  # (start_char_set_def) Start character set definition #1, with #2 characters in the set
    # 'sdrfq': b'',  # (enter_draft_quality) Enter draft-quality mode
    'setab': b'\x1b[4%p1%dm',  # (set_a_background) Set background color to #1, using ANSI escape
    'setaf': b'\x1b[3%p1%dm',  # (set_a_foreground) Set foreground color to #1, using ANSI escape
    'setb': b'\x1b[4%?%p1%{1}%=%t4%e%p1%{3}%=%t6%e%p1%{4}%=%t1%e%p1%{6}%=%t3%e%p1%d%;m',  # (set_background) Set background color #1
    # 'setcolor': b'',  # (set_color_band) Change to ribbon color #1
    'setf': b'\x1b[3%?%p1%{1}%=%t4%e%p1%{3}%=%t6%e%p1%{4}%=%t1%e%p1%{6}%=%t3%e%p1%d%;m',  # (set_foreground) Set foreground color #1
    'sgr0': b'\x1b(B\x1b[m',  # (exit_attribute_mode) turn off all attributes
    'sgr': b'%?%p9%t\x1b(0%e\x1b(B%;\x1b[0%?%p6%t;1%;%?%p5%t;2%;%?%p2%t;4%;%?%p1%p3%|%t;7%;%?%p4%t;5%;%?%p7%t;8%;m',  # (set_attributes) define video attributes #1-#9 (PG9)
    'sitm': b'\x1b[3m',  # (enter_italics_mode) Enter italic mode
    # 'slines': b'',  # (set_page_length) Set page length to #1 lines
    # 'slm': b'',  # (enter_leftward_mode) Start leftward carriage motion
    'smacs': b'\x1b(0',  # (enter_alt_charset_mode) start alternate character set (P)
    'smam': b'\x1b[?7h',  # (enter_am_mode) turn on automatic margins
    'smcup': b'\x1b[?1049h\x1b[22;0;0t',  # (enter_ca_mode) string to start programs using cup
    # 'smdc': b'',  # (enter_delete_mode) enter delete mode
    # 'smgb': b'',  # (set_bottom_margin) Set bottom margin at current line
    # 'smgbp': b'',  # (set_bottom_margin_parm) Set bottom margin at line #1 or (if smgtp is not given) #2 lines from bottom
    # 'smgl': b'',  # (set_left_margin) set left soft margin at current column.     See smgl. (ML is not in BSD termcap).
    # 'smglp': b'',  # (set_left_margin_parm) Set left (right) margin at column #1
    # 'smglr': b'',  # (set_lr_margin) Set both left and right margins to #1, #2.  (ML is not in BSD termcap).
    # 'smgr': b'',  # (set_right_margin) set right soft margin at current column
    # 'smgrp': b'',  # (set_right_margin_parm) Set right margin at column #1
    # 'smgtb': b'',  # (set_tb_margin) Sets both top and bottom margins to #1, #2
    # 'smgt': b'',  # (set_top_margin) Set top margin at current line
    # 'smgtp': b'',  # (set_top_margin_parm) Set top (bottom) margin at row #1
    # 'smicm': b'',  # (enter_micro_mode) Start micro-motion mode
    'smir': b'\x1b[4h',  # (enter_insert_mode) enter insert mode
    'smkx': b'\x1b[?1h\x1b=',  # (keypad_xmit) enter 'keyboard_transmit' mode
    # 'smln': b'',  # (label_on) turn on soft labels
    'smm': b'\x1b[?1034h',  # (meta_on) turn on meta mode (8th-bit on)
    # 'smpch': b'',  # (enter_pc_charset_mode) Enter PC character display mode
    # 'smsc': b'',  # (enter_scancode_mode) Enter PC scancode mode
    'smso': b'\x1b[7m',  # (enter_standout_mode) begin standout mode
    'smul': b'\x1b[4m',  # (enter_underline_mode) begin underline mode
    # 'smxon': b'',  # (enter_xon_mode) turn on xon/xoff handshaking
    # 'snlq': b'',  # (enter_near_letter_quality) Enter NLQ mode
    # 'snrmq': b'',  # (enter_normal_quality) Enter normal-quality mode
    # 'sshm': b'',  # (enter_shadow_mode) Enter shadow-print mode
    # 'ssubm': b'',  # (enter_subscript_mode) Enter subscript mode
    # 'ssupm': b'',  # (enter_superscript_mode) Enter superscript mode
    # 'subcs': b'',  # (subscript_characters) List of subscriptable characters
    # 'sum': b'',  # (enter_upward_mode) Start upward carriage motion
    # 'supcs': b'',  # (superscript_characters) List of superscriptable characters
    # 'swidm': b'',  # (enter_doublewide_mode) Enter double-wide mode
    'tbc': b'\x1b[3g',  # (clear_all_tabs) clear all tab stops (P)
    # 'tone': b'',  # (tone) select touch tone dialing
    # 'tsl': b'',  # (to_status_line) move to status line, column #1
    # 'u0': b'',  # (user0) User string #0
    # 'u1': b'',  # (user1) User string #1
    # 'u2': b'',  # (user2) User string #2
    # 'u3': b'',  # (user3) User string #3
    # 'u4': b'',  # (user4) User string #4
    # 'u5': b'',  # (user5) User string #5
    'u6': b'\x1b[%i%d;%dR',  # (user6) User string #6 [cursor position report (equiv. to ANSI/ECMA-48 CPR)]
    'u7': b'\x1b[6n',  # (user7) User string #7 [cursor position request (equiv. to VT100/ANSI/ECMA-48 DSR 6)]
    'u8': b'\x1b[?%[;0123456789]c',  # (user8) User string #8 [terminal answerback description]
    'u9': b'\x1b[c',  # (user9) User string #9 [terminal enquire string (equiv. to ANSI/ECMA-48 DA)]
    # 'uc': b'',  # (underline_char) underline char and move past it
    'vpa': b'\x1b[%i%p1%dd',  # (row_address) vertical position #1 absolute (P)
    # 'wait': b'',  # (wait_tone) wait for dial-tone
    # 'wind': b'',  # (set_window) current window is lines #1-#2 cols #3-#4
    # 'wingo': b'',  # (goto_window) go to window #1
    # 'xoffc': b'',  # (xoff_character) XOFF character
    # 'xonc': b'',  # (xon_character) XON character
    # 'zerom': b'',  # (zero_motion) No motion for subsequent character
}
