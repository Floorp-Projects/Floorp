# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import random
import errno
import types
import os
import xml.etree.ElementTree as ET
from .common import CommonBackend

from ..frontend.data import (
    Defines,
)

# TODO Have ./mach eclipse generate the workspace and index it:
# /Users/bgirard/mozilla/eclipse/eclipse/eclipse/eclipse -application org.eclipse.cdt.managedbuilder.core.headlessbuild -data $PWD/workspace -importAll $PWD/eclipse
# Open eclipse:
# /Users/bgirard/mozilla/eclipse/eclipse/eclipse/eclipse -data $PWD/workspace

class CppEclipseBackend(CommonBackend):
    """Backend that generates Cpp Eclipse project files.
    """

    def _init(self):
        CommonBackend._init(self)

        self._paths_to_defines = {}
        self._workspace_dir = os.path.join(self.environment.topobjdir, 'eclipse_workspace')
        self._project_dir = os.path.join(self._workspace_dir, 'gecko')

        self._macbundle = self.environment.substs['MOZ_MACBUNDLE_NAME']
        self._appname = self.environment.substs['MOZ_APP_NAME']
        self._bin_suffix = self.environment.substs['BIN_SUFFIX']
        self._cxx = self.environment.substs['CXX']
        # Note: We need the C Pre Processor (CPP) flags, not the CXX flags
        self._cppflags = self.environment.substs.get('CPPFLAGS', '')

        def detailed(summary):
            return ('\n' + \
                   'Generated Cpp Eclipse workspace in "%s".\n' + \
                   'OPTIONAL: Setup & index the project using: eclipse -application org.eclipse.cdt.managedbuilder.core.headlessbuild -data %s -importAll %s\n' + \
                   'NOTE: This will take about 10 minutes.\n\n' \
                   'Run with: eclipse -data %s\n' \
                   'Import the project using File > Import > General > Existing Project into workspace') \
                   % (self._workspace_dir, self._workspace_dir, self._project_dir, self._workspace_dir)

        self.summary.backend_detailed_summary = types.MethodType(detailed,
            self.summary)

    def consume_object(self, obj):
        obj.ack()

        reldir = getattr(obj, 'relativedir', None)

        # Note that unlike VS, Eclipse' indexer seem to crawl the headers and
        # isn't picky about the local includes.
        if isinstance(obj, Defines):
            self._paths_to_defines.setdefault(reldir, {}).update(obj.defines)

    def consume_finished(self):
        settings_dir = os.path.join(self._project_dir, '.settings')
        launch_dir = os.path.join(self._project_dir, 'RunConfigurations')
        workspace_settings_dir = os.path.join(self._workspace_dir, '.metadata/.plugins/org.eclipse.core.runtime/.settings')
        workspace_language_dir = os.path.join(self._workspace_dir, '.metadata/.plugins/org.eclipse.cdt.core')

        for dir_name in [self._project_dir, settings_dir, launch_dir, workspace_settings_dir, workspace_language_dir]:
            try:
                os.makedirs(dir_name)
            except OSError as e:
                if e.errno != errno.EEXIST:
                    raise

        project_path = os.path.join(self._project_dir, '.project')
        with open(project_path, 'wb') as fh:
            self._write_project(fh)

        cproject_path = os.path.join(self._project_dir, '.cproject')
        with open(cproject_path, 'wb') as fh:
            self._write_cproject(fh)

        language_path = os.path.join(settings_dir, 'language.settings.xml')
        with open(language_path, 'wb') as fh:
            self._write_language_settings(fh)

        workspace_language_path = os.path.join(workspace_language_dir, 'language.settings.xml')
        with open(workspace_language_path, 'wb') as fh:
            workspace_lang_settings = WORKSPACE_LANGUAGE_SETTINGS_TEMPLATE
            workspace_lang_settings = workspace_lang_settings.replace("@COMPILER_FLAGS@", self._cxx + " " + self._cppflags);
            fh.write(workspace_lang_settings)

        self._write_launch_files(launch_dir)

        # This will show up as an 'unmanged' formatter. This can be named by generating
        # another file.
        formatter_prefs_path = os.path.join(settings_dir, 'org.eclipse.cdt.core.prefs')
        with open(formatter_prefs_path, 'wb') as fh:
            fh.write(FORMATTER_SETTINGS);

        editor_prefs_path = os.path.join(workspace_settings_dir, "org.eclipse.ui.editors.prefs");
        with open(editor_prefs_path, 'wb') as fh:
            fh.write(EDITOR_SETTINGS);

    def _define_entry(self, name, value):
        define = ET.Element('entry')
        define.set('kind', 'macro')
        define.set('name', name)
        define.set('value', value)
        return ET.tostring(define)

    def _write_language_settings(self, fh):
        settings = LANGUAGE_SETTINGS_TEMPLATE

        settings = settings.replace('@GLOBAL_INCLUDE_PATH@', os.path.join(self.environment.topobjdir, 'dist/include'))
        settings = settings.replace('@NSPR_INCLUDE_PATH@', os.path.join(self.environment.topobjdir, 'dist/include/nspr'))
        settings = settings.replace('@IPDL_INCLUDE_PATH@', os.path.join(self.environment.topobjdir, 'ipc/ipdl/_ipdlheaders'))
        settings = settings.replace('@PREINCLUDE_FILE_PATH@', os.path.join(self.environment.topobjdir, 'dist/include/mozilla-config.h'))
        settings = settings.replace('@DEFINE_MOZILLA_INTERNAL_API@', self._define_entry('MOZILLA_INTERNAL_API', '1'))
        settings = settings.replace('@DEFINE_MDCPUCFG@', self._define_entry('MDCPUCFG', self.environment.substs['TARGET_NSPR_MDCPUCFG']))
        settings = settings.replace("@COMPILER_FLAGS@", self._cxx + " " + self._cppflags);

        fh.write(settings)

    def _write_launch_files(self, launch_dir):
        bin_dir = os.path.join(self.environment.topobjdir, 'dist')

        # TODO Improve binary detection
        if self._macbundle:
            exe_path = os.path.join(bin_dir, self._macbundle, 'Contents/MacOS')
        else:
            exe_path = os.path.join(bin_dir, 'bin')

        exe_path = os.path.join(exe_path, self._appname + self._bin_suffix)

        if self.environment.substs['MOZ_WIDGET_TOOLKIT'] != 'gonk':
            main_gecko_launch = os.path.join(launch_dir, 'gecko.launch')
            with open(main_gecko_launch, 'wb') as fh:
                launch = GECKO_LAUNCH_CONFIG_TEMPLATE
                launch = launch.replace('@LAUNCH_PROGRAM@', exe_path)
                launch = launch.replace('@LAUNCH_ARGS@', '-P -no-remote')
                fh.write(launch)

        if self.environment.substs['MOZ_WIDGET_TOOLKIT'] == 'gonk':
            b2g_flash = os.path.join(launch_dir, 'b2g-flash.launch')
            with open(b2g_flash, 'wb') as fh:
                # We assume that the srcdir is inside the b2g tree.
                # If that's not the case the user can always adjust the path
                # from the eclipse IDE.
                fastxul_path = os.path.join(self.environment.topsrcdir, '..', 'scripts', 'fastxul.sh')
                launch = B2GFLASH_LAUNCH_CONFIG_TEMPLATE
                launch = launch.replace('@LAUNCH_PROGRAM@', fastxul_path)
                launch = launch.replace('@OBJDIR@', self.environment.topobjdir)
                fh.write(launch)

        #TODO Add more launch configs (and delegate calls to mach)

    def _write_project(self, fh):
        project = PROJECT_TEMPLATE;

        project = project.replace('@PROJECT_NAME@', 'Gecko')
        project = project.replace('@PROJECT_TOPSRCDIR@', self.environment.topsrcdir)
        fh.write(project)

    def _write_cproject(self, fh):
        cproject_header = CPROJECT_TEMPLATE_HEADER
        cproject_header = cproject_header.replace('@PROJECT_TOPSRCDIR@', self.environment.topobjdir)
        cproject_header = cproject_header.replace('@MACH_COMMAND@', os.path.join(self.environment.topsrcdir, 'mach'))
        fh.write(cproject_header)

        for path, defines in self._paths_to_defines.items():
            folderinfo = CPROJECT_TEMPLATE_FOLDER_INFO_HEADER
            folderinfo = folderinfo.replace('@FOLDER_ID@', str(random.randint(1000000, 99999999999)))
            folderinfo = folderinfo.replace('@FOLDER_NAME@', 'tree/' + path)
            fh.write(folderinfo)
            for k, v in defines.items():
                define = ET.Element('listOptionValue')
                define.set('builtIn', 'false')
                define.set('value', str(k) + "=" + str(v))
                fh.write(ET.tostring(define))
            fh.write(CPROJECT_TEMPLATE_FOLDER_INFO_FOOTER)


        fh.write(CPROJECT_TEMPLATE_FOOTER)


