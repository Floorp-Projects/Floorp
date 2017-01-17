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
self_dirname=$(dirname "$0")

. "$self_dirname/msvs_common.sh"|| exit 127

show_help() {
    cat <<EOF
Usage: ${self_basename} --name=projname [options] file1 [file2 ...]

This script generates a Visual Studio project file from a list of source
code files.

Options:
    --help                      Print this message
    --exe                       Generate a project for building an Application
    --lib                       Generate a project for creating a static library
    --dll                       Generate a project for creating a dll
    --static-crt                Use the static C runtime (/MT)
    --target=isa-os-cc          Target specifier (required)
    --out=filename              Write output to a file [stdout]
    --name=project_name         Name of the project (required)
    --proj-guid=GUID            GUID to use for the project
    --module-def=filename       File containing export definitions (for DLLs)
    --ver=version               Version (7,8,9) of visual studio to generate for
    --src-path-bare=dir         Path to root of source tree
    -Ipath/to/include           Additional include directories
    -DFLAG[=value]              Preprocessor macros to define
    -Lpath/to/lib               Additional library search paths
    -llibname                   Library to link against
EOF
    exit 1
}

generate_filter() {
    local var=$1
    local name=$2
    local pats=$3
    local file_list_sz
    local i
    local f
    local saveIFS="$IFS"
    local pack
    echo "generating filter '$name' from ${#file_list[@]} files" >&2
    IFS=*

    open_tag Filter \
        Name=$name \
        Filter=$pats \
        UniqueIdentifier=`generate_uuid` \

    file_list_sz=${#file_list[@]}
    for i in ${!file_list[@]}; do
        f=${file_list[i]}
        for pat in ${pats//;/$IFS}; do
            if [ "${f##*.}" == "$pat" ]; then
                unset file_list[i]

                objf=$(echo ${f%.*}.obj \
                       | sed -e "s,$src_path_bare,," \
                             -e 's/^[\./]\+//g' -e 's,[:/ ],_,g')
                open_tag File RelativePath="$f"

                if [ "$pat" == "asm" ] && $asm_use_custom_step; then
                    # Avoid object file name collisions, i.e. vpx_config.c and
                    # vpx_config.asm produce the same object file without
                    # this additional suffix.
                    objf=${objf%.obj}_asm.obj
                    for plat in "${platforms[@]}"; do
                        for cfg in Debug Release; do
                            open_tag FileConfiguration \
                                Name="${cfg}|${plat}" \

                            tag Tool \
                                Name="VCCustomBuildTool" \
                                Description="Assembling \$(InputFileName)" \
                                CommandLine="$(eval echo \$asm_${cfg}_cmdline) -o \$(IntDir)\\$objf" \
                                Outputs="\$(IntDir)\\$objf" \

                            close_tag FileConfiguration
                        done
                    done
                fi
                if [ "$pat" == "c" ] || \
                   [ "$pat" == "cc" ] || [ "$pat" == "cpp" ]; then
                    for plat in "${platforms[@]}"; do
                        for cfg in Debug Release; do
                            open_tag FileConfiguration \
                                Name="${cfg}|${plat}" \

                            tag Tool \
                                Name="VCCLCompilerTool" \
                                ObjectFile="\$(IntDir)\\$objf" \

                            close_tag FileConfiguration
                        done
                    done
                fi
                close_tag File

                break
            fi
        done
    done

    close_tag Filter
    IFS="$saveIFS"
}

# Process command line
unset target
for opt in "$@"; do
    optval="${opt#*=}"
    case "$opt" in
        --help|-h) show_help
        ;;
        --target=*) target="${optval}"
        ;;
        --out=*) outfile="$optval"
        ;;
        --name=*) name="${optval}"
        ;;
        --proj-guid=*) guid="${optval}"
        ;;
        --module-def=*) link_opts="${link_opts} ModuleDefinitionFile=${optval}"
        ;;
        --exe) proj_kind="exe"
        ;;
        --dll) proj_kind="dll"
        ;;
        --lib) proj_kind="lib"
        ;;
        --src-path-bare=*)
            src_path_bare=$(fix_path "$optval")
            src_path_bare=${src_path_bare%/}
        ;;
        --static-crt) use_static_runtime=true
        ;;
        --ver=*)
            vs_ver="$optval"
            case "$optval" in
                [789])
                ;;
                *) die Unrecognized Visual Studio Version in $opt
                ;;
            esac
        ;;
        -I*)
            opt=${opt##-I}
            opt=$(fix_path "$opt")
            opt="${opt%/}"
            incs="${incs}${incs:+;}&quot;${opt}&quot;"
            yasmincs="${yasmincs} -I&quot;${opt}&quot;"
        ;;
        -D*) defines="${defines}${defines:+;}${opt##-D}"
        ;;
        -L*) # fudge . to $(OutDir)
            if [ "${opt##-L}" == "." ]; then
                libdirs="${libdirs}${libdirs:+;}&quot;\$(OutDir)&quot;"
            else
                 # Also try directories for this platform/configuration
                 opt=${opt##-L}
                 opt=$(fix_path "$opt")
                 libdirs="${libdirs}${libdirs:+;}&quot;${opt}&quot;"
                 libdirs="${libdirs}${libdirs:+;}&quot;${opt}/\$(PlatformName)/\$(ConfigurationName)&quot;"
                 libdirs="${libdirs}${libdirs:+;}&quot;${opt}/\$(PlatformName)&quot;"
            fi
        ;;
        -l*) libs="${libs}${libs:+ }${opt##-l}.lib"
        ;;
        -*) die_unknown $opt
        ;;
        *)
            # The paths in file_list are fixed outside of the loop.
            file_list[${#file_list[@]}]="$opt"
            case "$opt" in
                 *.asm) uses_asm=true
                 ;;
            esac
        ;;
    esac
done

# Make one call to fix_path for file_list to improve performance.
fix_file_list file_list

outfile=${outfile:-/dev/stdout}
guid=${guid:-`generate_uuid`}
asm_use_custom_step=false
uses_asm=${uses_asm:-false}
case "${vs_ver:-8}" in
    7) vs_ver_id="7.10"
       asm_use_custom_step=$uses_asm
       warn_64bit='Detect64BitPortabilityProblems=true'
    ;;
    8) vs_ver_id="8.00"
       asm_use_custom_step=$uses_asm
       warn_64bit='Detect64BitPortabilityProblems=true'
    ;;
    9) vs_ver_id="9.00"
       asm_use_custom_step=$uses_asm
       warn_64bit='Detect64BitPortabilityProblems=false'
    ;;
