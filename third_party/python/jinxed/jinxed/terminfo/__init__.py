"""
jinxed terminal info library

Most of this information came from the terminfo man pages, part of ncurses
More information on ncurses can be found at:
https://www.gnu.org/software/ncurses/ncurses.html

Boolean and numeric capabilities are listed here to support tigetnum() and tigetflag()
"""

# pylint: disable=wrong-spelling-in-comment

BOOL_CAPS = [
    'am',  # (auto_right_margin) terminal has automatic margins
    'bce',  # (back_color_erase) screen erased with background color
    'bw',  # (auto_left_margin) cub1 wraps from column 0 to last column
    'ccc',  # (can_change) terminal can re-define existing colors
    'chts',  # (hard_cursor) cursor is hard to see
    'cpix',  # (cpi_changes_res) changing character pitch changes resolution
    'crxm',  # (cr_cancels_micro_mode) using cr turns off micro mode
    'daisy',  # (has_print_wheel) printer needs operator to change character set
    'da',  # (memory_above) display may be retained above the screen
    'db',  # (memory_below) display may be retained below the screen
    'eo',  # (erase_overstrike) can erase overstrikes with a blank
    'eslok',  # (status_line_esc_ok) escape can be used on the status line
    'gn',  # (generic_type) generic line type
    'hc',  # (hard_copy) hardcopy terminal
    'hls',  # (hue_lightness_saturation) terminal uses only HLS color notation (Tektronix)
    'hs',  # (has_status_line) has extra status line
    'hz',  # (tilde_glitch) cannot print ~'s (Hazeltine)
    'in',  # (insert_null_glitch) insert mode distinguishes nulls
    'km',  # (has_meta_key) Has a meta key (i.e., sets 8th-bit)
    'lpix',  # (lpi_changes_res) changing line pitch changes resolution
    'mc5i',  # (prtr_silent) printer will not echo on screen
    'mir',  # (move_insert_mode) safe to move while in insert mode
    'msgr',  # (move_standout_mode) safe to move while in standout mode
    'ndscr',  # (non_dest_scroll_region) scrolling region is non-destructive
    'npc',  # (no_pad_char) pad character does not exist
    'nrrmc',  # (non_rev_rmcup) smcup does not reverse rmcup
    'nxon',  # (needs_xon_xoff) padding will not work, xon/xoff required
    'os',  # (over_strike) terminal can overstrike
    'sam',  # (semi_auto_right_margin) printing in last column causes cr
    'ul',  # (transparent_underline) underline character overstrikes
    'xenl',  # (eat_newline_glitch) newline ignored after 80 cols (concept)
    'xhpa',  # (col_addr_glitch) only positive motion for hpa/mhpa caps
    'xhp',  # (ceol_standout_glitch) standout not erased by overwriting (hp)
    'xon',  # (xon_xoff) terminal uses xon/xoff handshaking
    'xsb',  # (no_esc_ctlc) beehive (f1=escape, f2=ctrl C)
    'xt',  # (dest_tabs_magic_smso) tabs destructive, magic so char (t1061)
    'xvpa',  # (row_addr_glitch) only positive motion for vpa/mvpa caps
]

NUM_CAPS = [
    'bitwin',  # (bit_image_entwining) number of passes for each bit-image row
    'bitype',  # (bit_image_type) type of bit-image device
    'btns',  # (buttons) number of buttons on mouse
    'bufsz',  # (buffer_capacity) numbers of bytes buffered before printing
    'colors',  # (max_colors) maximum number of colors on screen
    'cols',  # (columns) number of columns in a line
    'cps',  # (print_rate) print rate in characters per second
    'it',  # (init_tabs) tabs initially every # spaces
    'lh',  # (label_height) rows in each label
    'lines',  # (lines) number of lines on screen or page
    'lm',  # (lines_of_memory) lines of memory if > line. 0 means varies
    'lw',  # (label_width) columns in each label
    'ma',  # (max_attributes) maximum combined attributes terminal can handle
    'maddr',  # (max_micro_address) maximum value in micro_..._address
    'mcs',  # (micro_col_size) character step size when in micro mode
    'mjump',  # (max_micro_jump) maximum value in parm_..._micro
    'mls',  # (micro_line_size) line step size when in micro mode
    'ncv',  # (no_color_video) video attributes that cannot be used with colors
    'nlab',  # (num_labels) number of labels on screen
    'npins',  # (number_of_pins) numbers of pins in print-head
    'orc',  # (output_res_char) horizontal resolution in units per line
    'orhi',  # (output_res_horz_inch) horizontal resolution in units per inch
    'orl',  # (output_res_line) vertical resolution in units per line
    'orvi',  # (output_res_vert_inch) vertical resolution in units per inch
    'pairs',  # (max_pairs) maximum number of color-pairs on the screen
    'pb',  # (padding_baud_rate) lowest baud rate where padding needed
    'spinh',  # (dot_horz_spacing) spacing of dots horizontally in dots per inch
    'spinv',  # (dot_vert_spacing) spacing of pins vertically in pins per inch
    'vt',  # (virtual_terminal) virtual terminal number (CB/unix)
    'widcs',  # (wide_char_size) character step size when in double wide mode
    'wnum',  # (maximum_windows) maximum number of definable windows
    'wsl',  # (width_status_line) number of columns in status line
    'xmc',  # (magic_cookie_glitch) number of blank characters left by smso or rmso
]