PROJECT_TEMPLATE = """<?xml version="1.0" encoding="UTF-8"?>
<projectDescription>
        <name>@PROJECT_NAME@</name>
        <comment></comment>
        <projects>
        </projects>
        <buildSpec>
                <buildCommand>
                        <name>org.eclipse.cdt.managedbuilder.core.genmakebuilder</name>
                        <triggers>clean,full,incremental,</triggers>
                        <arguments>
                        </arguments>
                </buildCommand>
                <buildCommand>
                        <name>org.eclipse.cdt.managedbuilder.core.ScannerConfigBuilder</name>
                        <triggers></triggers>
			<arguments>
			</arguments>
                </buildCommand>
        </buildSpec>
        <natures>
                <nature>org.eclipse.cdt.core.cnature</nature>
                <nature>org.eclipse.cdt.core.ccnature</nature>
                <nature>org.eclipse.cdt.managedbuilder.core.managedBuildNature</nature>
                <nature>org.eclipse.cdt.managedbuilder.core.ScannerConfigNature</nature>
        </natures>
        <linkedResources>
                <link>
                        <name>tree</name>
                        <type>2</type>
                        <location>@PROJECT_TOPSRCDIR@</location>
                </link>
        </linkedResources>
        <filteredResources>
                <filter>
                        <id>17111971</id>
                        <name>tree</name>
                        <type>30</type>
                        <matcher>
                                <id>org.eclipse.ui.ide.multiFilter</id>
                                <arguments>1.0-name-matches-false-false-obj-*</arguments>
                        </matcher>
                </filter>
                <filter>
                        <id>14081994</id>
                        <name>tree</name>
                        <type>22</type>
                        <matcher>
                                <id>org.eclipse.ui.ide.multiFilter</id>
                                <arguments>1.0-name-matches-false-false-*.rej</arguments>
                        </matcher>
                </filter>
                <filter>
                        <id>25121970</id>
                        <name>tree</name>
                        <type>22</type>
                        <matcher>
                                <id>org.eclipse.ui.ide.multiFilter</id>
                                <arguments>1.0-name-matches-false-false-*.orig</arguments>
                        </matcher>
                </filter>
                <filter>
                        <id>10102004</id>
                        <name>tree</name>
                        <type>10</type>
                        <matcher>
                                <id>org.eclipse.ui.ide.multiFilter</id>
                                <arguments>1.0-name-matches-false-false-.hg</arguments>
                        </matcher>
                </filter>
                <filter>
                        <id>23122002</id>
                        <name>tree</name>
                        <type>22</type>
                        <matcher>
                                <id>org.eclipse.ui.ide.multiFilter</id>
                                <arguments>1.0-name-matches-false-false-*.pyc</arguments>
                        </matcher>
                </filter>
        </filteredResources>
</projectDescription>
"""

