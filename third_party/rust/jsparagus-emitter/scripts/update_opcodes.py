#!/usr/bin/env python3

""" Extract opcodes from C++ header.

To use, pipe the output of this command to your clipboard,
then paste it into opcode.rs.
"""

import argparse
import os
import re
import shutil
import subprocess
import sys
from textwrap import dedent

parser = argparse.ArgumentParser(description='Update opcode.rs')
parser.add_argument('PATH_TO_MOZILLA_CENTRAL',
                    help='Path to mozilla-central')
parser.add_argument('PATH_TO_JSPARAGUS',
                    help='Path to jsparagus')
args = parser.parse_args()


def ensure_exists(path):
    if not os.path.exists(path):
        print(f'{path} does not exist', file=sys.stderr)
        sys.exit(1)


def ensure_input_files(files):
    paths = {}
    for (parent, name) in files:
        path = os.path.join(parent, name)
        ensure_exists(path)
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
    (vm_dir, 'FunctionFlags.h'),
    (vm_dir, 'FunctionPrefixKind.h'),
    (vm_dir, 'GeneratorAndAsyncKind.h'),
    (vm_dir, 'GeneratorResumeKind.h'),
    (vm_dir, 'Opcodes.h'),
    (vm_dir, 'ThrowMsgKind.h'),
    (vm_dir, 'StencilEnums.h'),
])


def get_emitter_source_path(name):
    path = os.path.join(args.PATH_TO_JSPARAGUS,
                        'crates', 'emitter', 'src', name)
    ensure_exists(path)
    return path


opcode_dest_path = get_emitter_source_path('opcode.rs')
emitter_dest_path = get_emitter_source_path('emitter.rs')
function_dest_path = get_emitter_source_path('function.rs')

copy_dir = os.path.join(args.PATH_TO_JSPARAGUS,
                        'crates', 'emitter', 'src', 'copy')
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


def extract_opcode_flags(paths):
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


def filter_enum_body(body):
    block_comment_pat = re.compile(r'/\*.+?\*/', re.M)
    line_comment_pat = re.compile(r'//.*')
    space_pat = re.compile(r'\s*')

    result = ''
    for line in block_comment_pat.sub('', body).split('\n'):
        line = line_comment_pat.sub('', line)
        line = space_pat.sub('', line)
        result += line

    return result


def extract_function_flags(paths):
    ty = 'Flags'

    variants_pat = re.compile(
        r'enum(?:\s+class)?\s*' + ty + r'\s*:\s*([A-Za-z0-9_]+)\s*\{([^}]+)\}', re.M)
    simple_init_pat = re.compile(r'^([A-Za-z0-9_]+)=((:?0x)?[A-Fa-f0-9+]+)$')
    bits_init_pat = re.compile(r'^([A-Za-z0-9_]+)=(\d+)<<(\d+)$')
    kind_init_pat = re.compile(r'^([A-Za-z0-9_]+)=([A-Za-z0-9_]+)<<([A-Za-z0-9_]+)$')
    combined_init_pat = re.compile(r'^([A-Za-z0-9_]+)=([A-Za-z0-9_]+(\|[A-Za-z0-9_]+)*)$')

    with open(paths['FunctionFlags.h'], 'r') as f:
        content = f.read()

    m = variants_pat.search(content)
    assert m, f'enum {ty} is not found'

    size_type = m.group(1)
    body = m.group(2)

    assert size_type == 'uint16_t'

    body = filter_enum_body(body)

    function_flags = []

    for variant in body.split(','):
        m = simple_init_pat.search(variant)
        if m:
            name = m.group(1)
            value = m.group(2)

            function_flags.append((name, value))
            continue

        m = bits_init_pat.search(variant)
        if m:
            name = m.group(1)
            bits = m.group(2)
            shift = m.group(3)

            value = f'{bits} << {shift}'

            function_flags.append((name, value))
            continue

        m = kind_init_pat.search(variant)
        if m:
            name = m.group(1)
            bits = m.group(2)
            shift = m.group(3)

            value = f'(FunctionKind::{bits} as u16) << {shift}'

            function_flags.append((name, value))
            continue

        m = combined_init_pat.search(variant)
        if m:
            name = m.group(1)
            value = m.group(2)

            function_flags.append((name, value))
            continue

        raise Exception(f'unhandled variant {variant}')

    return function_flags


size_types = {
    'bool': 'bool',
    'int8_t': 'i8',
    'uint8_t': 'u8',
    'uint16_t': 'u16',
    'uint24_t': 'u24',
    'int32_t': 'i32',
    'uint32_t': 'u32',
}


def extract_enum(types, paths, ty, filename=None):
    variants_pat = re.compile(
        r'enum(?:\s+class)?\s*' + ty + r'\s*:\s*([A-Za-z0-9_]+)\s*\{([^}]+)\}', re.M)
    init_pat = re.compile(r'(.+)=(\d+)')

    if not filename:
        filename = f'{ty}.h'
    with open(paths[filename], 'r') as f:
        content = f.read()

    m = variants_pat.search(content)
    assert m, f'enum {ty} is not found'

    size_type = m.group(1)
    body = m.group(2)

    if size_type not in size_types:
        print(f'{size_types} is not supported', file=sys.stderr)
        sys.exit(1)

    size = size_types[size_type]

    body = filter_enum_body(body)

    variants = []
    i = 0
    for variant in body.split(','):
        m = init_pat.search(variant)
        if m:
            i = int(m.group(2))
            variants.append((m.group(1), i))
        else:
            variants.append((variant, i))
        i += 1

    types[ty] = {
        'size': size,
        'variants': variants
    }


