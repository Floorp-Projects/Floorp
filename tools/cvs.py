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

# Version: $Id: cvs.py,v 1.3 2000/01/28 18:26:09 slamm%netscape.com Exp $

# module cvs -- Add multithreading to the cvs client.

"""A multithreading wrapper around the cvs client.

Exports:
    class CVS(module='<cvs_module>')
        CVS.module_files -- Array of module files and directories.
            Files that need to be grouped together are space separated
            within the same array element. For example,
               ['mozilla/xpcom','mozilla/js !mozilla/js/extra']
    class ModuleFileQueue(): A queue of cvs module files and directories.
    xargs() - Emulate xargs, but use threads to draw from common queue.
    class XArgsThread() - Helper class for xargs()
    parallel_cvs_co() - Use xargs and ModuleFileQueue to checkout. Time it too.
  
Three threads should be enough to get a decent pull time. In fact,
any more threads than that will probably not help.

"""

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
         
        members = self._flatten_module(members, self.module)
        members.sort()
        members = self._get_checkout_groups(members)

        return members

    def _flatten_module(self, members, module):
        result = []
        for member in  members[module]:
            if members.has_key(member):
                result.extend(self._flatten_module(members,member))
            else:
                result.append(member)
        return result
   
    def _get_checkout_groups(self, members):
        """Combine ignored files with their parents.

        Remove duplicates.
        Assumes that `members' is sorted in ascending order.
        """
        exclude = []
        checkout_groups = []
        seen = {}
        for member in members:
            if member[0] == "!":
                seen[member[1:]] = 'excluded'
                exclude.append(member)
            else:
                if self._is_covered(seen, member):
                    continue
                seen[member] = 'included'
                group = [member]
                for file in exclude:
                    if file[1:len(member)+1] == member:
                        group.append(file)
                  #exclude.remove(file)
                checkout_groups.append(group)
        return checkout_groups

    def _is_covered(self, seen, member):
        """Check if a directory's parent has been seen.

        If the parent has been seen, do not include this member.
        Otherwise this directory will be pulled twice.
        """
        components = string.split(member,'/')
        subdir = components[0]
        subdirs = [subdir]
        for dir in components[1:]:
            subdir = string.join((subdir, dir), '/')
            subdirs.append(subdir)
        subdirs.reverse()
        for subdir in subdirs:
            if seen.has_key(subdir):
                return seen[subdir] == 'included'
        return 0


class ModuleFileQueue(Queue.Queue):
    """Use a thread-safe Queue for listing module files and directories."""

    def __init__(self, cvs_module):
        mozcvs = CVS(module=cvs_module)
        queue_size = len(mozcvs.module_files)
        Queue.Queue.__init__(self, queue_size)
        for file in mozcvs.module_files:
            self.put(file)


class XArgsThread(threading.Thread):
    """An individual thread for the XArgs class"""

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
                files.extend(next_file_group)
            if self.error_event and self.error_event.isSet():
                sys.exit()
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

def xargs(command, queue, **params):
    """Similar to shell's xargs.

    Requires a shell command and a queue for arguments.
    """
    num_threads = params.get('num_threads')
    max_args    = params.get('max_args')
    verbose     = params.get('verbose')
    if not num_threads: num_threads = 1
    if not max_args:    max_args = 10

    # Use an event to stop all the threads if one fails
    error_event = threading.Event()

    # Create the threads
    threads = []
    for ii in range(0,num_threads):
        thread = XArgsThread(command, queue, max_args=max_args,
                             error_event=error_event, verbose=verbose)
        threads.append(thread)
        thread.start()
    # Wait until the threads finish
    while threading.activeCount() > 1:
        time.sleep(1)
        pass
    for thread in threads:
        if thread.status:
            return thread.status
    return 0


def parallel_cvs_co(co_cmd, modules):
    """Checkout a cvs module using multiple threads."""
    start_time = time.time()      # Track the time to pull

    # Create a Queue of the modules individual files and directories
    module_queue = ModuleFileQueue(modules)

    status = xargs(co_cmd, module_queue, num_threads=3, max_args=10,
                   verbose=1)

    # Compute and print time spent
    total_time = time.time() - start_time
    total_time = time.strftime("%H:%M:%S", time.gmtime(total_time))
    print "Total time to pull: " + total_time

    if status:  # Did xargs return an error?
        sys.exit(status)


if __name__ == "__main__":
    # Check for module on the command-line
    # XXX Need more general cvs argument handling
    if len(sys.argv) < 2:
        print "cvs.py: must specity at least one module or directory"
        sys.exit(1)
    modules = string.join(sys.argv[1:])
    parallel_cvs_co('cvs -z3 -q co -P', modules)
