#!/usr/bin/python
#
# ***** BEGIN LICENSE BLOCK *****
# Version: MPL 1.1/GPL 2.0/LGPL 2.1
#
# The contents of this file are subject to the Mozilla Public License Version
# 1.1 (the "License"); you may not use this file except in compliance with
# the License. You may obtain a copy of the License at
# http://www.mozilla.org/MPL/
#
# Software distributed under the License is distributed on an "AS IS" basis,
# WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
# for the specific language governing rights and limitations under the
# License.
#
# The Original Code is standalone Firefox memory test.
#
# The Initial Developer of the Original Code is
# Mozilla Corporation.
# Portions created by the Initial Developer are Copyright (C) 2006
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#   Graydon Hoare <graydon@mozilla.com> (original author)
#
# Alternatively, the contents of this file may be used under the terms of
# either the GNU General Public License Version 2 or later (the "GPL"), or
# the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
# in which case the provisions of the GPL or the LGPL are applicable instead
# of those above. If you wish to allow use of your version of this file only
# under the terms of either the GPL or the LGPL, and not to allow others to
# use your version of this file under the terms of the MPL, indicate your
# decision by deleting the provisions above and replace them with the notice
# and other provisions required by the GPL or the LGPL. If you do not delete
# the provisions above, a recipient may use your version of this file under
# the terms of any one of the MPL, the GPL or the LGPL.
#
# ***** END LICENSE BLOCK *****


import subprocess
import tempfile
import os
import re
import getpass
import datetime
import logging
import time
import sys
import signal
import shutil
import glob

def sighandler(signal, frame):
        print "signal %d, throwing" % signal
        raise Exception

signal.signal(signal.SIGHUP, sighandler)


########################################################################

def start_xvnc(disp, tmpdir):
    xvnc_stdout = open(os.path.join(tmpdir, "xvnc.stdout"), mode="w")
    xvnc_stderr = open(os.path.join(tmpdir, "xvnc.stderr"), mode="w")
    xvnc_proc = subprocess.Popen (["Xvnc",
                   "-fp", "/usr/share/X11/fonts/misc",
                                   "-localhost",
                                   "-SecurityTypes", "None",
                   "-IdleTimeout", "604800",
                                   "-depth",         "24",
                                   "-ac",
                                   (":%d" % disp)],
                                  cwd=tmpdir,
                                  shell=False,
                                  stdout=xvnc_stdout,
                                  stderr=xvnc_stderr)
    time.sleep(3)
    assert xvnc_proc.poll() == None
    logging.info("started Xvnc server (pid %d) on display :%d"
                 % (xvnc_proc.pid, disp))
    return xvnc_proc

########################################################################

def large_variant_filename(variant):
    return variant + "-large.swf"

def large_variant_html_filename(variant):
    return variant + "-large.html"

def small_variant_filename(variant):
    return variant + ".swf"

def start_xvnc_recorder(vnc2swfpath, disp, variant, tmpdir):
    rec_stdout = open(os.path.join(tmpdir, "vnc2swf.stdout"), mode="w")
    rec_stderr = open(os.path.join(tmpdir, "vnc2swf.stderr"), mode="w")
    rec_proc = subprocess.Popen ([os.path.join(vnc2swfpath, "vnc2swf.py"),
                  "-n",
                  "-o", large_variant_filename(variant),
                  "-C", "800x600+0+100",
                  "-r", "2",
                  "-t", "video",
                  "localhost:%d" % disp],
                                  cwd=tmpdir,
                                  shell=False,
                                  stdout=rec_stdout,
                                  stderr=rec_stderr)
    time.sleep(3)
    assert rec_proc.poll() == None
    logging.info("started vnc2swf recorder (pid %d) on display :%d"
                 % (rec_proc.pid, disp))
    return rec_proc

def complete_xvnc_recording(vnc2swfpath, proc, variant, tmpdir):
    edit_stdout = open(os.path.join(tmpdir, "edit.stdout"), mode="w")
    edit_stderr = open(os.path.join(tmpdir, "edit.stderr"), mode="w")
    logging.info("killing vnc2swf recorder (pid %d)" % proc.pid)
    os.kill(proc.pid, signal.SIGINT)
    proc.wait()
    logging.info("down-sampling video")
    subprocess.Popen([os.path.join(vnc2swfpath, "edit.py"),
            "-c",
            "-o", small_variant_filename(variant),
            "-t", "video",
            "-s", "0.5",
            "-r", "32",
            large_variant_filename(variant)],
                        cwd=tmpdir,
                        shell=False,
                        stdout=edit_stdout,
                        stderr=edit_stderr).wait()
    #os.unlink(large_variant_html_filename(variant))
    #os.unlink(large_variant_filename(variant))
    logging.info("output video is in " + small_variant_filename(variant))
    