CPROJECT_TEMPLATE_HEADER = """<?xml version="1.0" encoding="UTF-8" standalone="no"?>
<?fileVersion 4.0.0?>

<cproject storage_type_id="org.eclipse.cdt.core.XmlProjectDescriptionStorage">
        <storageModule moduleId="org.eclipse.cdt.core.settings">
                <cconfiguration id="0.1674256904">
                        <storageModule buildSystemId="org.eclipse.cdt.managedbuilder.core.configurationDataProvider" id="0.1674256904" moduleId="org.eclipse.cdt.core.settings" name="Default">
                                <externalSettings/>
                                <extensions>
                                        <extension id="org.eclipse.cdt.core.VCErrorParser" point="org.eclipse.cdt.core.ErrorParser"/>
                                        <extension id="org.eclipse.cdt.core.GmakeErrorParser" point="org.eclipse.cdt.core.ErrorParser"/>
                                        <extension id="org.eclipse.cdt.core.CWDLocator" point="org.eclipse.cdt.core.ErrorParser"/>
                                        <extension id="org.eclipse.cdt.core.GCCErrorParser" point="org.eclipse.cdt.core.ErrorParser"/>
                                        <extension id="org.eclipse.cdt.core.GASErrorParser" point="org.eclipse.cdt.core.ErrorParser"/>
                                        <extension id="org.eclipse.cdt.core.GLDErrorParser" point="org.eclipse.cdt.core.ErrorParser"/>
                                </extensions>
                        </storageModule>
                        <storageModule moduleId="cdtBuildSystem" version="4.0.0">
                                <configuration artifactName="${ProjName}" buildProperties="" description="" id="0.1674256904" name="Default" parent="org.eclipse.cdt.build.core.prefbase.cfg">
                                        <folderInfo id="0.1674256904." name="/" resourcePath="">
                                                <toolChain id="cdt.managedbuild.toolchain.gnu.cross.exe.debug.1276586933" name="Cross GCC" superClass="cdt.managedbuild.toolchain.gnu.cross.exe.debug">
                                                        <targetPlatform archList="all" binaryParser="org.eclipse.cdt.core.ELF" id="cdt.managedbuild.targetPlatform.gnu.cross.710759961" isAbstract="false" osList="all" superClass="cdt.managedbuild.targetPlatform.gnu.cross"/>
							<builder arguments="--log-no-times build" buildPath="@PROJECT_TOPSRCDIR@" command="@MACH_COMMAND@" enableCleanBuild="false" incrementalBuildTarget="binaries" id="org.eclipse.cdt.build.core.settings.default.builder.1437267827" keepEnvironmentInBuildfile="false" name="Gnu Make Builder" superClass="org.eclipse.cdt.build.core.settings.default.builder"/>
                                                </toolChain>
                                        </folderInfo>
"""
CPROJECT_TEMPLATE_FOLDER_INFO_HEADER = """
					<folderInfo id="0.1674256904.@FOLDER_ID@" name="/" resourcePath="@FOLDER_NAME@">
						<toolChain id="org.eclipse.cdt.build.core.prefbase.toolchain.1022318069" name="No ToolChain" superClass="org.eclipse.cdt.build.core.prefbase.toolchain" unusedChildren="">
							<tool id="org.eclipse.cdt.build.core.settings.holder.libs.1259030812" name="holder for library settings" superClass="org.eclipse.cdt.build.core.settings.holder.libs.1800697532"/>
							<tool id="org.eclipse.cdt.build.core.settings.holder.1407291069" name="GNU C++" superClass="org.eclipse.cdt.build.core.settings.holder.582514939">
								<option id="org.eclipse.cdt.build.core.settings.holder.symbols.1907658087" superClass="org.eclipse.cdt.build.core.settings.holder.symbols" valueType="definedSymbols">
"""
CPROJECT_TEMPLATE_FOLDER_INFO_DEFINE = """
									<listOptionValue builtIn="false" value="@FOLDER_DEFINE@"/>
"""
CPROJECT_TEMPLATE_FOLDER_INFO_FOOTER = """
								</option>
								<inputType id="org.eclipse.cdt.build.core.settings.holder.inType.440601711" languageId="org.eclipse.cdt.core.g++" languageName="GNU C++" sourceContentType="org.eclipse.cdt.core.cxxSource,org.eclipse.cdt.core.cxxHeader" superClass="org.eclipse.cdt.build.core.settings.holder.inType"/>
							</tool>
						</toolChain>
					</folderInfo>
"""
CPROJECT_TEMPLATE_FILEINFO = """                                        <fileInfo id="0.1674256904.474736658" name="Layers.cpp" rcbsApplicability="disable" resourcePath="tree/gfx/layers/Layers.cpp" toolsToInvoke="org.eclipse.cdt.build.core.settings.holder.582514939.463639939">
                                                <tool id="org.eclipse.cdt.build.core.settings.holder.582514939.463639939" name="GNU C++" superClass="org.eclipse.cdt.build.core.settings.holder.582514939">
                                                        <option id="org.eclipse.cdt.build.core.settings.holder.symbols.232300236" superClass="org.eclipse.cdt.build.core.settings.holder.symbols" valueType="definedSymbols">
                                                                <listOptionValue builtIn="false" value="BENWA=BENWAVAL"/>
                                                        </option>
                                                        <inputType id="org.eclipse.cdt.build.core.settings.holder.inType.1942876228" languageId="org.eclipse.cdt.core.g++" languageName="GNU C++" sourceContentType="org.eclipse.cdt.core.cxxSource,org.eclipse.cdt.core.cxxHeader" superClass="org.eclipse.cdt.build.core.settings.holder.inType"/>
                                                </tool>
                                        </fileInfo>
"""
CPROJECT_TEMPLATE_FOOTER = """                                </configuration>
                        </storageModule>
                        <storageModule moduleId="org.eclipse.cdt.core.externalSettings"/>
                </cconfiguration>
        </storageModule>
        <storageModule moduleId="cdtBuildSystem" version="4.0.0">
                <project id="Empty.null.1281234804" name="Empty"/>
        </storageModule>
        <storageModule moduleId="scannerConfiguration">
                <autodiscovery enabled="true" problemReportingEnabled="true" selectedProfileId=""/>
                <scannerConfigBuildInfo instanceId="0.1674256904">
                        <autodiscovery enabled="true" problemReportingEnabled="true" selectedProfileId=""/>
                </scannerConfigBuildInfo>
        </storageModule>
        <storageModule moduleId="refreshScope" versionNumber="2">
                <configuration configurationName="Default"/>
        </storageModule>
        <storageModule moduleId="org.eclipse.cdt.core.LanguageSettingsProviders"/>
</cproject>
"""