def extract_types(paths):
    types = {}

    def extract_symbols():
        pat = re.compile(r'MACRO\((.+)\)')

        ty = 'SymbolCode'
        variants = []
        i = 0

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
                        variants.append((sym, i))
                        i += 1

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
        i = 0

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
                        variants.append((m.group(1), i))
                        i += 1

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

    extract_enum(types, paths, 'AsyncFunctionResolveKind')
    extract_enum(types, paths, 'CheckIsCallableKind')
    extract_enum(types, paths, 'CheckIsObjectKind')
    extract_enum(types, paths, 'FunctionPrefixKind')
    extract_enum(types, paths, 'GeneratorResumeKind')
    extract_enum(types, paths, 'ThrowMsgKind')
    extract_enum(types, paths, 'TryNoteKind', 'StencilEnums.h')

    extract_symbols()

    extract_source_notes()

    return types


def extract_function_types(paths):
    types = {}

    extract_enum(types, paths, 'FunctionKind', filename='FunctionFlags.h')
    extract_enum(types, paths, 'GeneratorKind',
                 filename='GeneratorAndAsyncKind.h')
    extract_enum(types, paths, 'FunctionAsyncKind',
                 filename='GeneratorAndAsyncKind.h')

    return types


def format_opcodes(out, opcodes):
    for opcode in opcodes:
        out.write(f'{opcode}\n')


def format_opcode_flags(out, flags):
    for flag in flags:
        out.write(dedent(f"""\
        /// {flag['comment']}
        const {flag['name']}: u32 = {flag['value']};

        """))


def rustfmt(path):
    subprocess.run(['rustfmt', path], check=True)


def update_opcode(path, opcodes, flags):
    tmppath = f'{path}.tmp'

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
                    format_opcode_flags(out_f, flags)
                elif '@@@@ END FLAGS @@@@' in line:
                    assert state == 'flags'
                    state = 'normal'
                    out_f.write(line)
                elif state == 'normal':
                    out_f.write(line)
            assert state == 'normal'

    os.replace(tmppath, path)
    rustfmt(path)


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
            print(f'Unspported operand type {ty}', file=sys.stderr)
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
        for variant, i in types[ty]['variants']:
            variants.append(dedent(f"""\
            {variant} = {i},
            """))

        out_f.write(dedent(f"""\
        #[derive(Debug)]
        pub enum {ty} {{
        {''.join(variants)}}}

        """))


def format_function_flags(out_f, function_flags):
    for name, value in function_flags:
        out_f.write(dedent(f"""\
        #[allow(dead_code)]
        const {name} : u16 = {value};
        """))


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
            op_snake = f'{op_snake}_'

        params = parse_operands(opcode)

        method = 'emit_op'
        extra_args = ''

        if 'JOF_ARGC' in opcode.format_:
            assert int(opcode.nuses) == -1
            method = 'emit_argc_op'
            extra_args = f', {params[0][1]}'
        elif op == 'PopN':
            assert int(opcode.nuses) == -1
            method = 'emit_pop_n_op'
            extra_args = f', {params[0][1]}'
        elif op == 'RegExp':
            assert len(params) == 1
            assert params[0][0] == 'u32'
            params[0] = ('GCThingIndex', params[0][1])
        elif 'JOF_OBJECT' in opcode.format_:
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
            method_params.append(f', {name}: {ty}')

        out_f.write(dedent(f"""\
        pub fn {op_snake}(&mut self{''.join(method_params)}) {{
           self.{method}(Opcode::{op}{extra_args});
        """))

        for (ty, name) in params:
            if ty in types:
                size_ty = types[ty]['size']
                out_f.write(dedent(f"""\
                self.write_{size_ty}({name} as {size_ty});
                """))
            else:
                out_f.write(dedent(f"""\
                self.write_{to_snake_case(ty)}({name});
                """))

        out_f.write(dedent(f"""\
        }}

        """))


def update_emit(path, types):
    sys.path.append(vm_dir)
    from jsopcode import get_opcodes

    _, opcodes = get_opcodes(args.PATH_TO_MOZILLA_CENTRAL)

    tmppath = f'{path}.tmp'

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
    rustfmt(path)


def update_function(path, types, flags):
    sys.path.append(vm_dir)
    from jsopcode import get_opcodes

    _, opcodes = get_opcodes(args.PATH_TO_MOZILLA_CENTRAL)

    tmppath = f'{path}.tmp'

    with open(path, 'r') as in_f:
        with open(tmppath, 'w') as out_f:
            state = 'normal'
            for line in in_f:
                if '@@@@ BEGIN TYPES @@@@' in line:
                    state = 'types'
                    out_f.write(line)
                    generate_emit_types(out_f, types)
                    format_function_flags(out_f, flags)
                elif '@@@@ END TYPES @@@@' in line:
                    assert state == 'types'
                    state = 'normal'
                    out_f.write(line)
                elif state == 'normal':
                    out_f.write(line)
            assert state == 'normal'

    os.replace(tmppath, path)
    rustfmt(path)


def copy_input(paths):
    for name, path in paths.items():
        shutil.copyfile(path,
                        os.path.join(copy_dir, name))


opcodes = extract_opcodes(input_paths)
opcode_flags = extract_opcode_flags(input_paths)
types = extract_types(input_paths)

function_flags = extract_function_flags(input_paths)
function_types = extract_function_types(input_paths)

update_opcode(opcode_dest_path, opcodes, opcode_flags)
update_emit(emitter_dest_path, types)

update_function(function_dest_path, function_types, function_flags)

copy_input(input_paths)