########################################################################

def write_vgopts(tmpdir, vgopts):
    f = open(os.path.join(tmpdir, ".valgrindrc"), "w")
    for i in vgopts:
        f.write(i + "\n")
    f.close()

########################################################################

class Firefox_runner:
    def __init__(this, batchprefix, homedir, ffdir, timestr, tmpdir, disp):
        this.tmpdir = tmpdir
        this.homedir = homedir
        this.ffdir = ffdir
        this.profile = batchprefix + timestr
        this.profile_dir = os.path.join(this.tmpdir, this.profile)
        this.bin = os.path.join(this.ffdir, "firefox-bin")
        this.environ = {
            "PATH"            : os.getenv("PATH"),
            "HOME"            : this.homedir,
            "LD_LIBRARY_PATH" : this.ffdir,
            "MOZ_NO_REMOTE"   : "1",
#            "DISPLAY"         : ":0",
            "DISPLAY"         : (":%d" % disp),
            }

        this.kill_initial_profiles_ini()
        this.create_tmp_profile()
        this.write_proxy_pac_file()
        this.write_user_prefs()
        this.perform_initial_registration()

    def kill_initial_profiles_ini(this):
        prof = os.path.join(this.homedir, ".mozilla")
        shutil.rmtree(prof, True)
        os.mkdir(prof)


    def clear_cache(this):
        shutil.rmtree(os.path.join(this.profile_dir, "Cache"), True)

    def create_tmp_profile(this):
        subprocess.Popen ([this.bin, "-CreateProfile", (this.profile + " " + this.profile_dir)],
                          cwd=this.tmpdir, shell=False, env=this.environ).wait()

    def write_proxy_pac_file(this):
        this.proxypac = os.path.join(this.tmpdir, "proxy.pac")
        p = open(this.proxypac, "w")
        p.write("function FindProxyForURL(url, host) {\n"
                "  if (host == \"localhost\")\n"
                "    return \"DIRECT\";\n"
                "  else\n"
                "    return \"PROXY localhost\";\n"
                "}\n")
        p.close()

    def write_user_prefs(this):
        userprefs = open(os.path.join(this.profile_dir, "user.js"), "w")
        userprefs.write("user_pref(\"network.proxy.autoconfig_url\", \"file://%s\");\n" % this.proxypac)
        userprefs.write("user_pref(\"network.proxy.type\", 2);\n")
        userprefs.write("user_pref(\"dom.max_script_run_time\", 0);\n")
        userprefs.write("user_pref(\"hangmonitor.timeout\", 0);\n");
        userprefs.write("user_pref(\"dom.allow_scripts_to_close_windows\", true);\n")
        userprefs.close()

    def perform_initial_registration(this):
        dummy_file = os.path.join(this.tmpdir, "dummy.html")
        f = open(dummy_file, "w")
        f.write("<html><body onload=\"window.close()\" /></html>\n")
        f.close()
        logging.info("running dummy firefox under profile, for initial component-registration")
        subprocess.Popen ([this.bin, "-P", this.profile, "file://" + dummy_file],
                          cwd=this.tmpdir, shell=False, env=this.environ).wait()

    def run_normal(this, url):
        ff_proc = subprocess.Popen ([this.bin, "-P", this.profile, url],
                                    cwd=this.tmpdir, shell=False, env=this.environ)
        logging.info("started 'firefox-bin ... %s' process (pid %d)"
                     % (url, ff_proc.pid))
        return ff_proc


    def run_valgrind(this, vg_tool, url):
        vg_proc = subprocess.Popen (["valgrind",
                                     "--tool=" + vg_tool,
                                     "--log-file=valgrind-" + vg_tool + "-log",
                                     this.bin, "-P", this.profile, url],
                                     cwd=this.tmpdir, shell=False, env=this.environ)
        logging.info("started 'valgrind --tool=%s firefox-bin ... %s' process (pid %d)"
                     % (vg_tool, url, vg_proc.pid))

        return vg_proc


########################################################################
# homebrew memory monitor until valgrind works properly
########################################################################


class sampler:

    def __init__(self, name):
        self.name = name
        self.max = 0
        self.final = 0
        self.sum = 0
        self.samples = 0

    def sample(self):
        s = self.get_sample()
        self.samples += 1
        self.sum += s
        self.max = max(self.max, s)
        self.final = s

    def report(self):
        self.samples = max(self.samples, 1)
        logging.info("__COUNT_%s_MAX!%d" % (self.name.upper(), self.max))
        logging.info("__COUNT_%s_AVG!%d" % (self.name.upper(), (self.sum / self.samples)))
        logging.info("__COUNT_%s_FINAL!%d" % (self.name.upper(), self.final))


