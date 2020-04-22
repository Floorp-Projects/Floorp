#!/usr/bin/env python3

""" Extract opcodes from C++ header.

To use, pipe the output of this command to your clipboard,
then paste it into opcode.rs.
"""

import argparse
import os
import re
import shutil
import sys

parser = argparse.ArgumentParser(description='Update opcode.rs')
parser.add_argument('PATH_TO_MOZILLA_CENTRAL',
                    help='Path to mozilla-central')
parser.add_argument('PATH_TO_JSPARAGUS',
                    help='Path to jsparagus')
args = parser.parse_args()


def ensure_input_files(files):
    paths = dict()
    for (parent, name) in files:
        path = os.path.join(parent, name)

        if not os.path.exists(path):
            print('{} does not exist'.format(path),
                  file=sys.stderr)
            sys.exit(1)

        paths[name] = path

    return paths


js_dir = os.path.join(args.PATH_TO_MOZILLA_CENTRAL, 'js')
frontend_dir = os.path.join(js_dir, 'src', 'frontend')
vm_dir = os.path.join(js_dir, 'src', 'vm')
public_dir = os.path.join(js_dir, 'public')

input_paths = ensure_input_files([
    (frontend_dir, 'SourceNotes.h'),
    (public_dir, 'Symbol.h'),
    (vm_dir, 'AsyncFunctionResolveKind.h'),
    (vm_dir, 'BytecodeFormatFlags.h'),
    (vm_dir, 'CheckIsCallableKind.h'),
    (vm_dir, 'CheckIsObjectKind.h'),
    (vm_dir, 'FunctionPrefixKind.h'),
    (vm_dir, 'GeneratorResumeKind.h'),
    (vm_dir, 'Opcodes.h'),
    (vm_dir, 'ThrowMsgKind.h'),
    (vm_dir, 'TryNoteKind.h'),
])

opcode_dest_path = os.path.join(args.PATH_TO_JSPARAGUS,
                                'crates', 'emitter', 'src', 'opcode.rs')
if not os.path.exists(opcode_dest_path):
    print('{} does not exist'.format(opcode_dest_path),
          file=sys.stderr)
    sys.exit(1)

emitter_dest_path = os.path.join(args.PATH_TO_JSPARAGUS,
                                 'crates', 'emitter', 'src', 'emitter.rs')
if not os.path.exists(emitter_dest_path):
    print('{} does not exist'.format(emitter_dest_path),
          file=sys.stderr)
    sys.exit(1)

copy_dir = os.path.join(args.PATH_TO_JSPARAGUS,
                        'crates/emitter/src/copy')
if not os.path.exists(copy_dir):
    os.makedirs(copy_dir)


def extract_opcodes(paths):
    opcodes = []

    with open(paths['Opcodes.h'], 'r') as f:
        for line in f:
            line = line.strip()
            if line.startswith('MACRO(') and ',' in line:
                line = line[5:]
                if line.endswith(' \\'):
                    line = line[:-2]
                assert line.endswith(')')
                opcodes.append((" " * 16) + line + ",")

    return opcodes


def extract_flags(paths):
    pat = re.compile(r'(JOF_[A-Z0-9_]+)\s=\s([^,]+),\s*/\*\s+(.*)\s+\*/')

    flags = []

    with open(paths['BytecodeFormatFlags.h'], 'r') as f:
        for line in f:
            m = pat.search(line)
            if not m:
                continue

            name = m.group(1)
            value = m.group(2)
            comment = m.group(3)

            if name == 'JOF_MODEMASK':
                continue

            flags.append({
                'name': name,
                'value': value,
                'comment': comment,
            })

    return flags


size_types = {
    'int8_t': 'i8',
    'uint8_t': 'u8',
    'uint16_t': 'u16',
    'uint24_t': 'u24',
    'int32_t': 'i32',
    'uint32_t': 'u32',
}