esac

[ -n "$name" ] || die "Project name (--name) must be specified!"
[ -n "$target" ] || die "Target (--target) must be specified!"

if ${use_static_runtime:-false}; then
    release_runtime=0
    debug_runtime=1
    lib_sfx=mt
else
    release_runtime=2
    debug_runtime=3
    lib_sfx=md
fi

# Calculate debug lib names: If a lib ends in ${lib_sfx}.lib, then rename
# it to ${lib_sfx}d.lib. This precludes linking to release libs from a
# debug exe, so this may need to be refactored later.
for lib in ${libs}; do
    if [ "$lib" != "${lib%${lib_sfx}.lib}" ]; then
        lib=${lib%.lib}d.lib
    fi
    debug_libs="${debug_libs}${debug_libs:+ }${lib}"
done


# List Keyword for this target
case "$target" in
    x86*) keyword="ManagedCProj"
    ;;
    *) die "Unsupported target $target!"
esac

# List of all platforms supported for this target
case "$target" in
    x86_64*)
        platforms[0]="x64"
        asm_Debug_cmdline="yasm -Xvc -g cv8 -f win64 ${yasmincs} &quot;\$(InputPath)&quot;"
        asm_Release_cmdline="yasm -Xvc -f win64 ${yasmincs} &quot;\$(InputPath)&quot;"
    ;;
    x86*)
        platforms[0]="Win32"
        asm_Debug_cmdline="yasm -Xvc -g cv8 -f win32 ${yasmincs} &quot;\$(InputPath)&quot;"
        asm_Release_cmdline="yasm -Xvc -f win32 ${yasmincs} &quot;\$(InputPath)&quot;"
    ;;
    *) die "Unsupported target $target!"
    ;;