WORKSPACE_LANGUAGE_SETTINGS_TEMPLATE = """<?xml version="1.0" encoding="UTF-8" standalone="no"?>
<plugin>
        <extension point="org.eclipse.cdt.core.LanguageSettingsProvider">
                <provider class="org.eclipse.cdt.managedbuilder.language.settings.providers.GCCBuiltinSpecsDetector" console="true" id="org.eclipse.cdt.managedbuilder.core.GCCBuiltinSpecsDetector" keep-relative-paths="false" name="CDT GCC Built-in Compiler Settings" parameter="@COMPILER_FLAGS@ -E -P -v -dD &quot;${INPUTS}&quot;">
                        <language-scope id="org.eclipse.cdt.core.gcc"/>
                        <language-scope id="org.eclipse.cdt.core.g++"/>
                </provider>
        </extension>
</plugin>
"""

LANGUAGE_SETTINGS_TEMPLATE = """<?xml version="1.0" encoding="UTF-8" standalone="no"?>
<project>
        <configuration id="0.1674256904" name="Default">
                <extension point="org.eclipse.cdt.core.LanguageSettingsProvider">
                        <provider class="org.eclipse.cdt.core.language.settings.providers.LanguageSettingsGenericProvider" id="org.eclipse.cdt.ui.UserLanguageSettingsProvider" name="CDT User Setting Entries" prefer-non-shared="true" store-entries-with-project="true">
                                <language id="org.eclipse.cdt.core.g++">
                                        <resource project-relative-path="">
                                                <entry kind="includePath" name="@GLOBAL_INCLUDE_PATH@">
                                                        <flag value="LOCAL"/>
                                                </entry>
                                                <entry kind="includePath" name="@NSPR_INCLUDE_PATH@">
                                                        <flag value="LOCAL"/>
                                                </entry>
                                                <entry kind="includePath" name="@IPDL_INCLUDE_PATH@">
                                                        <flag value="LOCAL"/>
                                                </entry>
                                                <entry kind="includeFile" name="@PREINCLUDE_FILE_PATH@">
                                                        <flag value="LOCAL"/>
                                                </entry>
                                                <!--
                                                  Because of https://developer.mozilla.org/en-US/docs/Eclipse_CDT#Headers_are_only_parsed_once
                                                  we need to make sure headers are parsed with MOZILLA_INTERNAL_API to make sure
                                                  the indexer gets the version that is used in most of the true. This means that
                                                  MOZILLA_EXTERNAL_API code will suffer.
                                                -->
                                                @DEFINE_MOZILLA_INTERNAL_API@
                                                @DEFINE_MDCPUCFG@
                                        </resource>
                                </language>
                        </provider>
                        <provider class="org.eclipse.cdt.internal.build.crossgcc.CrossGCCBuiltinSpecsDetector" console="false" env-hash="-859273372804152468" id="org.eclipse.cdt.build.crossgcc.CrossGCCBuiltinSpecsDetector" keep-relative-paths="false" name="CDT Cross GCC Built-in Compiler Settings" parameter="@COMPILER_FLAGS@ -E -P -v -dD &quot;${INPUTS}&quot; -std=c++11" prefer-non-shared="true" store-entries-with-project="true">
                             <language-scope id="org.eclipse.cdt.core.gcc"/>
                             <language-scope id="org.eclipse.cdt.core.g++"/>
                        </provider>
                        <provider-reference id="org.eclipse.cdt.managedbuilder.core.MBSLanguageSettingsProvider" ref="shared-provider"/>
                </extension>
        </configuration>
</project>
"""