def extract_types(paths):
    types = dict()

    def extract_enum(ty):
        variants_pat = re.compile(
            r'enum class ' + ty + r' : ([A-Za-z0-9_]+) \{([^}]+)\}', re.M)

        name = '{}.h'.format(ty)
        with open(paths[name], 'r') as f:
            content = f.read()

        m = variants_pat.search(content)
        size_type = m.group(1)
        if size_type not in size_types:
            print('{} is not supported'.format(size_type),
                  file=sys.stderr)
            sys.exit(1)

        size = size_types[size_type]
        variants = list(map(lambda s: s.strip(), m.group(2).split(',')))

        types[ty] = {
            'size': size,
            'variants': variants
        }

    def extract_symbols():
        pat = re.compile(r'MACRO\((.+)\)')

        ty = 'SymbolCode'
        variants = []

        found = False
        state = 'before'
        with open(paths['Symbol.h'], 'r') as f:
            for line in f:
                if 'enum class SymbolCode : uint32_t {' in line:
                    found = True

                if state == 'before':
                    if 'JS_FOR_EACH_WELL_KNOWN_SYMBOL' in line:
                        state = 'macro'
                elif state == 'macro':
                    m = pat.search(line)
                    if m:
                        sym = m.group(1)
                        sym = sym[0].upper() + sym[1:]
                        variants.append(sym)

                    if not line.strip().endswith('\\'):
                        state = 'after'

        if not found:
            print('SymbolCode : uint32_t is not found',
                  file=sys.stderr)
            sys.exit(1)

        types[ty] = {
            'size': 'u32',
            'variants': variants
        }

    def extract_source_notes():
        pat = re.compile(r'M\((.+),(.+),(.+)\)')

        ty = 'SrcNoteType'
        variants = []

        found = False
        state = 'before'
        with open(paths['SourceNotes.h'], 'r') as f:
            for line in f:
                if 'enum class SrcNoteType : uint8_t {' in line:
                    found = True

                if state == 'before':
                    if 'FOR_EACH_SRC_NOTE_TYPE' in line:
                        state = 'macro'
                elif state == 'macro':
                    m = pat.search(line)
                    if m:
                        variants.append(m.group(1))

                    if not line.strip().endswith('\\'):
                        state = 'after'

        if not found:
            print('SrcNoteType : uint8_t is not found',
                  file=sys.stderr)
            sys.exit(1)

        types[ty] = {
            'size': 'u8',
            'variants': variants
        }

    extract_enum('AsyncFunctionResolveKind')
    extract_enum('CheckIsCallableKind')
    extract_enum('CheckIsObjectKind')
    extract_enum('FunctionPrefixKind')
    extract_enum('GeneratorResumeKind')
    extract_enum('ThrowMsgKind')
    extract_enum('TryNoteKind')

    extract_symbols()

    extract_source_notes()

    return types


def format_opcodes(out, opcodes):
    for opcode in opcodes:
        out.write('{}\n'.format(opcode))


def format_flags(out, flags):
    for flag in flags:
        out.write('/// {}\n'.format(flag['comment']))
        out.write('const {}: u32 = {};\n'.format(flag['name'], flag['value']))
        out.write('\n')


def update_opcode(path, opcodes, flags):
    tmppath = '{}.tmp'.format(path)

    with open(path, 'r') as in_f:
        with open(tmppath, 'w') as out_f:
            state = 'normal'
            for line in in_f:
                if '@@@@ BEGIN OPCODES @@@@' in line:
                    state = 'opcodes'
                    out_f.write(line)
                    format_opcodes(out_f, opcodes)
                elif '@@@@ END OPCODES @@@@' in line:
                    assert state == 'opcodes'
                    state = 'normal'
                    out_f.write(line)
                elif '@@@@ BEGIN FLAGS @@@@' in line:
                    state = 'flags'
                    out_f.write(line)
                    format_flags(out_f, flags)
                elif '@@@@ END FLAGS @@@@' in line:
                    assert state == 'flags'
                    state = 'normal'
                    out_f.write(line)
                elif state == 'normal':
                    out_f.write(line)
            assert state == 'normal'

    os.replace(tmppath, path)


def to_snake_case(s):
    return re.sub(r'(?<!^)(?=[A-Z])', '_', s).lower()


def parse_operands(opcode):
    params = []

    copied_types = [
        'AsyncFunctionResolveKind',
        'CheckIsCallableKind',
        'CheckIsObjectKind',
        'FunctionPrefixKind',
        'GeneratorResumeKind',
        'ThrowMsgKind',
    ]

    for operand in opcode.operands_array:
        tmp = operand.split(' ')
        ty = tmp[0]
        name = to_snake_case(tmp[1])

        if ty in size_types:
            ty = size_types[ty]
        elif ty == 'double':
            ty = 'f64'
        elif ty in copied_types:
            pass
        else:
            print('Unspported operand type {}'.format(ty),
                  file=sys.stderr)
            sys.exit(1)

        if 'JOF_ATOM' in opcode.format_:
            assert ty == 'u32'
            ty = 'ScriptAtomSetIndex'

        if 'JOF_ICINDEX' in opcode.format_ or 'JOF_LOOPHEAD' in opcode.format_:
            if ty == 'u32' and name == 'ic_index':
                ty = 'IcIndex'
                name = ''
            else:
                assert 'JOF_LOOPHEAD' in opcode.format_ and name == 'depth_hint'

        # FIXME: Stronger typing for Opcode::CheckIsObj kind parameter.

        params.append((ty, name))

    return params


