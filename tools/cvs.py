#!/usr/bin/python
# 
# The contents of this file are subject to the Mozilla Public
# License Version 1.1 (the "License"); you may not use this file
# except in compliance with the License. You may obtain a copy of
# the License at http://www.mozilla.org/MPL/
# 
# Software distributed under the License is distributed on an "AS
# IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
# implied. See the License for the specific language governing
# rights and limitations under the License.
# 
# The Original Code is a Mozilla build tool.
# 
# The Initial Developer of the Original Code is Netscape Communications
# Corporation.  Portions created by Netscape are
# Copyright (C) 2000 Netscape Communications Corportation.  All
# Rights Reserved.
# 
# Contributor(s): Stephen Lamm <slamm@netscape.com>
# 

# Version: $Id: cvs.py,v 1.1 2000/01/28 01:20:49 slamm%netscape.com Exp $

#
# cvs.py - Pull cvs tree with parallel threads.
#
#   In basic tests, no more than three threads for pulls.
#
#   I would like to make this behave more like cvs itself. Right now,
#   most of the cvs parameters are hard-coded.

import os
import string
import sys
import threading
import time
import Queue


class CVS:
    """Handle basic cvs operations like reading the modules file.

    Example usage:
        cvs_cmd = cvs(module='SeaMonkeyAll')
        modules = cvs_cmd.module_files

    (Doesn't do much else yet.)

    """
    def __init__(self, **params):
        self.cvsroot = os.environ["CVSROOT"]
        self.params = params
        
    # The magic function that makes cvs.module_files a function call
    def __getattr__(self, attr):
        if attr == "module_files":
            # Parse the module and safe the result for later reference
            self.module_files = self.parse_module_file()
            return self.module_files
        elif self.params.has_key(attr):
            return self.params[attr]
      
    def parse_module_file(self):
        import re
        mod_pat = re.compile(r'\s+-a\s+')
        spc_pat = re.compile(r'\s+')
        module  = ''
        members = {} 

        for line in os.popen("cvs co -c").readlines():
            line = line[:-1]
            module_split = re.split(mod_pat, line)
            if len(module_split) > 1:
                (module, line) = module_split
                members[module] = re.split(spc_pat, line)
            else:
                space_split = re.split(spc_pat, line)[1:]
                members[module].extend(space_split)
         
        members = self.flatten_module(members, self.module)
        members.sort()
        members = self.get_checkout_groups(members)

        return members

    def flatten_module(self, members, module):
        result = []
        for member in  members[module]:
            if members.has_key(member):
                result.extend(self.flatten_module(members,member))
            else:
                result.append(member)
        return result
   
    def get_checkout_groups(self, members):
        import string
        ignore = []
        checkout_groups = []
        for member in members:
            if member[0] == "!":
                ignore.append(member)
            else:
                group = [member]
                for file in ignore:
                    if file[1:len(member)+1] == member:
                        group.append(file)
                  #ignore.remove(file)
                group = string.join(group)
                checkout_groups.append(group)
        return checkout_groups


class ModuleFileQueue(Queue.Queue):
    """Use a thread-safe Queue for listing module files and directories."""

    def __init__(self, cvs_module):
        mozcvs = CVS(module=cvs_module)
        queue_size = len(mozcvs.module_files)
        Queue.Queue.__init__(self, queue_size)
        for file in mozcvs.module_files:
            self.put(file)


class XArgs(threading.Thread):
    """Similar to shell's xargs.

    Requires a shell command and a queue for arguments.

    """
    def __init__(self, shell_command, queue, **params):
        self.shell_command = shell_command
        self.queue         = queue
        self.max_args      = params.get('max_args')
        self.error_event   = params.get('error_event')
        self.verbose       = params.get('verbose')
        self.status = 0
        threading.Thread.__init__(self)
        
    def run(self):
        while not self.queue.empty():
            files = []
            while (self.max_args and len(files) < self.max_args):
                try:
                    next_file_group = self.queue.get_nowait()
                except Queue.Empty:
                    break
                next_files = string.split(next_file_group)
                files.extend(next_files)
            if self.error_event and self.error_event.isSet():
                thread.exit()
            if files:
                if self.verbose:
                    print "%s: %s %s" % (self.getName(), self.shell_command,
                                         string.join(files))
                def add_quotes(xx): return '"' + xx + '"'
                quoted_files = map(add_quotes, files)
                quoted_files = string.join(files)
                error = os.system(self.shell_command + " " + quoted_files)

                if error:
                    # Stop this thread
                    self.status = error
                    if self.error_event: self.error_event.set()
                    sys.exit()           


def parallel_cvs_co(co_cmd, modules):
    """Checkout a cvs module using multiple threads."""
    # Keep track of how long the pull takes.
    start_time = time.time()

    # Create a Queue of the modules individual files and directories
    module_queue = ModuleFileQueue(modules)

    # Create an event to stop all the threads when one fails
    cvs_error = threading.Event()

    # Create the threads
    threads = []
    for ii in range(0,3):
        thread = XArgs(co_cmd, module_queue,
                       max_args=5, error_event=cvs_error, verbose=1)
        threads.append(thread)
        thread.start()

    # Wait until the threads finish
    while threading.activeCount() > 1:
        time.sleep(1)
        pass

    # Compute and print time spent
    end_time = time.time()
    total_time = end_time - start_time
    total_time = time.strftime("%H:%M:%S", time.gmtime(total_time))
    print "Total time to pull: " + total_time

    # Check for an error on exit
    if cvs_error.isSet():
        for thread in threads:
            if thread.status:
                sys.exit(thread.status)


if __name__ == "__main__":
    # Check for module on the command-line
    # XXX Need more general cvs argument handling
    if len(sys.argv) < 2:
        print "cvs.py: must specity at least one module or directory"
        sys.exit(1)
    modules = string.join(sys.argv[1:])
    parallel_cvs_co('cvs -z3 -q co -P', modules)
