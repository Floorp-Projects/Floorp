#!/usr/bin/python
# vim: set ts=4 sw=4 tw=99 et:
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
import os
import sys
import subprocess
from optparse import OptionParser

class FolderChanger:
    def __init__(self, folder):
        self.old = os.getcwd()
        self.new = folder

    def __enter__(self):
        os.chdir(self.new)

    def __exit__(self, type, value, traceback):
        os.chdir(self.old)

def MakeEnv(options, args):
    env = os.environ.copy()
    env['CC'] = options.cc.strip('"')
    env['CXX'] = options.cxx.strip('"')
    env['CFLAGS'] = ' '.join(args)
    env['CXXFLAGS'] = ' '.join(args)
    if '-m32' in args:
        env['LDFLAGS'] = '-m32'
    return env

def Make(options, args):
    subprocess.check_output(['make', '-j4'], stderr=subprocess.STDOUT, env=os.environ)

def Configure(options, args, path, extra = []):
    # Bypass normal MakeEnv since we expect the benchmark to pass -O2 automatically.
    env = MakeEnv(options, args)
    args = [path] + extra
    subprocess.check_output(args, stderr=subprocess.STDOUT, env=env)

class Box2D(object):
    def __init__(self):
        self.name = 'box2d'

    def build(self, options, args):
        env = MakeEnv(options, args)
        with FolderChanger('box2d'):
            subprocess.check_output(['make', 'clean'], stderr=subprocess.STDOUT, env=env)
            subprocess.check_output(['make'], stderr=subprocess.STDOUT, env=env)
        return os.path.join('box2d', 'run-box2d')

class LuaBinaryTrees(object):
    def __init__(self):
        self.name = 'lua_binarytrees'

    def build(self, options, args):
        env = MakeEnv(options, args)
        with FolderChanger('lua'):
            subprocess.call(['make', 'clean'], stderr=subprocess.STDOUT, env=env)
            vec = ['make', 'generic', 'MYCFLAGS=' + env['CFLAGS']]
            try:
                subprocess.check_output(vec, stderr=subprocess.STDOUT, env=env)
            except subprocess.CalledProcessError, e:
                pass # lua needs two make's, first fails
            subprocess.check_output(vec, stderr=subprocess.STDOUT, env=env)
        return os.path.join('lua', 'run-lua-binarytrees.sh')

class Bullet(object):
    def __init__(self):
        self.name = 'bullet'

    def build(self, options, args):
        extra = ['--disable-demos', '--disable-dependency-tracking']
        build_dir = 'build-bullet'

        if not os.path.exists(build_dir):
            os.mkdir(build_dir)

        with FolderChanger(build_dir):
            try:
                Make(options, args)
            except:
                Configure(options, args, os.path.join('..', 'bullet', 'configure'), extra)
                Make(options, args)

        libs = [os.path.join('src', '.libs', 'libBulletDynamics.a'),
                os.path.join('src', '.libs', 'libBulletCollision.a'),
                os.path.join('src', '.libs', 'libLinearMath.a')]

        with FolderChanger('build-bullet'):
            demo = os.path.join('..', 'bullet', 'Demos', 'Benchmarks')
            env = MakeEnv(options, args)
            argv = options.cxx.strip('"').split(' ')
            argv += args
            argv += [os.path.join(demo, 'BenchmarkDemo.cpp')]
            argv += [os.path.join(demo, 'main.cpp')]
            argv += libs
            argv += ['-I' + os.path.join(demo)]
            argv += ['-I' + os.path.join('..', 'bullet', 'src')]
            argv += ['-o', 'bullet']
            subprocess.check_output(argv, stderr=subprocess.STDOUT, env=env)
        
        return os.path.join('build-bullet', 'bullet')

class Zlib(object):
    def __init__(self):
        self.name = 'zlib'

    def build(self, options, args):
        with FolderChanger('zlib'):
            try:
                Make(options, args)
            except:
                Configure(options, args, './configure')
                Make(options, args)

            env = MakeEnv(options, args)
            argv = options.cc.strip('"').split(' ')
            argv += args
            argv += ['benchmark.c']
            argv += ['libz.a']
            argv += ['-o', 'run-zlib']
            subprocess.check_output(argv, stderr=subprocess.STDOUT, env=env)

        return os.path.join('zlib', 'run-zlib')

Benchmarks = [Box2D(),
              Bullet(),
              LuaBinaryTrees(),
              Zlib()
             ]

def BenchmarkNative(options, args):
    for benchmark in Benchmarks:
        binary = benchmark.build(options, args)

        # factor 1: loadtime
        with open('/dev/null', 'w') as fp:
            before = os.times()[4]
            subprocess.check_call([binary, '1'], stdout=fp)
            after = os.times()[4]
            print(benchmark.name + '-loadtime - ' + str((after - before) * 1000))

        # factor 3: throughput
        with open('/dev/null', 'w') as fp:
            before = os.times()[4]
            subprocess.check_call([binary, '3'], stdout=fp)
            after = os.times()[4]
            print(benchmark.name + '-throughput - ' + str((after - before) * 1000))


def Exec(vec):
    o = subprocess.check_output(vec, stderr=subprocess.STDOUT, env=os.environ)
    return o.decode("utf-8")

def BenchmarkJavaScript(options, args):
    for benchmark in Benchmarks:

        # factor 1: loadtime
        argv = [] + args
        argv.extend(['run.js', '--', benchmark.name + '.js', '1'])
        try:
            t = Exec(argv)
            t = t.strip()
            print(benchmark.name + '-loadtime - ' + t)
        except Exception as e:
            print('Exception when running ' + benchmark.name + ': ' + str(e))

        # factor 3: throughput
        argv = [] + args
        argv.extend(['run.js', '--', benchmark.name + '.js', '3'])
        try:
            t = Exec(argv)
            t = t.strip()
            print(benchmark.name + '-throughput - ' + t)
        except Exception as e:
            print('Exception when running ' + benchmark.name + ': ' + str(e))

def main(argv):
    parser = OptionParser()
    parser.add_option('--native', action='store_true', dest='native')
    parser.add_option('--cc', type='string', dest='cc', default='cc')
    parser.add_option('--cxx', type='string', dest='cxx', default='c++')
    (options, args) = parser.parse_args(argv)

    args = args[1:]

    if len(args) < 1 and not options.native:
        print("Usage: ")
        print("  --native [--cc=] [--cxx=] [-- flags]")
        print("  <shell> [-- options]")

    if options.native:
        BenchmarkNative(options, args)
    else:
        BenchmarkJavaScript(options, args)

if __name__ == "__main__":
    main(sys.argv)