class proc_maps_heap_sampler(sampler):

    def __init__(self, procpath):
        sampler.__init__(self, "heap")
        self.procpath = procpath

    def get_sample(self):
        map_entry_rx = re.compile("^([0-9a-f]+)-([0-9a-f]+)\s+[-r][-w][-x][-p]\s+(?:\S+\s+){3}\[heap\]$")
        maps_path = os.path.join(self.procpath, "maps")
        maps_file = open(maps_path)
        for line in maps_file.xreadlines():
            m = map_entry_rx.match(line)
            if m:
                (lo,hi) = m.group(1,2)
                lo = int(lo, 16)
                hi = int(hi, 16)
                sz = hi - lo
                maps_file.close()
                return sz
        maps_file.close()
        return 0


class proc_status_sampler(sampler):
    
    def __init__(self, procpath, entry):
        sampler.__init__(self, entry)
        self.status_entry_rx = re.compile("^Vm%s:\s+(\d+) kB" % entry)
        self.procpath = procpath

    def get_sample(self):
        status_path = os.path.join(self.procpath, "status")
        status_file = open(status_path)
        for line in status_file.xreadlines():
            m = self.status_entry_rx.match(line)
            if m:
                status_file.close()
                return int(m.group(1)) * 1024

        status_file.close()
        return 0


def wait_collecting_memory_stats(process):


    procdir = os.path.join("/proc", str(process.pid))
    samplers = [    proc_status_sampler(procdir, "RSS"),
            proc_status_sampler(procdir, "Size"),
            proc_maps_heap_sampler(procdir) ]

    process.poll()
    while process.returncode == None:

        for s in samplers:
            s.sample()

        time.sleep(1)
        process.poll()


    for s in samplers:
        s.report()


########################################################################
# config variables
########################################################################


disp        = 25
batchprefix = "batch-"
homedir     = "/home/mozvalgrind"
vnc2swfpath = "%s/pyvnc2swf-0.8.2" % homedir
url         = "file://%s/workload.xml" % homedir
probe_point = "nsWindow::SetTitle(*"
num_frames  = 10
vgopts   = [ "--memcheck:leak-check=yes",
             "--memcheck:error-limit=no",
             ("--memcheck:num-callers=%d" % num_frames),
#             "--memcheck:leak-resolution=high",
#             "--memcheck:show-reachable=yes",
             "--massif:format=html",
             ("--massif:depth=%d" % num_frames),
             "--massif:instrs=yes",
             "--callgrind:simulate-cache=yes",
             "--callgrind:simulate-hwpref=yes",
#             ("--callgrind:dump-before=%s" % probe_point),
             "--callgrind:I1=65536,2,64",
             "--callgrind:D1=65536,2,64",
             "--callgrind:L2=524288,8,64",
         "--verbose"
             ]


######################################################
# logging results
######################################################

def archive_dir(dir, sums):
    res = "current"
    tar = res + ".tar.gz"

    if os.path.exists(res):
        shutil.rmtree(res)

    if os.path.exists(tar):
        os.unlink(tar)

    os.mkdir(res)
    ix = open(os.path.join(res, "index.html"), "w")
    ix.write("<html>\n<body>\n")
    ix.write("<h1>run: %s</h1>\n" % dir)

    # summary info
    ix.write("<h2>Summary info</h2>\n")
    ix.write("<table>\n")
    for x in sums:
        ix.write("<tr><td>%s</td><td>%s</td></tr>\n" % (x, sums[x]))
    ix.write("</table>\n")


    # primary logs
    ix.write("<h2>Primary logs</h2>\n")
    for log in glob.glob(os.path.join(dir, "valgrind-*-log*")):
        (dirname, basename) = os.path.split(log)
        shutil.copy(log, os.path.join(res, basename))
        ix.write("<a href=\"%s\">%s</a><br />\n" % (basename, basename))   


    # massif graphs
    ix.write("<h2>Massif results</h2>\n")
    ix.write("<h3>Click graph to see details</h3>\n")
    for mp in glob.glob(os.path.join(dir, "massif.*.ps")):
        (dirname,basename) = os.path.split(mp)
        (base,ext) = os.path.splitext(basename)
        png = base + ".png"
        html = base + ".html"
        subprocess.call(["convert", "-rotate", "270", mp, os.path.join(res, png)])
        shutil.copy(os.path.join(dir, html), os.path.join(res, html))
        ix.write("<a href=\"%s\"><img src=\"%s\" /></a><br />\n" % (html, png))

    # run movies
    ix.write("<h2>Movies</h2>\n")
    for movie in ["memcheck", "massif", "callgrind"]:
        for ext in [".html", ".swf"]:
            base = movie + ext
            if os.path.exists(os.path.join(dir, base)):
                shutil.copy(os.path.join(dir, base), os.path.join(res, base))
        if os.path.exists(os.path.join(res, movie + ".html")):
            ix.write("<a href=\"%s\">%s movie</a><br />\n" % (movie + ".html", movie))

    # callgrind profile
    ix.write("<h2>Callgrind profiles</h2>\n")
    for cg in glob.glob(os.path.join(dir, "callgrind.out.*")):
        (dir, base) = os.path.split(cg)
        shutil.copy(cg, os.path.join(res, base))
        ix.write("<a href=\"%s\">%s</a><br />\n" % (base, base))

    ix.write("</body>\n</html>\n")
    ix.close()

    for i in glob.glob(os.path.join(res, "*")):
        os.chmod(i, 0644)
    os.chmod(res, 0755)

    subprocess.call(["tar", "czvf", tar, res])
    os.chmod(tar, 0644)


