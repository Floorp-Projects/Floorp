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

# Common source files used by tools and examples

dav1d_input_sources = files(
    'input/input.c',
    'input/annexb.c',
    'input/ivf.c',
    'input/section5.c',
)

dav1d_output_sources = files(
    'output/md5.c',
    'output/null.c',
    'output/output.c',
    'output/y4m2.c',
    'output/yuv.c',
)

dav1d_input_objs = static_library('dav1d_input',
    dav1d_input_sources,

    include_directories : dav1d_inc_dirs,
    install : false,
    build_by_default : false,
)

dav1d_output_objs = static_library('dav1d_output',
    dav1d_output_sources,

    include_directories : dav1d_inc_dirs,
    install : false,
    build_by_default : false,
)


# Leave subdir if tools are disabled
if not get_option('enable_tools')
    subdir_done()
endif


#
# Build definition for the dav1d tools
#

# Configuratin data for cli_config.h
cli_cdata = configuration_data()

cli_config_h_target = configure_file(output: 'cli_config.h', configuration: cli_cdata)

# dav1d cli tool sources
dav1d_sources = files(
    'dav1d.c',
    'dav1d_cli_parse.c',
)

dav1d = executable('dav1d',
    dav1d_sources,
    rev_target, cli_config_h_target,

    link_with : [libdav1d, dav1d_input_objs, dav1d_output_objs],
    include_directories : [dav1d_inc_dirs],
    dependencies : [getopt_dependency, thread_dependency, rt_dependency],
    install : true,
)