def generate_emit_types(out_f, types):
    for ty in types:
        variants = []
        for i, variant in enumerate(types[ty]['variants']):
            variants.append("""\
    {} = {},
""".format(variant, i))

        out_f.write("""\
pub enum {} {{
{}}}

""".format(ty, ''.join(variants)))


def generate_emit_methods(out_f, opcodes, types):
    for op, opcode in opcodes.items():
        if op in ['True', 'False']:
            # done by `boolean` method
            continue

        if op in ['Void', 'Pos', 'Neg', 'Pos', 'BitNot', 'Not']:
            # done by `emit_unary_op` method
            continue

        if op in ['BitOr', 'BitXor', 'BitAnd',
                  'Eq', 'Ne', 'StrictEq', 'StrictNe',
                  'Lt', 'Gt', 'Le', 'Ge',
                  'Instanceof', 'In',
                  'Lsh', 'Rsh', 'Ursh',
                  'Add', 'Sub', 'Mul', 'Div', 'Mod', 'Pow']:
            # done by `emit_binary_op` method
            continue

        if op == 'TableSwitch':
            # Unsupported
            continue

        op_snake = opcode.op_snake
        if op_snake in ['yield', 'await']:
            op_snake = '{}_'.format(op_snake)

        params = parse_operands(opcode)

        method = 'emit_op'
        extra_args = ''

        if 'JOF_ARGC' in opcode.format_:
            assert int(opcode.nuses) == -1
            method = 'emit_argc_op'
            extra_args = ', {}'.format(params[0][1])
        elif op == 'PopN':
            assert int(opcode.nuses) == -1
            method = 'emit_pop_n_op'
            extra_args = ', {}'.format(params[0][1])
        elif op == 'RegExp':
            assert len(params) == 1
            assert params[0][0] == 'u32'
            params[0] = ('GCThingIndex', params[0][1])
        elif 'JOF_JUMP' in opcode.format_:
            assert params[0][0] == 'i32'
            params[0] = ('BytecodeOffsetDiff', params[0][1])
        else:
            assert int(opcode.nuses) != -1

        assert int(opcode.ndefs) != -1

        method_params = []
        for ty, name in params:
            if ty == 'IcIndex':
                continue
            method_params.append(', {}: {}'.format(name, ty))

        out_f.write("""\
    pub fn {op_snake}(&mut self{method_params}) {{
        self.{method}(Opcode::{op}{extra_args});
""".format(method=method,
           method_params=''.join(method_params),
           extra_args=extra_args,
           op=op,
           op_snake=op_snake))

        for (ty, name) in params:
            if ty in types:
                size_ty = types[ty]['size']
                out_f.write("""\
        self.write_{size_ty}({name} as {size_ty});
""".format(size_ty=size_ty, name=name))
            else:
                out_f.write("""\
        self.write_{ty}({name});
""".format(ty=to_snake_case(ty), name=name))

        out_f.write("""\
    }

""")


def update_emit(path, types):
    sys.path.append(vm_dir)
    from jsopcode import get_opcodes

    _, opcodes = get_opcodes(args.PATH_TO_MOZILLA_CENTRAL)

    tmppath = '{}.tmp'.format(path)

    with open(path, 'r') as in_f:
        with open(tmppath, 'w') as out_f:
            state = 'normal'
            for line in in_f:
                if '@@@@ BEGIN METHODS @@@@' in line:
                    state = 'methods'
                    out_f.write(line)
                    generate_emit_methods(out_f, opcodes, types)
                elif '@@@@ END METHODS @@@@' in line:
                    assert state == 'methods'
                    state = 'normal'
                    out_f.write(line)
                elif '@@@@ BEGIN TYPES @@@@' in line:
                    state = 'types'
                    out_f.write(line)
                    generate_emit_types(out_f, types)
                elif '@@@@ END TYPES @@@@' in line:
                    assert state == 'types'
                    state = 'normal'
                    out_f.write(line)
                elif state == 'normal':
                    out_f.write(line)
            assert state == 'normal'

    os.replace(tmppath, path)


def copy_input(paths):
    for name, path in paths.items():
        shutil.copyfile(path,
                        os.path.join(copy_dir, name))


opcodes = extract_opcodes(input_paths)
flags = extract_flags(input_paths)
types = extract_types(input_paths)

update_opcode(opcode_dest_path, opcodes, flags)
update_emit(emitter_dest_path, types)

copy_input(input_paths)