def log_result_summaries(tmpdir):

    pats = {
        "IR"      :  re.compile("==\d+==\s+I\s+refs:\s+([0-9,]+)"),
        "ALLOC"      :  re.compile("==\d+==\s+malloc/free:\s+[0-9,]+\s+allocs,\s+[0-9,]+\s+frees,\s+([0-9,]+)\s+bytes allocated."),   
        "LEAKED"  :  re.compile("==\d+==\s+(?:definitely|indirectly)\s+lost:\s+([0-9,]+)\s+bytes"),   
        "DUBIOUS" :  re.compile("==\d+==\s+possibly\s+lost:\s+([0-9,]+)\s+bytes"),
        "LIVE"    :  re.compile("==\d+==\s+still\s+reachable:\s+([0-9,]+)\s+bytes"),   
        }

    sums = {}

    for fname in glob.glob("%s/valgrind-*-log*" % tmpdir):
        f = open(fname)

        for line in f.xreadlines():
            for pat in pats:
                rx = pats[pat]
                match = rx.search(line)
                if match:
                    val = int(match.group(1).replace(",", ""))
                    if pat in sums:
                        val = val + sums[pat]
                    sums[pat] = val
        f.close()

    for pat in sums:
        logging.info("__COUNT_%s!%d" % (pat, sums[pat]))

    archive_dir(tmpdir, sums)



########################################################################
# main
########################################################################

if len(sys.argv) != 2:
    print("usage: %s <firefox-bin build dir>" % sys.argv[0])
    sys.exit(1)

logging.basicConfig(level=logging.INFO,
                    format='%(asctime)s %(levelname)s %(message)s')

logging.info("running as %s" % getpass.getuser())

logging.info("killing any residual processes in this group")
subprocess.call(["killall", "-q", "-9", "memcheck", "callgrind", "massif", "valgrind", "firefox-bin"])
time.sleep(3)

dirs=glob.glob(os.path.join(homedir, batchprefix + "*"))
dirs.sort()
if len(dirs) > 5:
    for olddir in dirs[:-5]:
        logging.info("cleaning up old directory %s" % olddir)
        shutil.rmtree(olddir)
    

timestr = datetime.datetime.now().isoformat().replace(":", "-")
tmpdir = tempfile.mkdtemp(prefix=(batchprefix + timestr + "-"), dir=homedir)

logging.info("started batch run in dir " + tmpdir)

os.chdir(tmpdir)
ffdir = sys.argv[1]

write_vgopts(tmpdir, vgopts)

xvnc_proc = None
runner = None
recorder = None

######################################################
# note: runit is supervising a single Xvnc on disp 25
# there is no need to run one here as well
######################################################
# xvnc_proc = start_xvnc(disp, tmpdir)
######################################################

try:

    runner = Firefox_runner(batchprefix, homedir, ffdir, timestr, tmpdir, disp)

    wait_collecting_memory_stats(runner.run_normal(url))
    runner.clear_cache()

#    for variant in ["memcheck", "massif", "callgrind"]:
#        recorder = start_xvnc_recorder(vnc2swfpath, disp, variant, tmpdir)
#        runner.run_valgrind(variant, url).wait()
#        runner.clear_cache()
#        complete_xvnc_recording(vnc2swfpath, recorder, variant, tmpdir)

    log_result_summaries(tmpdir)
    logging.info("valgrind-firefox processes complete")

finally:
    if recorder and recorder.poll() == None:
        logging.info("killing recorder process %d" % recorder.pid)
    os.kill(recorder.pid, signal.SIGKILL)
    if xvnc_proc and xvnc_proc.poll() == None:
        logging.info("killing Xvnc process %d" % xvnc_proc.pid)
        os.kill(xvnc_proc.pid, signal.SIGKILL)

logging.info("batch run in dir %s complete" % tmpdir)
