# Copyright © 2018, VideoLAN and dav1d authors
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
#
# 1. Redistributions of source code must retain the above copyright notice, this
#    list of conditions and the following disclaimer.
#
# 2. Redistributions in binary form must reproduce the above copyright notice,
#    this list of conditions and the following disclaimer in the documentation
#    and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
# ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
# (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
# LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
# ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
# SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#
# Build definition for the dav1d tests
#

# Leave subdir if tests are disabled
if not get_option('build_tests')
    subdir_done()
endif

libdav1d_nasm_objs_if_needed = []

if is_asm_enabled
    checkasm_sources = files(
        'checkasm/checkasm.c',
        'checkasm/msac.c',
    )

    checkasm_tmpl_sources = files(
        'checkasm/cdef.c',
        'checkasm/ipred.c',
        'checkasm/itx.c',
        'checkasm/loopfilter.c',
        'checkasm/looprestoration.c',
        'checkasm/mc.c',
    )

    checkasm_bitdepth_objs = []
    foreach bitdepth : dav1d_bitdepths
        checkasm_bitdepth_lib = static_library(
            'checkasm_bitdepth_@0@'.format(bitdepth),
            checkasm_tmpl_sources,
            include_directories: dav1d_inc_dirs,
            c_args: ['-DBITDEPTH=@0@'.format(bitdepth), stackalign_flag],
            install: false,
            build_by_default: false,
        )
        checkasm_bitdepth_objs += checkasm_bitdepth_lib.extract_all_objects()
    endforeach

    checkasm_nasm_objs = []
    if host_machine.cpu_family() == 'aarch64'
        checkasm_sources += files('checkasm/arm/checkasm_64.S')
    elif host_machine.cpu_family().startswith('arm')
        checkasm_sources += files('checkasm/arm/checkasm_32.S')
    elif host_machine.cpu_family().startswith('x86')
        checkasm_nasm_objs = nasm_gen.process(files('checkasm/x86/checkasm.asm'))
    endif

    m_lib = cc.find_library('m', required: false)

    if meson.version().version_compare('< 0.48.999')
        libdav1d_nasm_objs_if_needed = libdav1d_nasm_objs
    endif

    checkasm = executable('checkasm',
        checkasm_sources,
        checkasm_nasm_objs,
        libdav1d_nasm_objs_if_needed,

        objects: [
            checkasm_bitdepth_objs,
            libdav1d.extract_all_objects(recursive: true),
            ],

        include_directories: dav1d_inc_dirs,
        c_args: [stackalign_flag, stackrealign_flag],
        build_by_default: false,
        dependencies : [thread_dependency, m_lib],
        )

    test('checkasm', checkasm, is_parallel: false)
endif

dav1d_fuzzer_sources =  files('libfuzzer/dav1d_fuzzer.c')
fuzzer_ldflags = []

if fuzzing_engine == 'none'
    dav1d_fuzzer_sources += files('libfuzzer/main.c')
elif fuzzing_engine == 'libfuzzer'
    fuzzer_ldflags += ['-fsanitize=fuzzer']
elif fuzzing_engine == 'oss-fuzz'
    # libFuzzingEngine needs libc++
    fuzzer_ldflags += ['-fsanitize=fuzzer', '-lFuzzingEngine', '-lc++']
endif

dav1d_fuzzer = executable('dav1d_fuzzer',
    dav1d_fuzzer_sources,
    include_directories: dav1d_inc_dirs,
    c_args: [stackalign_flag, stackrealign_flag],
    link_args: fuzzer_ldflags,
    link_with : libdav1d,
    build_by_default: true,
    dependencies : [thread_dependency],
    )

dav1d_fuzzer_mt = executable('dav1d_fuzzer_mt',
    dav1d_fuzzer_sources,
    include_directories: dav1d_inc_dirs,
    c_args: [stackalign_flag, stackrealign_flag, '-DDAV1D_MT_FUZZING'],
    link_args: fuzzer_ldflags,
    link_with : libdav1d,
    build_by_default: true,
    dependencies : [thread_dependency],
    )

objcopy = find_program('objcopy',
                       required: false)
if (objcopy.found() and
    not get_option('b_lto') and
    get_option('default_library') == 'static' and
    cc.has_function('posix_memalign', prefix : '#include <stdlib.h>', args : test_args))

    libdav1d_af = custom_target('libdav1d_af',
                                input: libdav1d,
                                output: 'libdav1d_af.a',
                                depends: libdav1d,
                                command: [objcopy,
                                          '--redefine-sym', 'malloc=__wrap_malloc',
                                          '--redefine-sym', 'posix_memalign=__wrap_posix_memalign',
                                          '--redefine-sym', 'pthread_create=__wrap_pthread_create',
                                          '--redefine-sym', 'pthread_cond_init=__wrap_pthread_cond_init',
                                          '--redefine-sym', 'pthread_mutex_init=__wrap_pthread_mutex_init',
                                          '@INPUT@', '@OUTPUT@'])

    dav1d_fuzzer_mem = executable('dav1d_fuzzer_mem',
        dav1d_fuzzer_sources + ['libfuzzer/alloc_fail.c'],
        include_directories: dav1d_inc_dirs,
        c_args: [stackalign_flag, stackrealign_flag, '-DDAV1D_ALLOC_FAIL'],
        link_args: fuzzer_ldflags + [join_paths(libdav1d_af.full_path())],
        link_depends: libdav1d_af,
        build_by_default: false,
        dependencies : [thread_dependency],
        )
endif

# Include dav1d test data repository with additional tests
if get_option('testdata_tests')
    subdir('dav1d-test-data')
endif