esac

generate_vcproj() {
    case "$proj_kind" in
        exe) vs_ConfigurationType=1
        ;;
        dll) vs_ConfigurationType=2
        ;;
        *)   vs_ConfigurationType=4
        ;;
    esac

    echo "<?xml version=\"1.0\" encoding=\"Windows-1252\"?>"
    open_tag VisualStudioProject \
        ProjectType="Visual C++" \
        Version="${vs_ver_id}" \
        Name="${name}" \
        ProjectGUID="{${guid}}" \
        RootNamespace="${name}" \
        Keyword="${keyword}" \

    open_tag Platforms
    for plat in "${platforms[@]}"; do
        tag Platform Name="$plat"
    done
    close_tag Platforms

    open_tag Configurations
    for plat in "${platforms[@]}"; do
        plat_no_ws=`echo $plat | sed 's/[^A-Za-z0-9_]/_/g'`
        open_tag Configuration \
            Name="Debug|$plat" \
            OutputDirectory="\$(SolutionDir)$plat_no_ws/\$(ConfigurationName)" \
            IntermediateDirectory="$plat_no_ws/\$(ConfigurationName)/${name}" \
            ConfigurationType="$vs_ConfigurationType" \
            CharacterSet="1" \

        case "$target" in
            x86*)
                case "$name" in
                    vpx)
                        tag Tool \
                            Name="VCCLCompilerTool" \
                            Optimization="0" \
                            AdditionalIncludeDirectories="$incs" \
                            PreprocessorDefinitions="WIN32;_DEBUG;_CRT_SECURE_NO_WARNINGS;_CRT_SECURE_NO_DEPRECATE;$defines" \
                            RuntimeLibrary="$debug_runtime" \
                            UsePrecompiledHeader="0" \
                            WarningLevel="3" \
                            DebugInformationFormat="2" \
                            $warn_64bit \

                        $uses_asm && tag Tool Name="YASM"  IncludePaths="$incs" Debug="true"
                    ;;
                    *)
                        tag Tool \
                            Name="VCCLCompilerTool" \
                            Optimization="0" \
                            AdditionalIncludeDirectories="$incs" \
                            PreprocessorDefinitions="WIN32;_DEBUG;_CRT_SECURE_NO_WARNINGS;_CRT_SECURE_NO_DEPRECATE;$defines" \
                            RuntimeLibrary="$debug_runtime" \
                            UsePrecompiledHeader="0" \
                            WarningLevel="3" \
                            DebugInformationFormat="2" \
                            $warn_64bit \

                        $uses_asm && tag Tool Name="YASM"  IncludePaths="$incs" Debug="true"
                    ;;
                esac
            ;;
        esac

        case "$proj_kind" in
            exe)
                case "$target" in
                    x86*)
                        case "$name" in
                            *)
                                tag Tool \
                                    Name="VCLinkerTool" \
                                    AdditionalDependencies="$debug_libs \$(NoInherit)" \
                                    AdditionalLibraryDirectories="$libdirs" \
                                    GenerateDebugInformation="true" \
                                    ProgramDatabaseFile="\$(OutDir)/${name}.pdb" \
                            ;;
                        esac
                    ;;
                 esac
            ;;
            lib)
                case "$target" in
                    x86*)
                        tag Tool \
                            Name="VCLibrarianTool" \
                            OutputFile="\$(OutDir)/${name}${lib_sfx}d.lib" \

                    ;;
                esac
            ;;
            dll)
                tag Tool \
                    Name="VCLinkerTool" \
                    AdditionalDependencies="\$(NoInherit)" \
                    LinkIncremental="2" \
                    GenerateDebugInformation="true" \
                    AssemblyDebug="1" \
                    TargetMachine="1" \
                    $link_opts \

            ;;
        esac

        close_tag Configuration

        open_tag Configuration \
            Name="Release|$plat" \
            OutputDirectory="\$(SolutionDir)$plat_no_ws/\$(ConfigurationName)" \
            IntermediateDirectory="$plat_no_ws/\$(ConfigurationName)/${name}" \
            ConfigurationType="$vs_ConfigurationType" \
            CharacterSet="1" \
            WholeProgramOptimization="0" \

        case "$target" in
            x86*)
                case "$name" in
                    vpx)
                        tag Tool \
                            Name="VCCLCompilerTool" \
                            Optimization="2" \
                            FavorSizeorSpeed="1" \
                            AdditionalIncludeDirectories="$incs" \
                            PreprocessorDefinitions="WIN32;NDEBUG;_CRT_SECURE_NO_WARNINGS;_CRT_SECURE_NO_DEPRECATE;$defines" \
                            RuntimeLibrary="$release_runtime" \
                            UsePrecompiledHeader="0" \
                            WarningLevel="3" \
                            DebugInformationFormat="0" \
                            $warn_64bit \

                        $uses_asm && tag Tool Name="YASM"  IncludePaths="$incs"
                    ;;
                    *)
                        tag Tool \
                            Name="VCCLCompilerTool" \
                            AdditionalIncludeDirectories="$incs" \
                            Optimization="2" \
                            FavorSizeorSpeed="1" \
                            PreprocessorDefinitions="WIN32;NDEBUG;_CRT_SECURE_NO_WARNINGS;_CRT_SECURE_NO_DEPRECATE;$defines" \
                            RuntimeLibrary="$release_runtime" \
                            UsePrecompiledHeader="0" \
                            WarningLevel="3" \
                            DebugInformationFormat="0" \
                            $warn_64bit \

                        $uses_asm && tag Tool Name="YASM"  IncludePaths="$incs"
                    ;;
                esac
            ;;
        esac

        case "$proj_kind" in
            exe)
                case "$target" in
                    x86*)
                        case "$name" in
                            *)
                                tag Tool \
                                    Name="VCLinkerTool" \
                                    AdditionalDependencies="$libs \$(NoInherit)" \
                                    AdditionalLibraryDirectories="$libdirs" \

                            ;;
                        esac
                    ;;
                 esac
            ;;
            lib)
                case "$target" in
                    x86*)
                        tag Tool \
                            Name="VCLibrarianTool" \
                            OutputFile="\$(OutDir)/${name}${lib_sfx}.lib" \

                    ;;
                esac
            ;;
            dll) # note differences to debug version: LinkIncremental, AssemblyDebug
                tag Tool \
                    Name="VCLinkerTool" \
                    AdditionalDependencies="\$(NoInherit)" \
                    LinkIncremental="1" \
                    GenerateDebugInformation="true" \
                    TargetMachine="1" \
                    $link_opts \

            ;;
        esac

        close_tag Configuration
    done
    close_tag Configurations

    open_tag Files
    generate_filter srcs   "Source Files"   "c;cc;cpp;def;odl;idl;hpj;bat;asm;asmx"
    generate_filter hdrs   "Header Files"   "h;hm;inl;inc;xsd"
    generate_filter resrcs "Resource Files" "rc;ico;cur;bmp;dlg;rc2;rct;bin;rgs;gif;jpg;jpeg;jpe;resx;tiff;tif;png;wav"
    generate_filter resrcs "Build Files"    "mk"
    close_tag Files

    tag       Globals
    close_tag VisualStudioProject

    # This must be done from within the {} subshell
    echo "Ignored files list (${#file_list[@]} items) is:" >&2
    for f in "${file_list[@]}"; do
        echo "    $f" >&2
    done
}

generate_vcproj |
    sed  -e '/"/s;\([^ "]\)/;\1\\;g' > ${outfile}

exit
<!--
TODO: Add any files not captured by filters.
                <File
                        RelativePath=".\ReadMe.txt"
                        >
                </File>
-->
