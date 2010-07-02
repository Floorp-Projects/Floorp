# ***** BEGIN LICENSE BLOCK *****
# Version: MPL 1.1/GPL 2.0/LGPL 2.1
#
# The contents of this file are subject to the Mozilla Public License Version
# 1.1 (the "License"); you may not use this file except in compliance with
# the License. You may obtain a copy of the License at
# http://www.mozilla.org/MPL/
#
# Software distributed under the License is distributed on an "AS IS" basis,
# WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
# for the specific language governing rights and limitations under the
# License.
#
# The Original Code is Mozilla code.
#
# The Initial Developer of the Original Code is the Mozilla Foundation.
# Portions created by the Initial Developer are Copyright (C) 2010
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#  Robert O'Callahan <robert@ocallahan.org>
#
# Alternatively, the contents of this file may be used under the terms of
# either the GNU General Public License Version 2 or later (the "GPL"), or
# the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
# in which case the provisions of the GPL or the LGPL are applicable instead
# of those above. If you wish to allow use of your version of this file only
# under the terms of either the GPL or the LGPL, and not to allow others to
# use your version of this file under the terms of the MPL, indicate your
# decision by deleting the provisions above and replace them with the notice
# and other provisions required by the GPL or the LGPL. If you do not delete
# the provisions above, a recipient may use your version of this file under
# the terms of any one of the MPL, the GPL or the LGPL.
#
# ***** END LICENSE BLOCK *****

# A crude YASM-to-MASM translation script for the VP8 code

import sys, os.path, re

known_extern_types = {
  'rand': 'PROC',
  'vp8_bilinear_filters_mmx': 'DWORD'
}

comment_re = re.compile(r'(;.*)$')
global_re = re.compile(r'^\s*global ([\w()]+)')
extern_re = re.compile(r'^extern (sym\((\w+)\))')
movd_from_mem_re = re.compile(r'\b(movd\s+x?mm\d,\s*)(\[[^]]+\]|arg\(\d+\))')
movd_to_mem_re = re.compile(r'\b(movd\s*)(\[[^]]+\],\s*x?mm\d)')
label_re = re.compile(r'^\s*([\w()]+):')
times_hex_re = re.compile(r'^\s*times\s+(\d+)\s+(db|dw|dd)\s+0x([0-9a-fA-F]+)')
times_dec_re = re.compile(r'^\s*times\s+(\d+)\s+(db|dw|dd)\s+(-?\d+)')
mem_define_re = re.compile(r'^\s*%define\s*(\w+)\s*(\[[^]]+\])')
num_define_re = re.compile(r'^\s*%define\s*(\w+)\s*(\d+)')
preproc_re = re.compile(r'^\s*%(if|else|endif|elif)\b(.*)$')
ifidn_re = re.compile(r'^\s*%ifidn\b(.*)$')
undef_re = re.compile(r'%undef\s')
size_arg_re = re.compile(r'\b(byte|word|dword)\s+(arg\(\d+\)|\[[^]]+\])')
hex_re = re.compile(r'\b0x([0-9A-Fa-f]+)')

and_expr_re = re.compile(r'^(.*)&&(.*)$')
equal_expr_re = re.compile(r'^(.*)=(.*)$')
section_text_re = re.compile(r'^\s*section .text')

in_data = False
data_label = ''

def translate_expr(s):
    if and_expr_re.search(s):
        match = and_expr_re.search(s)
        return '(' + translate_expr(match.group(1)) + ' and ' + translate_expr(match.group(2)) + ')'
    if equal_expr_re.search(s):
        match = equal_expr_re.search(s)
        return '(' + translate_expr(match.group(1)) + ' eq ' + translate_expr(match.group(2)) + ')'
    return s

def type_of_extern(id):
    if id in known_extern_types:
        return known_extern_types[id]
    return 'ABS'

def translate(s):
    # Fix include, get rid of quotes and load the special hand-coded win32 preamble
    if s == '%include "vpx_ports/x86_abi_support.asm"':
        return 'include vpx_ports/x86_abi_support_win32.asm'

    if s == '%include "x86_abi_support.asm"':
        return 'include x86_abi_support_win32.asm'        
        
    # Replace 'global' with 'public'
    if global_re.search(s):
        return global_re.sub('public \\1', s)

    # Add types to externs
    if extern_re.search(s):
        match = extern_re.search(s)
        return extern_re.sub('extern \\1:' + type_of_extern(match.group(2)), s)

    # Replace %define with textequ/equ
    if mem_define_re.search(s):
        return mem_define_re.sub('\\1 textequ <\\2>', s)
    if num_define_re.search(s):
        return num_define_re.sub('\\1 equ \\2', s)

    # Translate conditional compilation directives
    if preproc_re.search(s):
        match = preproc_re.search(s)
        if match.group(1) == 'elif':
            return 'elseif ' + translate_expr(match.group(2))
        return match.group(1) + translate_expr(match.group(2))

    if ifidn_re.search(s):
        return 'if 0'

    if section_text_re.search(s):
        return '.code';
        
    # Remove undefs
    if undef_re.search(s):
        return ''

    # Fix 'movd' to add 'dword ptr' to its memory argument
    if movd_from_mem_re.search(s):
        return movd_from_mem_re.sub('\\1dword ptr \\2', s)
    if movd_to_mem_re.search(s):
        return movd_to_mem_re.sub('\\1dword ptr \\2', s)

    # Replace byte/word/dword with '... ptr' before 'arg(n)' or memory expressions
    if size_arg_re.search(s):
        return size_arg_re.sub('\\1 ptr \\2', s)

    # Translate data declarations, in particular we need to give data
    # labels the correct type
    global data_label
    if in_data and label_re.search(s):
        data_label = label_re.search(s).group(1)
        return ''
    if in_data and times_hex_re.search(s):
        match = times_hex_re.search(s)
        if (match.group(1) == '8' and match.group(2) == 'db') or \
           (match.group(1) == '4' and match.group(2) == 'dw'):
            # make labels for 8-byte quantities have type 'qword'
            print data_label + ' equ this qword'
            data_label = ''
        if match.group(1) == '16' and match.group(2) == 'db':
            # make labels for 16-byte quantities have type 'xmmword'
            print data_label + ' equ this xmmword'
            data_label = ''
        s = times_hex_re.sub(data_label + ' \\2 \\1 dup (0\\3h)', s)
        data_label = ''
        return s
    if in_data and times_dec_re.search(s):
        s = times_dec_re.sub(data_label + ' \\2 \\1 dup (\\3)', s)
        data_label = ''
        return s

    # Replace 0xABC with ABCh
    if hex_re.search(s):
        return hex_re.sub('0\\1h', s)

    return s

while 1:
    try:
        s = raw_input()
    except EOFError:
        break
    if s == 'SECTION_RODATA':
        in_data = True
    print translate(s)

print "end"
