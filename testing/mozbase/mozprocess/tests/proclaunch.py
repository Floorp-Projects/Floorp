#!/usr/bin/env python

import argparse
import collections
import ConfigParser
import multiprocessing
import time

ProcessNode = collections.namedtuple('ProcessNode', ['maxtime', 'children'])


class ProcessLauncher(object):

    """ Create and Launch process trees specified by a '.ini' file

        Typical .ini file accepted by this class :

        [main]
        children=c1, 1*c2, 4*c3
        maxtime=10

        [c1]
        children= 2*c2, c3
        maxtime=20

        [c2]
        children=3*c3
        maxtime=5

        [c3]
        maxtime=3

        This generates a process tree of the form:
            [main]
                |---[c1]
                |     |---[c2]
                |     |     |---[c3]
                |     |     |---[c3]
                |     |     |---[c3]
                |     |
                |     |---[c2]
                |     |     |---[c3]
                |     |     |---[c3]
                |     |     |---[c3]
                |     |
                |     |---[c3]
                |
                |---[c2]
                |     |---[c3]
                |     |---[c3]
                |     |---[c3]
                |
                |---[c3]
                |---[c3]
                |---[c3]

        Caveat: The section names cannot contain a '*'(asterisk) or a ','(comma)
        character as these are used as delimiters for parsing.
    """

    # Unit time for processes in seconds
    UNIT_TIME = 1

    def __init__(self, manifest, verbose=False):
        """
        Parses the manifest and stores the information about the process tree
        in a format usable by the class.

        Raises IOError if :
            - The path does not exist
            - The file cannot be read
        Raises ConfigParser.*Error if:
            - Files does not contain section headers
            - File cannot be parsed because of incorrect specification

        :param manifest: Path to the manifest file that contains the
        configuration for the process tree to be launched
        :verbose: Print the process start and end information.
        Genrates a lot of output. Disabled by default.
        """

        self.verbose = verbose

        # Children is a dictionary used to store information from the,
        # Configuration file in a more usable format.
        # Key : string contain the name of child process
        # Value : A Named tuple of the form (max_time, (list of child processes of Key))
        #   Where each child process is a list of type: [count to run, name of child]
        self.children = {}

        cfgparser = ConfigParser.ConfigParser()

        if not cfgparser.read(manifest):
            raise IOError('The manifest %s could not be found/opened', manifest)

        sections = cfgparser.sections()
        for section in sections:
            # Maxtime is a mandatory option
            # ConfigParser.NoOptionError is raised if maxtime does not exist
            if '*' in section or ',' in section:
                raise ConfigParser.ParsingError(
                    "%s is not a valid section name. "
                    "Section names cannot contain a '*' or ','." % section)
            m_time = cfgparser.get(section, 'maxtime')
            try:
                m_time = int(m_time)
            except ValueError:
                raise ValueError('Expected maxtime to be an integer, specified %s' % m_time)

            # No children option implies there are no further children
            # Leaving the children option blank is an error.
            try:
                c = cfgparser.get(section, 'children')
                if not c:
                    # If children is an empty field, assume no children
                    children = None

                else:
                    # Tokenize chilren field, ignore empty strings
                    children = [[y.strip() for y in x.strip().split('*', 1)]
                                for x in c.split(',') if x]
                    try:
                        for i, child in enumerate(children):
                            # No multiplicate factor infront of a process implies 1
                            if len(child) == 1:
                                children[i] = [1, child[0]]
                            else:
                                children[i][0] = int(child[0])

                            if children[i][1] not in sections:
                                raise ConfigParser.ParsingError(
                                    'No section corresponding to child %s' % child[1])
                    except ValueError:
                        raise ValueError(
                            'Expected process count to be an integer, specified %s' % child[0])

            except ConfigParser.NoOptionError:
                children = None
            pn = ProcessNode(maxtime=m_time,
                             children=children)
            self.children[section] = pn

    def run(self):
        """
        This function launches the process tree.
        """
        self._run('main', 0)

    def _run(self, proc_name, level):
        """
        Runs the process specified by the section-name `proc_name` in the manifest file.
        Then makes calls to launch the child processes of `proc_name`

        :param proc_name: File name of the manifest as a string.
        :param level: Depth of the current process in the tree.
        """
        if proc_name not in self.children.keys():
            raise IOError("%s is not a valid process" % proc_name)

        maxtime = self.children[proc_name].maxtime
        if self.verbose:
            print "%sLaunching %s for %d*%d seconds" % (" " * level,
                                                        proc_name,
                                                        maxtime,
                                                        self.UNIT_TIME)

        while self.children[proc_name].children:
            child = self.children[proc_name].children.pop()

            count, child_proc = child
            for i in range(count):
                p = multiprocessing.Process(target=self._run, args=(child[1], level + 1))
                p.start()

        self._launch(maxtime)
        if self.verbose:
            print "%sFinished %s" % (" " * level, proc_name)

    def _launch(self, running_time):
        """
        Create and launch a process and idles for the time specified by
        `running_time`

        :param running_time: Running time of the process in seconds.
        """
        elapsed_time = 0

        while elapsed_time < running_time:
            time.sleep(self.UNIT_TIME)
            elapsed_time += self.UNIT_TIME


if __name__ == '__main__':

    parser = argparse.ArgumentParser()
    parser.add_argument("manifest", help="Specify the configuration .ini file")
    args = parser.parse_args()

    proclaunch = ProcessLauncher(args.manifest)
    proclaunch.run()
