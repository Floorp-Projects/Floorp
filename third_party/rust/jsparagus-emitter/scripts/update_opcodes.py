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

vm_dir = os.path.join(args.PATH_TO_MOZILLA_CENTRAL, 'js', 'src', 'vm')
if not os.path.exists(vm_dir):
    print('{} does not exist'.format(vm_dir),
          file=sys.stderr)
    sys.exit(1)

opcodes_path = os.path.join(vm_dir, 'Opcodes.h')
if not os.path.exists(opcodes_path):
    print('{} does not exist'.format(opcodes_path),
          file=sys.stderr)
    sys.exit(1)

util_path = os.path.join(vm_dir, 'BytecodeUtil.h')
if not os.path.exists(util_path):
    print('{} does not exist'.format(util_path),
          file=sys.stderr)
    sys.exit(1)

opcode_dest_path = os.path.join(args.PATH_TO_JSPARAGUS,
                                'crates/emitter/src/opcode.rs')
if not os.path.exists(opcode_dest_path):
    print('{} does not exist'.format(opcode_dest_path),
          file=sys.stderr)
    sys.exit(1)

emitter_dest_path = os.path.join(args.PATH_TO_JSPARAGUS,
                                 'crates/emitter/src/emitter.rs')
if not os.path.exists(emitter_dest_path):
    print('{} does not exist'.format(emitter_dest_path),
          file=sys.stderr)
    sys.exit(1)

copy_dir = os.path.join(args.PATH_TO_JSPARAGUS,
                        'crates/emitter/src/copy')
if not os.path.exists(copy_dir):
    os.makedirs(copy_dir)


def extract_opcodes(path):
    opcodes = []

    with open(path, 'r') as f:
        for line in f:
            line = line.strip()
            if line.startswith('MACRO(') and ',' in line:
                line = line[5:]
                if line.endswith(' \\'):
                    line = line[:-2]
                assert line.endswith(')')
                opcodes.append((" " * 16) + line + ",")

    return opcodes


def extract_flags(path):
    pat = re.compile(r'(JOF_[A-Z0-9_]+)\s=\s([^,]+),\s*/\*\s+(.*)\s+\*/')

    flags = []

    with open(path, 'r') as f:
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

    for operand in opcode.operands_array:
        tmp = operand.split(' ')
        ty = tmp[0]
        name = to_snake_case(tmp[1])

        if ty == 'int8_t':
            ty = 'i8'
        elif ty == 'uint8_t':
            ty = 'u8'
        elif ty == 'uint16_t':
            ty = 'u16'
        elif ty == 'uint24_t':
            ty = 'u24'
        elif ty == 'int32_t':
            ty = 'i32'
        elif ty == 'uint32_t':
            ty = 'u32'
        elif ty == 'GeneratorResumeKind':
            ty = 'ResumeKind'
        elif ty == 'double':
            ty = 'f64'
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


def generate_emit_methods(out_f, opcodes):
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
            out_f.write("""\
        self.write_{}({});
""".format(to_snake_case(ty), name))

        out_f.write("""\
    }

""")


def update_emit(path):
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
                    generate_emit_methods(out_f, opcodes)
                elif '@@@@ END METHODS @@@@' in line:
                    assert state == 'methods'
                    state = 'normal'
                    out_f.write(line)
                elif state == 'normal':
                    out_f.write(line)
            assert state == 'normal'

    os.replace(tmppath, path)


opcodes = extract_opcodes(opcodes_path)
flags = extract_flags(util_path)

update_opcode(opcode_dest_path, opcodes, flags)
update_emit(emitter_dest_path)

shutil.copyfile(opcodes_path,
                os.path.join(copy_dir, os.path.basename(opcodes_path)))
shutil.copyfile(util_path,
                os.path.join(copy_dir, os.path.basename(util_path)))
