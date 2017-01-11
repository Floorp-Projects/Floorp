#!/bin/bash
##
##  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
##
##  Use of this source code is governed by a BSD-style license
##  that can be found in the LICENSE file in the root of the source
##  tree. An additional intellectual property rights grant can be found
##  in the file PATENTS.  All contributing project authors may
##  be found in the AUTHORS file in the root of the source tree.
##


self=$0
self_basename=${self##*/}
EOL=$'\n'
EOLDOS=$'\r'

show_help() {
    cat <<EOF
Usage: ${self_basename} [options] file1 [file2 ...]

This script generates a Visual Studio 2005 solution file from a list of project
files.

Options:
    --help                      Print this message
    --out=outfile               Redirect output to a file
    --ver=version               Version (7,8,9,10,11) of visual studio to generate for
    --target=isa-os-cc          Target specifier
EOF
    exit 1
}

die() {
    echo "${self_basename}: $@" >&2
    [ -f "${outfile}" ] && rm -f ${outfile}{,.mk}
    exit 1
}

die_unknown(){
    echo "Unknown option \"$1\"." >&2
    echo "See ${self_basename} --help for available options." >&2
    [ -f "${outfile}" ] && rm -f ${outfile}{,.mk}
    exit 1
}

indent1=$'\t'
indent=""
indent_push() {
    indent="${indent}${indent1}"
}
indent_pop() {
    indent="${indent%${indent1}}"
}

parse_project() {
    local file=$1
    if [ "$sfx" = "vcproj" ]; then
        local name=`grep Name "$file" | awk 'BEGIN {FS="\""}{if (NR==1) print $2}'`
        local guid=`grep ProjectGUID "$file" | awk 'BEGIN {FS="\""}{if (NR==1) print $2}'`
    else
        local name=`grep RootNamespace "$file" | sed 's,.*<.*>\(.*\)</.*>.*,\1,'`
        local guid=`grep ProjectGuid "$file" | sed 's,.*<.*>\(.*\)</.*>.*,\1,'`
    fi

    # save the project GUID to a varaible, normalizing to the basename of the
    # vcproj file without the extension
    local var
    var=${file##*/}
    var=${var%%.${sfx}}
    eval "${var}_file=\"$1\""
    eval "${var}_name=$name"
    eval "${var}_guid=$guid"

    if [ "$sfx" = "vcproj" ]; then
        cur_config_list=`grep -A1 '<Configuration' $file |
            grep Name | cut -d\" -f2`
    else
        cur_config_list=`grep -B1 'Label="Configuration"' $file |
            grep Condition | cut -d\' -f4`
    fi
    new_config_list=$(for i in $config_list $cur_config_list; do
        echo $i
    done | sort | uniq)
    if [ "$config_list" != "" ] && [ "$config_list" != "$new_config_list" ]; then
        mixed_platforms=1
    fi
    config_list="$new_config_list"
    eval "${var}_config_list=\"$cur_config_list\""
    proj_list="${proj_list} ${var}"
}

process_project() {
    eval "local file=\${$1_file}"
    eval "local name=\${$1_name}"
    eval "local guid=\${$1_guid}"

    # save the project GUID to a varaible, normalizing to the basename of the
    # vcproj file without the extension
    local var
    var=${file##*/}
    var=${var%%.${sfx}}
    eval "${var}_guid=$guid"

    echo "Project(\"{8BC9CEB8-8B4A-11D0-8D11-00A0C91BC942}\") = \"$name\", \"$file\", \"$guid\""
    indent_push

    eval "local deps=\"\${${var}_deps}\""
    if [ -n "$deps" ] && [ "$sfx" = "vcproj" ]; then
        echo "${indent}ProjectSection(ProjectDependencies) = postProject"
        indent_push

        for dep in $deps; do
            eval "local dep_guid=\${${dep}_guid}"
            [ -z "${dep_guid}" ] && die "Unknown GUID for $dep (dependency of $var)"
            echo "${indent}$dep_guid = $dep_guid"
        done

        indent_pop
        echo "${indent}EndProjectSection"

    fi

    indent_pop
    echo "EndProject"
}

process_global() {
    echo "Global"
    indent_push

    #
    # Solution Configuration Platforms
    #
    echo "${indent}GlobalSection(SolutionConfigurationPlatforms) = preSolution"
    indent_push
    IFS_bak=${IFS}
    IFS=$'\r'$'\n'
    if [ "$mixed_platforms" != "" ]; then
        config_list="
Release|Mixed Platforms
Debug|Mixed Platforms"
    fi
    for config in ${config_list}; do
        echo "${indent}$config = $config"
    done
    IFS=${IFS_bak}
    indent_pop
    echo "${indent}EndGlobalSection"

    #
    # Project Configuration Platforms
    #
    echo "${indent}GlobalSection(ProjectConfigurationPlatforms) = postSolution"
    indent_push
    for proj in ${proj_list}; do
        eval "local proj_guid=\${${proj}_guid}"
        eval "local proj_config_list=\${${proj}_config_list}"
        IFS=$'\r'$'\n'
        for config in ${proj_config_list}; do
            if [ "$mixed_platforms" != "" ]; then
                local c=${config%%|*}
                echo "${indent}${proj_guid}.${c}|Mixed Platforms.ActiveCfg = ${config}"
                echo "${indent}${proj_guid}.${c}|Mixed Platforms.Build.0 = ${config}"
            else
                echo "${indent}${proj_guid}.${config}.ActiveCfg = ${config}"
                echo "${indent}${proj_guid}.${config}.Build.0 = ${config}"
            fi

        done
        IFS=${IFS_bak}
    done
    indent_pop
    echo "${indent}EndGlobalSection"

    #
    # Solution Properties
    #
    echo "${indent}GlobalSection(SolutionProperties) = preSolution"
    indent_push
    echo "${indent}HideSolutionNode = FALSE"
    indent_pop
    echo "${indent}EndGlobalSection"

    indent_pop
    echo "EndGlobal"
}

process_makefile() {
    IFS_bak=${IFS}
    IFS=$'\r'$'\n'
    local TAB=$'\t'
    cat <<EOF
ifeq (\$(CONFIG_VS_VERSION),7)
MSBUILD_TOOL := devenv.com
else
MSBUILD_TOOL := msbuild.exe
endif
found_devenv := \$(shell which \$(MSBUILD_TOOL) >/dev/null 2>&1 && echo yes)
.nodevenv.once:
${TAB}@echo "  * \$(MSBUILD_TOOL) not found in path."
${TAB}@echo "  * "
${TAB}@echo "  * You will have to build all configurations manually using the"
${TAB}@echo "  * Visual Studio IDE. To allow make to build them automatically,"
${TAB}@echo "  * add the Common7/IDE directory of your Visual Studio"
${TAB}@echo "  * installation to your path, eg:"
${TAB}@echo "  *   C:\Program Files\Microsoft Visual Studio 8\Common7\IDE"
${TAB}@echo "  * "
${TAB}@touch \$@
CLEAN-OBJS += \$(if \$(found_devenv),,.nodevenv.once)

EOF

    for sln_config in ${config_list}; do
        local config=${sln_config%%|*}
        local platform=${sln_config##*|}
        local nows_sln_config=`echo $sln_config | sed -e 's/[^a-zA-Z0-9]/_/g'`
        cat <<EOF
BUILD_TARGETS += \$(if \$(NO_LAUNCH_DEVENV),,$nows_sln_config)
clean::
${TAB}rm -rf "$platform"/"$config"
.PHONY: $nows_sln_config
ifneq (\$(found_devenv),)
  ifeq (\$(CONFIG_VS_VERSION),7)
$nows_sln_config: $outfile
${TAB}\$(MSBUILD_TOOL) $outfile -build "$config"

  else
$nows_sln_config: $outfile
${TAB}\$(MSBUILD_TOOL) $outfile -m -t:Build \\
${TAB}${TAB}-p:Configuration="$config" -p:Platform="$platform"

  endif
else
$nows_sln_config: $outfile .nodevenv.once
${TAB}@echo "  * Skipping build of $sln_config (\$(MSBUILD_TOOL) not in path)."
${TAB}@echo "  * "
endif

EOF
    done
    IFS=${IFS_bak}
}

# Process command line
outfile=/dev/stdout
for opt in "$@"; do
    optval="${opt#*=}"
    case "$opt" in
    --help|-h) show_help
    ;;
    --out=*) outfile="${optval}"; mkoutfile="${optval}".mk
    ;;
    --dep=*) eval "${optval%%:*}_deps=\"\${${optval%%:*}_deps} ${optval##*:}\""
    ;;
    --ver=*) vs_ver="$optval"
             case $optval in
             [789]|10|11|12)
             ;;
             *) die Unrecognized Visual Studio Version in $opt
             ;;
             esac
    ;;
    --ver=*) vs_ver="$optval"
             case $optval in
             7) sln_vers="8.00"
                sln_vers_str="Visual Studio .NET 2003"
             ;;
             [89])
             ;;
             *) die "Unrecognized Visual Studio Version '$optval' in $opt"
             ;;
             esac
    ;;
    --target=*) target="${optval}"
    ;;
    -*) die_unknown $opt
    ;;
    *) file_list[${#file_list[@]}]="$opt"
    esac
done
outfile=${outfile:-/dev/stdout}
mkoutfile=${mkoutfile:-/dev/stdout}
case "${vs_ver:-8}" in
    7) sln_vers="8.00"
       sln_vers_str="Visual Studio .NET 2003"
    ;;
    8) sln_vers="9.00"
       sln_vers_str="Visual Studio 2005"
    ;;
    9) sln_vers="10.00"
       sln_vers_str="Visual Studio 2008"
    ;;
    10) sln_vers="11.00"
       sln_vers_str="Visual Studio 2010"
    ;;
    11) sln_vers="12.00"
       sln_vers_str="Visual Studio 2012"
    ;;
    12) sln_vers="12.00"
       sln_vers_str="Visual Studio 2013"
    ;;
esac
case "${vs_ver:-8}" in
    [789])
    sfx=vcproj
    ;;
    10|11|12)
    sfx=vcxproj
    ;;
esac

for f in "${file_list[@]}"; do
    parse_project $f
done
cat  >${outfile} <<EOF
Microsoft Visual Studio Solution File, Format Version $sln_vers${EOLDOS}
# $sln_vers_str${EOLDOS}
EOF
for proj in ${proj_list}; do
    process_project $proj >>${outfile}
done
process_global >>${outfile}
process_makefile >${mkoutfile}