GECKO_LAUNCH_CONFIG_TEMPLATE = """<?xml version="1.0" encoding="UTF-8" standalone="no"?>
<launchConfiguration type="org.eclipse.cdt.launch.applicationLaunchType">
<booleanAttribute key="org.eclipse.cdt.dsf.gdb.AUTO_SOLIB" value="true"/>
<listAttribute key="org.eclipse.cdt.dsf.gdb.AUTO_SOLIB_LIST"/>
<stringAttribute key="org.eclipse.cdt.dsf.gdb.DEBUG_NAME" value="lldb"/>
<booleanAttribute key="org.eclipse.cdt.dsf.gdb.DEBUG_ON_FORK" value="false"/>
<stringAttribute key="org.eclipse.cdt.dsf.gdb.GDB_INIT" value=""/>
<booleanAttribute key="org.eclipse.cdt.dsf.gdb.NON_STOP" value="false"/>
<booleanAttribute key="org.eclipse.cdt.dsf.gdb.REVERSE" value="false"/>
<listAttribute key="org.eclipse.cdt.dsf.gdb.SOLIB_PATH"/>
<stringAttribute key="org.eclipse.cdt.dsf.gdb.TRACEPOINT_MODE" value="TP_NORMAL_ONLY"/>
<booleanAttribute key="org.eclipse.cdt.dsf.gdb.UPDATE_THREADLIST_ON_SUSPEND" value="false"/>
<booleanAttribute key="org.eclipse.cdt.dsf.gdb.internal.ui.launching.LocalApplicationCDebuggerTab.DEFAULTS_SET" value="true"/>
<intAttribute key="org.eclipse.cdt.launch.ATTR_BUILD_BEFORE_LAUNCH_ATTR" value="2"/>
<stringAttribute key="org.eclipse.cdt.launch.COREFILE_PATH" value=""/>
<stringAttribute key="org.eclipse.cdt.launch.DEBUGGER_ID" value="gdb"/>
<stringAttribute key="org.eclipse.cdt.launch.DEBUGGER_START_MODE" value="run"/>
<booleanAttribute key="org.eclipse.cdt.launch.DEBUGGER_STOP_AT_MAIN" value="false"/>
<stringAttribute key="org.eclipse.cdt.launch.DEBUGGER_STOP_AT_MAIN_SYMBOL" value="main"/>
<stringAttribute key="org.eclipse.cdt.launch.PROGRAM_ARGUMENTS" value="@LAUNCH_ARGS@"/>
<stringAttribute key="org.eclipse.cdt.launch.PROGRAM_NAME" value="@LAUNCH_PROGRAM@"/>
<stringAttribute key="org.eclipse.cdt.launch.PROJECT_ATTR" value="Gecko"/>
<booleanAttribute key="org.eclipse.cdt.launch.PROJECT_BUILD_CONFIG_AUTO_ATTR" value="true"/>
<stringAttribute key="org.eclipse.cdt.launch.PROJECT_BUILD_CONFIG_ID_ATTR" value=""/>
<booleanAttribute key="org.eclipse.cdt.launch.use_terminal" value="true"/>
<listAttribute key="org.eclipse.debug.core.MAPPED_RESOURCE_PATHS">
<listEntry value="/gecko"/>
</listAttribute>
<listAttribute key="org.eclipse.debug.core.MAPPED_RESOURCE_TYPES">
<listEntry value="4"/>
</listAttribute>
<booleanAttribute key="org.eclipse.debug.ui.ATTR_LAUNCH_IN_BACKGROUND" value="false"/>
<stringAttribute key="process_factory_id" value="org.eclipse.cdt.dsf.gdb.GdbProcessFactory"/>
</launchConfiguration>
"""

B2GFLASH_LAUNCH_CONFIG_TEMPLATE = """<?xml version="1.0" encoding="UTF-8" standalone="no"?>
<launchConfiguration type="org.eclipse.cdt.launch.applicationLaunchType">
<booleanAttribute key="org.eclipse.cdt.dsf.gdb.AUTO_SOLIB" value="true"/>
<listAttribute key="org.eclipse.cdt.dsf.gdb.AUTO_SOLIB_LIST"/>
<stringAttribute key="org.eclipse.cdt.dsf.gdb.DEBUG_NAME" value="lldb"/>
<booleanAttribute key="org.eclipse.cdt.dsf.gdb.DEBUG_ON_FORK" value="false"/>
<stringAttribute key="org.eclipse.cdt.dsf.gdb.GDB_INIT" value=""/>
<booleanAttribute key="org.eclipse.cdt.dsf.gdb.NON_STOP" value="false"/>
<booleanAttribute key="org.eclipse.cdt.dsf.gdb.REVERSE" value="false"/>
<listAttribute key="org.eclipse.cdt.dsf.gdb.SOLIB_PATH"/>
<stringAttribute key="org.eclipse.cdt.dsf.gdb.TRACEPOINT_MODE" value="TP_NORMAL_ONLY"/>
<booleanAttribute key="org.eclipse.cdt.dsf.gdb.UPDATE_THREADLIST_ON_SUSPEND" value="false"/>
<booleanAttribute key="org.eclipse.cdt.dsf.gdb.internal.ui.launching.LocalApplicationCDebuggerTab.DEFAULTS_SET" value="true"/>
<intAttribute key="org.eclipse.cdt.launch.ATTR_BUILD_BEFORE_LAUNCH_ATTR" value="2"/>
<stringAttribute key="org.eclipse.cdt.launch.COREFILE_PATH" value=""/>
<stringAttribute key="org.eclipse.cdt.launch.DEBUGGER_ID" value="gdb"/>
<stringAttribute key="org.eclipse.cdt.launch.DEBUGGER_START_MODE" value="run"/>
<booleanAttribute key="org.eclipse.cdt.launch.DEBUGGER_STOP_AT_MAIN" value="false"/>
<stringAttribute key="org.eclipse.cdt.launch.DEBUGGER_STOP_AT_MAIN_SYMBOL" value="main"/>
<stringAttribute key="org.eclipse.cdt.launch.PROGRAM_NAME" value="@LAUNCH_PROGRAM@"/>
<stringAttribute key="org.eclipse.cdt.launch.PROJECT_ATTR" value="Gecko"/>
<booleanAttribute key="org.eclipse.cdt.launch.PROJECT_BUILD_CONFIG_AUTO_ATTR" value="true"/>
<stringAttribute key="org.eclipse.cdt.launch.PROJECT_BUILD_CONFIG_ID_ATTR" value=""/>
<stringAttribute key="org.eclipse.cdt.launch.WORKING_DIRECTORY" value="@OBJDIR@"/>
<booleanAttribute key="org.eclipse.cdt.launch.use_terminal" value="true"/>
<listAttribute key="org.eclipse.debug.core.MAPPED_RESOURCE_PATHS">
<listEntry value="/gecko"/>
</listAttribute>
<listAttribute key="org.eclipse.debug.core.MAPPED_RESOURCE_TYPES">
<listEntry value="4"/>
</listAttribute>
<booleanAttribute key="org.eclipse.debug.ui.ATTR_LAUNCH_IN_BACKGROUND" value="false"/>
<stringAttribute key="process_factory_id" value="org.eclipse.cdt.dsf.gdb.GdbProcessFactory"/>
</launchConfiguration>
"""


