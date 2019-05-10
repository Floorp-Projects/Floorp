# Copyright © 2018-2019, VideoLAN and dav1d authors
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
# Build definition for the dav1d library
#

# libdav1d source files
libdav1d_sources = files(
    'picture.c',
    'cpu.c',
    'data.c',
    'ref.c',
    'log.c',
    'getbits.c',
    'obu.c',
    'decode.c',
    'cdf.c',
    'msac.c',
    'tables.c',
    'scan.c',
    'dequant_tables.c',
    'intra_edge.c',
    'lf_mask.c',
    'ref_mvs.c',
    'warpmv.c',
    'wedge.c',
    'qm.c',
)

# libdav1d bitdepth source files
# These files are compiled for each bitdepth with
# `BITDEPTH` defined to the currently built bitdepth.
libdav1d_tmpl_sources = files(
    'ipred_prepare_tmpl.c',
    'ipred_tmpl.c',
    'itx_tmpl.c',
    'lf_apply_tmpl.c',
    'loopfilter_tmpl.c',
    'mc_tmpl.c',
    'cdef_apply_tmpl.c',
    'cdef_tmpl.c',
    'lr_apply_tmpl.c',
    'looprestoration_tmpl.c',
    'recon_tmpl.c',
    'film_grain_tmpl.c',
)

# libdav1d entrypoint source files
# These source files contain library entry points and are
# built with the stack-realign flag set, where necessary.
libdav1d_entrypoints_sources = files(
    'lib.c',
    'thread_task.c'
)

# ASM specific sources
libdav1d_nasm_objs = []
if is_asm_enabled
    if (host_machine.cpu_family() == 'aarch64' or
        host_machine.cpu_family().startswith('arm'))

        libdav1d_sources += files(
            'arm/cpu.c',
        )
        libdav1d_tmpl_sources += files(
            'arm/cdef_init_tmpl.c',
            'arm/loopfilter_init_tmpl.c',
            'arm/looprestoration_init_tmpl.c',
            'arm/mc_init_tmpl.c',
        )
        if host_machine.cpu_family() == 'aarch64'
            libdav1d_sources += files(
                'arm/64/cdef.S',
                'arm/64/loopfilter.S',
                'arm/64/looprestoration.S',
                'arm/64/mc.S',
                'arm/64/msac.S',
            )
        elif host_machine.cpu_family().startswith('arm')
            libdav1d_sources += files(
                'arm/32/looprestoration.S',
                'arm/32/mc.S',
            )
        endif
    elif host_machine.cpu_family().startswith('x86')

        libdav1d_sources += files(
            'x86/cpu.c',
        )

        libdav1d_tmpl_sources += files(
            'x86/cdef_init_tmpl.c',
            'x86/ipred_init_tmpl.c',
            'x86/itx_init_tmpl.c',
            'x86/loopfilter_init_tmpl.c',
            'x86/looprestoration_init_tmpl.c',
            'x86/mc_init_tmpl.c',
        )

        # NASM source files
        libdav1d_sources_asm = files(
            'x86/cpuid.asm',
            'x86/msac.asm',
        )

        if dav1d_bitdepths.contains('8')
            libdav1d_sources_asm += files(
                'x86/cdef.asm',
                'x86/cdef_sse.asm',
                'x86/ipred.asm',
                'x86/ipred_ssse3.asm',
                'x86/itx.asm',
                'x86/itx_ssse3.asm',
                'x86/loopfilter.asm',
                'x86/looprestoration.asm',
                'x86/looprestoration_ssse3.asm',
                'x86/mc.asm',
                'x86/mc_ssse3.asm',
            )
        endif

        if dav1d_bitdepths.contains('16')
            libdav1d_sources_asm += files(
            )
        endif

        # Compile the ASM sources with NASM
        libdav1d_nasm_objs = nasm_gen.process(libdav1d_sources_asm)
    endif
endif



api_export_flags = []

#
# Windows .rc file and API export flags
#

if host_machine.system() == 'windows' and get_option('default_library') != 'static'
    rc_version_array = meson.project_version().split('.')
    winmod = import('windows')
    rc_data = configuration_data()
    rc_data.set('PROJECT_VERSION_MAJOR', rc_version_array[0])
    rc_data.set('PROJECT_VERSION_MINOR', rc_version_array[1])
    rc_data.set('PROJECT_VERSION_REVISION', rc_version_array[2])
    rc_data.set('API_VERSION_MAJOR', dav1d_api_version_major)
    rc_data.set('API_VERSION_MINOR', dav1d_api_version_minor)
    rc_data.set('API_VERSION_REVISION', dav1d_api_version_revision)
    rc_data.set('COPYRIGHT_YEARS', '2019')

    rc_file = configure_file(
        input : 'dav1d.rc.in',
        output : 'dav1d.rc',
        configuration : rc_data
    )

    libdav1d_rc_obj = winmod.compile_resources(rc_file)

    api_export_flags = ['-DDAV1D_BUILDING_DLL']
else
    libdav1d_rc_obj = []
endif




#
# Library definitions
#

# Helper library for dav1d entrypoints
libdav1d_entrypoints_objs = static_library('dav1d_entrypoint',
    libdav1d_entrypoints_sources,
    rev_target, config_h_target,

    include_directories : dav1d_inc_dirs,
    dependencies: [stdatomic_dependency],
    c_args : [stackalign_flag, stackrealign_flag, api_export_flags],
    install : false,
    build_by_default : false,
).extract_all_objects()

# Helper library for each bitdepth
libdav1d_bitdepth_objs = []
foreach bitdepth : dav1d_bitdepths
    libdav1d_bitdepth_objs += static_library(
        'dav1d_bitdepth_@0@'.format(bitdepth),
        libdav1d_tmpl_sources, config_h_target,
        include_directories: dav1d_inc_dirs,
        dependencies : [stdatomic_dependency],
        c_args : ['-DBITDEPTH=@0@'.format(bitdepth)] + stackalign_flag,
        install : false,
        build_by_default : false,
    ).extract_all_objects()
endforeach

# The final dav1d library
if host_machine.system() == 'windows'
    dav1d_soversion = ''
else
    dav1d_soversion = dav1d_api_version_major
endif

libdav1d = library('dav1d',
    libdav1d_sources,
    libdav1d_nasm_objs,
    libdav1d_rc_obj,

    objects : [
        libdav1d_bitdepth_objs,
        libdav1d_entrypoints_objs
        ],

    include_directories : dav1d_inc_dirs,
    dependencies : [
        stdatomic_dependency,
        thread_dependency,
        thread_compat_dep,
        ],
    c_args : [stackalign_flag, api_export_flags],
    version : dav1d_soname_version,
    soversion : dav1d_soversion,
    install : true,
)

#
# Generate pkg-config .pc file
#
pkg_mod = import('pkgconfig')
pkg_mod.generate(libraries: libdav1d,
    version: meson.project_version(),
    name: 'libdav1d',
    filebase: 'dav1d',
    description: 'AV1 decoding library'
)
