import os
import json
import re

from mozharness.base.errors import MakefileErrorList
from mozharness.mozilla.buildbot import TBPL_WARNING


class HazardError(Exception):
    def __init__(self, value):
        self.value = value

    def __str__(self):
        return repr(self.value)

    # Logging ends up calling splitlines directly on what is being logged, which would fail.
    def splitlines(self):
        return str(self).splitlines()

class HazardAnalysis(object):
    def clobber_shell(self, builder):
        """Clobber the specially-built JS shell used to run the analysis"""
        dirs = builder.query_abs_dirs()
        builder.rmtree(dirs['shell_objdir'])

    def configure_shell(self, builder):
        """Configure the specially-built JS shell used to run the analysis"""
        dirs = builder.query_abs_dirs()

        if not os.path.exists(dirs['shell_objdir']):
            builder.mkdir_p(dirs['shell_objdir'])

        js_src_dir = os.path.join(dirs['gecko_src'], 'js', 'src')
        rc = builder.run_command(['autoconf-2.13'],
                              cwd=js_src_dir,
                              env=builder.env,
                              error_list=MakefileErrorList)
        if rc != 0:
            raise HazardError("autoconf failed, can't continue.")

        rc = builder.run_command([os.path.join(js_src_dir, 'configure'),
                               '--enable-optimize',
                               '--disable-debug',
                               '--enable-ctypes',
                               '--with-system-nspr',
                               '--without-intl-api'],
                              cwd=dirs['shell_objdir'],
                              env=builder.env,
                              error_list=MakefileErrorList)
        if rc != 0:
            raise HazardError("Configure failed, can't continue.")

    def build_shell(self, builder):
        """Build a JS shell specifically for running the analysis"""
        dirs = builder.query_abs_dirs()

        rc = builder.run_command(['make', '-j', str(builder.config.get('concurrency', 4)), '-s'],
                              cwd=dirs['shell_objdir'],
                              env=builder.env,
                              error_list=MakefileErrorList)
        if rc != 0:
            raise HazardError("Build failed, can't continue.")

    def clobber(self, builder):
        """Clobber all of the old analysis data. Note that theoretically we could do
           incremental analyses, but they seem to still be buggy."""
        dirs = builder.query_abs_dirs()
        builder.rmtree(dirs['abs_analysis_dir'])
        builder.rmtree(dirs['abs_analyzed_objdir'])

    def setup(self, builder):
        """Prepare the config files and scripts for running the analysis"""
        dirs = builder.query_abs_dirs()
        analysis_dir = dirs['abs_analysis_dir']

        if not os.path.exists(analysis_dir):
            builder.mkdir_p(analysis_dir)

        js_src_dir = os.path.join(dirs['gecko_src'], 'js', 'src')

        values = {
            'js': os.path.join(dirs['shell_objdir'], 'dist', 'bin', 'js'),
            'analysis_scriptdir': os.path.join(js_src_dir, 'devtools', 'rootAnalysis'),
            'source_objdir': dirs['abs_analyzed_objdir'],
            'source': os.path.join(dirs['abs_work_dir'], 'source'),
            'sixgill': os.path.join(dirs['abs_work_dir'], builder.config['sixgill']),
            'sixgill_bin': os.path.join(dirs['abs_work_dir'], builder.config['sixgill_bin']),
            'gcc_bin': os.path.join(dirs['abs_work_dir'], 'gcc'),
        }
        defaults = """
js = '%(js)s'
analysis_scriptdir = '%(analysis_scriptdir)s'
objdir = '%(source_objdir)s'
source = '%(source)s'
sixgill = '%(sixgill)s'
sixgill_bin = '%(sixgill_bin)s'
gcc_bin = '%(gcc_bin)s'
jobs = 4
""" % values

        defaults_path = os.path.join(analysis_dir, 'defaults.py')
        file(defaults_path, "w").write(defaults)
        builder.log("Wrote analysis config file " + defaults_path)

        build_script = builder.config['build_command']
        builder.copyfile(os.path.join(dirs['mozharness_scriptdir'],
                                   os.path.join('spidermonkey', build_script)),
                      os.path.join(analysis_dir, build_script),
                      copystat=True)

    def run(self, builder, env, error_list):
        """Execute the analysis, which consists of building all analyzed
        source code with a GCC plugin active that siphons off the interesting
        data, then running some JS scripts over the databases created by
        the plugin."""
        dirs = builder.query_abs_dirs()
        analysis_dir = dirs['abs_analysis_dir']
        analysis_scriptdir = os.path.join(dirs['abs_work_dir'], dirs['analysis_scriptdir'])

        build_script = builder.config['build_command']
        build_script = os.path.abspath(os.path.join(analysis_dir, build_script))

        cmd = [
            builder.config['python'],
            os.path.join(analysis_scriptdir, 'analyze.py'),
            "--source", dirs['gecko_src'],
            "--buildcommand", build_script,
        ]
        retval = builder.run_command(cmd,
                                  cwd=analysis_dir,
                                  env=env,
                                  error_list=error_list)
        if retval != 0:
            raise HazardError("failed to build")

    def collect_output(self, builder):
        """Gather up the analysis output and place in the upload dir."""
        dirs = builder.query_abs_dirs()
        analysis_dir = dirs['abs_analysis_dir']
        upload_dir = dirs['abs_upload_dir']
        builder.mkdir_p(upload_dir)
        files = (('rootingHazards.txt',
                  'rooting_hazards',
                  'list of rooting hazards, unsafe references, and extra roots'),
                 ('gcFunctions.txt',
                  'gcFunctions',
                  'list of functions that can gc, and why'),
                 ('allFunctions.txt',
                  'allFunctions',
                  'list of all functions that were compiled'),
                 ('gcTypes.txt',
                  'gcTypes',
                  'list of types containing unrooted gc pointers'),
                 ('unnecessary.txt',
                  'extra',
                  'list of extra roots (rooting with no GC function in scope)'),
                 ('refs.txt',
                  'refs',
                  'list of unsafe references to unrooted pointers'),
                 ('hazards.txt',
                  'hazards',
                  'list of just the hazards, together with gcFunction reason for each'))
        for f, short, long in files:
            builder.copy_to_upload_dir(os.path.join(analysis_dir, f),
                                    short_desc=short,
                                    long_desc=long,
                                    compress=True)

    def upload_results(self, builder):
        """Upload the results of the analysis."""
        dirs = builder.query_abs_dirs()
        upload_path = builder.query_upload_path()

        retval = builder.rsync_upload_directory(
            dirs['abs_upload_dir'],
            builder.query_upload_ssh_key(),
            builder.query_upload_ssh_user(),
            builder.query_upload_ssh_server(),
            upload_path
        )

        if retval is not None:
            raise HazardError("failed to upload")

        upload_url = "{baseuri}{upload_path}".format(
            baseuri=builder.query_upload_remote_baseuri(),
            upload_path=upload_path,
        )
        builder.info("TinderboxPrint: upload <a title='hazards_results' href='%s'>results</a>: complete" % upload_url)

    def check_expectations(self, builder):
        """Compare the actual to expected number of problems."""
        if 'expect_file' not in builder.config:
            builder.info('No expect_file given; skipping comparison with expected hazard count')
            return

        dirs = builder.query_abs_dirs()
        analysis_dir = dirs['abs_analysis_dir']
        analysis_scriptdir = os.path.join(dirs['gecko_src'], 'js', 'src', 'devtools', 'rootAnalysis')
        expect_file = os.path.join(analysis_scriptdir, builder.config['expect_file'])
        expect = builder.read_from_file(expect_file)
        if expect is None:
            raise HazardError("could not load expectation file")
        data = json.loads(expect)

        num_hazards = 0
        num_refs = 0
        with builder.opened(os.path.join(analysis_dir, "rootingHazards.txt")) as (hazards_fh, err):
            if err:
                raise HazardError("hazards file required")
            for line in hazards_fh:
                m = re.match(r"^Function.*has unrooted.*live across GC call", line)
                if m:
                    num_hazards += 1

                m = re.match(r'^Function.*takes unsafe address of unrooted', line)
                if m:
                    num_refs += 1

        expect_hazards = data.get('expect-hazards')
        status = []
        if expect_hazards is None:
            status.append("%d hazards" % num_hazards)
        else:
            status.append("%d/%d hazards allowed" % (num_hazards, expect_hazards))

        if expect_hazards is not None and expect_hazards != num_hazards:
            if expect_hazards < num_hazards:
                builder.warning("TEST-UNEXPECTED-FAIL %d more hazards than expected (expected %d, saw %d)" %
                             (num_hazards - expect_hazards, expect_hazards, num_hazards))
                builder.buildbot_status(TBPL_WARNING)
            else:
                builder.info("%d fewer hazards than expected! (expected %d, saw %d)" %
                          (expect_hazards - num_hazards, expect_hazards, num_hazards))

        expect_refs = data.get('expect-refs')
        if expect_refs is None:
            status.append("%d unsafe refs" % num_refs)
        else:
            status.append("%d/%d unsafe refs allowed" % (num_refs, expect_refs))

        if expect_refs is not None and expect_refs != num_refs:
            if expect_refs < num_refs:
                builder.warning("TEST-UNEXPECTED-FAIL %d more unsafe refs than expected (expected %d, saw %d)" %
                             (num_refs - expect_refs, expect_refs, num_refs))
                builder.buildbot_status(TBPL_WARNING)
            else:
                builder.info("%d fewer unsafe refs than expected! (expected %d, saw %d)" %
                          (expect_refs - num_refs, expect_refs, num_refs))

        builder.info("TinderboxPrint: " + ", ".join(status))