EDITOR_SETTINGS = """eclipse.preferences.version=1
lineNumberRuler=true
overviewRuler_migration=migrated_3.1
printMargin=true
printMarginColumn=80
showCarriageReturn=false
showEnclosedSpaces=false
showLeadingSpaces=false
showLineFeed=false
showWhitespaceCharacters=true
spacesForTabs=true
tabWidth=2
undoHistorySize=200
"""

FORMATTER_SETTINGS = """eclipse.preferences.version=1
org.eclipse.cdt.core.formatter.alignment_for_arguments_in_method_invocation=16
org.eclipse.cdt.core.formatter.alignment_for_assignment=16
org.eclipse.cdt.core.formatter.alignment_for_base_clause_in_type_declaration=80
org.eclipse.cdt.core.formatter.alignment_for_binary_expression=16
org.eclipse.cdt.core.formatter.alignment_for_compact_if=16
org.eclipse.cdt.core.formatter.alignment_for_conditional_expression=34
org.eclipse.cdt.core.formatter.alignment_for_conditional_expression_chain=18
org.eclipse.cdt.core.formatter.alignment_for_constructor_initializer_list=48
org.eclipse.cdt.core.formatter.alignment_for_declarator_list=16
org.eclipse.cdt.core.formatter.alignment_for_enumerator_list=48
org.eclipse.cdt.core.formatter.alignment_for_expression_list=0
org.eclipse.cdt.core.formatter.alignment_for_expressions_in_array_initializer=16
org.eclipse.cdt.core.formatter.alignment_for_member_access=0
org.eclipse.cdt.core.formatter.alignment_for_overloaded_left_shift_chain=16
org.eclipse.cdt.core.formatter.alignment_for_parameters_in_method_declaration=16
org.eclipse.cdt.core.formatter.alignment_for_throws_clause_in_method_declaration=16
org.eclipse.cdt.core.formatter.brace_position_for_array_initializer=end_of_line
org.eclipse.cdt.core.formatter.brace_position_for_block=end_of_line
org.eclipse.cdt.core.formatter.brace_position_for_block_in_case=next_line_shifted
org.eclipse.cdt.core.formatter.brace_position_for_method_declaration=next_line
org.eclipse.cdt.core.formatter.brace_position_for_namespace_declaration=end_of_line
org.eclipse.cdt.core.formatter.brace_position_for_switch=end_of_line
org.eclipse.cdt.core.formatter.brace_position_for_type_declaration=next_line
org.eclipse.cdt.core.formatter.comment.min_distance_between_code_and_line_comment=1
org.eclipse.cdt.core.formatter.comment.never_indent_line_comments_on_first_column=true
org.eclipse.cdt.core.formatter.comment.preserve_white_space_between_code_and_line_comments=true
org.eclipse.cdt.core.formatter.compact_else_if=true
org.eclipse.cdt.core.formatter.continuation_indentation=2
org.eclipse.cdt.core.formatter.continuation_indentation_for_array_initializer=2
org.eclipse.cdt.core.formatter.format_guardian_clause_on_one_line=false
org.eclipse.cdt.core.formatter.indent_access_specifier_compare_to_type_header=false
org.eclipse.cdt.core.formatter.indent_access_specifier_extra_spaces=0
org.eclipse.cdt.core.formatter.indent_body_declarations_compare_to_access_specifier=true
org.eclipse.cdt.core.formatter.indent_body_declarations_compare_to_namespace_header=false
org.eclipse.cdt.core.formatter.indent_breaks_compare_to_cases=true
org.eclipse.cdt.core.formatter.indent_declaration_compare_to_template_header=true
org.eclipse.cdt.core.formatter.indent_empty_lines=false
org.eclipse.cdt.core.formatter.indent_statements_compare_to_block=true
org.eclipse.cdt.core.formatter.indent_statements_compare_to_body=true
org.eclipse.cdt.core.formatter.indent_switchstatements_compare_to_cases=true
org.eclipse.cdt.core.formatter.indent_switchstatements_compare_to_switch=false
org.eclipse.cdt.core.formatter.indentation.size=2
org.eclipse.cdt.core.formatter.insert_new_line_after_opening_brace_in_array_initializer=do not insert
org.eclipse.cdt.core.formatter.insert_new_line_after_template_declaration=insert
org.eclipse.cdt.core.formatter.insert_new_line_at_end_of_file_if_missing=do not insert
org.eclipse.cdt.core.formatter.insert_new_line_before_catch_in_try_statement=do not insert
org.eclipse.cdt.core.formatter.insert_new_line_before_closing_brace_in_array_initializer=do not insert
org.eclipse.cdt.core.formatter.insert_new_line_before_colon_in_constructor_initializer_list=do not insert
org.eclipse.cdt.core.formatter.insert_new_line_before_else_in_if_statement=do not insert
org.eclipse.cdt.core.formatter.insert_new_line_before_identifier_in_function_declaration=insert
org.eclipse.cdt.core.formatter.insert_new_line_before_while_in_do_statement=do not insert
org.eclipse.cdt.core.formatter.insert_new_line_in_empty_block=insert
org.eclipse.cdt.core.formatter.insert_space_after_assignment_operator=insert
org.eclipse.cdt.core.formatter.insert_space_after_binary_operator=insert
org.eclipse.cdt.core.formatter.insert_space_after_closing_angle_bracket_in_template_arguments=insert
org.eclipse.cdt.core.formatter.insert_space_after_closing_angle_bracket_in_template_parameters=insert
org.eclipse.cdt.core.formatter.insert_space_after_closing_brace_in_block=insert
org.eclipse.cdt.core.formatter.insert_space_after_closing_paren_in_cast=insert
org.eclipse.cdt.core.formatter.insert_space_after_colon_in_base_clause=insert
org.eclipse.cdt.core.formatter.insert_space_after_colon_in_case=insert
org.eclipse.cdt.core.formatter.insert_space_after_colon_in_conditional=insert
org.eclipse.cdt.core.formatter.insert_space_after_colon_in_labeled_statement=insert
org.eclipse.cdt.core.formatter.insert_space_after_comma_in_array_initializer=insert
org.eclipse.cdt.core.formatter.insert_space_after_comma_in_base_types=insert
org.eclipse.cdt.core.formatter.insert_space_after_comma_in_declarator_list=insert
org.eclipse.cdt.core.formatter.insert_space_after_comma_in_enum_declarations=insert
org.eclipse.cdt.core.formatter.insert_space_after_comma_in_expression_list=insert
org.eclipse.cdt.core.formatter.insert_space_after_comma_in_method_declaration_parameters=insert
org.eclipse.cdt.core.formatter.insert_space_after_comma_in_method_declaration_throws=insert
org.eclipse.cdt.core.formatter.insert_space_after_comma_in_method_invocation_arguments=insert
org.eclipse.cdt.core.formatter.insert_space_after_comma_in_template_arguments=insert
org.eclipse.cdt.core.formatter.insert_space_after_comma_in_template_parameters=insert
org.eclipse.cdt.core.formatter.insert_space_after_opening_angle_bracket_in_template_arguments=do not insert
org.eclipse.cdt.core.formatter.insert_space_after_opening_angle_bracket_in_template_parameters=do not insert
org.eclipse.cdt.core.formatter.insert_space_after_opening_brace_in_array_initializer=insert
org.eclipse.cdt.core.formatter.insert_space_after_opening_bracket=do not insert
org.eclipse.cdt.core.formatter.insert_space_after_opening_paren_in_cast=do not insert
org.eclipse.cdt.core.formatter.insert_space_after_opening_paren_in_catch=do not insert
org.eclipse.cdt.core.formatter.insert_space_after_opening_paren_in_exception_specification=do not insert
org.eclipse.cdt.core.formatter.insert_space_after_opening_paren_in_for=do not insert
org.eclipse.cdt.core.formatter.insert_space_after_opening_paren_in_if=do not insert
org.eclipse.cdt.core.formatter.insert_space_after_opening_paren_in_method_declaration=do not insert
org.eclipse.cdt.core.formatter.insert_space_after_opening_paren_in_method_invocation=do not insert
org.eclipse.cdt.core.formatter.insert_space_after_opening_paren_in_parenthesized_expression=do not insert
org.eclipse.cdt.core.formatter.insert_space_after_opening_paren_in_switch=do not insert
org.eclipse.cdt.core.formatter.insert_space_after_opening_paren_in_while=do not insert
org.eclipse.cdt.core.formatter.insert_space_after_postfix_operator=do not insert
org.eclipse.cdt.core.formatter.insert_space_after_prefix_operator=do not insert
org.eclipse.cdt.core.formatter.insert_space_after_question_in_conditional=insert
org.eclipse.cdt.core.formatter.insert_space_after_semicolon_in_for=insert
org.eclipse.cdt.core.formatter.insert_space_after_unary_operator=do not insert
org.eclipse.cdt.core.formatter.insert_space_before_assignment_operator=insert
org.eclipse.cdt.core.formatter.insert_space_before_binary_operator=insert
org.eclipse.cdt.core.formatter.insert_space_before_closing_angle_bracket_in_template_arguments=do not insert
org.eclipse.cdt.core.formatter.insert_space_before_closing_angle_bracket_in_template_parameters=do not insert
org.eclipse.cdt.core.formatter.insert_space_before_closing_brace_in_array_initializer=insert
org.eclipse.cdt.core.formatter.insert_space_before_closing_bracket=do not insert
org.eclipse.cdt.core.formatter.insert_space_before_closing_paren_in_cast=do not insert
org.eclipse.cdt.core.formatter.insert_space_before_closing_paren_in_catch=do not insert
org.eclipse.cdt.core.formatter.insert_space_before_closing_paren_in_exception_specification=do not insert
org.eclipse.cdt.core.formatter.insert_space_before_closing_paren_in_for=do not insert
org.eclipse.cdt.core.formatter.insert_space_before_closing_paren_in_if=do not insert
org.eclipse.cdt.core.formatter.insert_space_before_closing_paren_in_method_declaration=do not insert
org.eclipse.cdt.core.formatter.insert_space_before_closing_paren_in_method_invocation=do not insert
org.eclipse.cdt.core.formatter.insert_space_before_closing_paren_in_parenthesized_expression=do not insert
org.eclipse.cdt.core.formatter.insert_space_before_closing_paren_in_switch=do not insert
org.eclipse.cdt.core.formatter.insert_space_before_closing_paren_in_while=do not insert
org.eclipse.cdt.core.formatter.insert_space_before_colon_in_base_clause=insert
org.eclipse.cdt.core.formatter.insert_space_before_colon_in_case=do not insert
org.eclipse.cdt.core.formatter.insert_space_before_colon_in_conditional=insert
org.eclipse.cdt.core.formatter.insert_space_before_colon_in_default=do not insert
org.eclipse.cdt.core.formatter.insert_space_before_colon_in_labeled_statement=do not insert
org.eclipse.cdt.core.formatter.insert_space_before_comma_in_array_initializer=do not insert
org.eclipse.cdt.core.formatter.insert_space_before_comma_in_base_types=do not insert
org.eclipse.cdt.core.formatter.insert_space_before_comma_in_declarator_list=do not insert
org.eclipse.cdt.core.formatter.insert_space_before_comma_in_enum_declarations=do not insert
org.eclipse.cdt.core.formatter.insert_space_before_comma_in_expression_list=do not insert
org.eclipse.cdt.core.formatter.insert_space_before_comma_in_method_declaration_parameters=do not insert
org.eclipse.cdt.core.formatter.insert_space_before_comma_in_method_declaration_throws=do not insert
org.eclipse.cdt.core.formatter.insert_space_before_comma_in_method_invocation_arguments=do not insert
org.eclipse.cdt.core.formatter.insert_space_before_comma_in_template_arguments=do not insert
org.eclipse.cdt.core.formatter.insert_space_before_comma_in_template_parameters=do not insert
org.eclipse.cdt.core.formatter.insert_space_before_opening_angle_bracket_in_template_arguments=do not insert
org.eclipse.cdt.core.formatter.insert_space_before_opening_angle_bracket_in_template_parameters=do not insert
org.eclipse.cdt.core.formatter.insert_space_before_opening_brace_in_array_initializer=insert
org.eclipse.cdt.core.formatter.insert_space_before_opening_brace_in_block=insert
org.eclipse.cdt.core.formatter.insert_space_before_opening_brace_in_method_declaration=insert
org.eclipse.cdt.core.formatter.insert_space_before_opening_brace_in_namespace_declaration=insert
org.eclipse.cdt.core.formatter.insert_space_before_opening_brace_in_switch=insert
org.eclipse.cdt.core.formatter.insert_space_before_opening_brace_in_type_declaration=insert
org.eclipse.cdt.core.formatter.insert_space_before_opening_bracket=do not insert
org.eclipse.cdt.core.formatter.insert_space_before_opening_paren_in_catch=insert
org.eclipse.cdt.core.formatter.insert_space_before_opening_paren_in_exception_specification=insert
org.eclipse.cdt.core.formatter.insert_space_before_opening_paren_in_for=insert
org.eclipse.cdt.core.formatter.insert_space_before_opening_paren_in_if=insert
org.eclipse.cdt.core.formatter.insert_space_before_opening_paren_in_method_declaration=do not insert
org.eclipse.cdt.core.formatter.insert_space_before_opening_paren_in_method_invocation=do not insert
org.eclipse.cdt.core.formatter.insert_space_before_opening_paren_in_parenthesized_expression=do not insert
org.eclipse.cdt.core.formatter.insert_space_before_opening_paren_in_switch=insert
org.eclipse.cdt.core.formatter.insert_space_before_opening_paren_in_while=insert
org.eclipse.cdt.core.formatter.insert_space_before_postfix_operator=do not insert
org.eclipse.cdt.core.formatter.insert_space_before_prefix_operator=do not insert
org.eclipse.cdt.core.formatter.insert_space_before_question_in_conditional=insert
org.eclipse.cdt.core.formatter.insert_space_before_semicolon=do not insert
org.eclipse.cdt.core.formatter.insert_space_before_semicolon_in_for=do not insert
org.eclipse.cdt.core.formatter.insert_space_before_unary_operator=do not insert
org.eclipse.cdt.core.formatter.insert_space_between_empty_braces_in_array_initializer=do not insert
org.eclipse.cdt.core.formatter.insert_space_between_empty_brackets=do not insert
org.eclipse.cdt.core.formatter.insert_space_between_empty_parens_in_exception_specification=do not insert
org.eclipse.cdt.core.formatter.insert_space_between_empty_parens_in_method_declaration=do not insert
org.eclipse.cdt.core.formatter.insert_space_between_empty_parens_in_method_invocation=do not insert
org.eclipse.cdt.core.formatter.join_wrapped_lines=false
org.eclipse.cdt.core.formatter.keep_else_statement_on_same_line=false
org.eclipse.cdt.core.formatter.keep_empty_array_initializer_on_one_line=false
org.eclipse.cdt.core.formatter.keep_imple_if_on_one_line=false
org.eclipse.cdt.core.formatter.keep_then_statement_on_same_line=false
org.eclipse.cdt.core.formatter.lineSplit=80
org.eclipse.cdt.core.formatter.number_of_empty_lines_to_preserve=1
org.eclipse.cdt.core.formatter.put_empty_statement_on_new_line=true
org.eclipse.cdt.core.formatter.tabulation.char=space
org.eclipse.cdt.core.formatter.tabulation.size=2
org.eclipse.cdt.core.formatter.use_tabs_only_for_leading_indentations=false
"""
