# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import os
import sys
import time


class EnduranceOptionsMixin(object):

    # parse_args
    def endurance_parse_args(self, options, tests, args=None, values=None):
        if options.iterations is not None:
            if options.checkpoint_interval is None or options.checkpoint_interval > options.iterations:
                options.checkpoint_interval = options.iterations

    # verify_usage
    def endurance_verify_usage(self, options, tests):
        if options.iterations is not None and options.iterations < 1:
            raise ValueError('iterations must be a positive integer')
        if options.checkpoint_interval is not None and options.checkpoint_interval < 1:
            raise ValueError('checkpoint interval must be a positive integer')
        if options.checkpoint_interval and not options.iterations:
            raise ValueError('you must specify iterations when using checkpoint intervals')

    def __init__(self, **kwargs):
        # Inheriting object must call this __init__ to set up option handling
        group = self.add_option_group('endurance')
        group.add_option('--iterations',
                         action='store',
                         dest='iterations',
                         type='int',
                         metavar='int',
                         help='iterations for endurance tests')
        group.add_option('--checkpoint',
                         action='store',
                         dest='checkpoint_interval',
                         type='int',
                         metavar='int',
                         help='checkpoint interval for endurance tests')
        self.parse_args_handlers.append(self.endurance_parse_args)
        self.verify_usage_handlers.append(self.endurance_verify_usage)


class EnduranceTestCaseMixin(object):
    def __init__(self, *args, **kwargs):
        self.iterations = kwargs.pop('iterations') or 1
        self.checkpoint_interval = kwargs.pop('checkpoint_interval') or self.iterations
        self.drive_setup_functions = []
        self.pre_test_functions = []
        self.post_test_functions = []
        self.checkpoint_functions = []
        self.process_checkpoint_functions = []
        self.log_name = None
        self.checkpoint_path = None

    def add_drive_setup_function(self, function):
        self.drive_setup_functions.append(function)

    def add_pre_test_function(self, function):
        self.pre_test_functions.append(function)

    def add_post_test_function(self, function):
        self.post_test_functions.append(function)

    def add_checkpoint_function(self, function):
        self.checkpoint_functions.append(function)

    def add_process_checkpoint_function(self, function):
        self.process_checkpoint_functions.append(function)

    def drive(self, test, app=None):
        self.test_method = test
        self.app_under_test = app
        for function in self.drive_setup_functions:
            function(test, app)

        # Now drive the actual test case iterations
        for count in range(1, self.iterations + 1):
            self.iteration = count
            self.marionette.log("%s iteration %d of %d" % (self.test_method.__name__, count, self.iterations))
            # Print to console so can see what iteration we're on while test is running
            if self.iteration == 1:
                print "\n"
            print "Iteration %d of %d..." % (count, self.iterations)
            sys.stdout.flush()

            for function in self.pre_test_functions:
                function()

            self.test_method()

            for function in self.post_test_functions:
                function()

            # Checkpoint time?
            if ((count % self.checkpoint_interval) == 0) or count == self.iterations:
                self.checkpoint()

        # Finished, now process checkpoint data into .json output
        self.process_checkpoint_data()

    def checkpoint(self):
        # Console output so know what's happening if watching console
        print "Checkpoint..."
        sys.stdout.flush()
        self.cur_time = time.strftime("%Y%m%d%H%M%S", time.localtime())
        # If first checkpoint, create the file if it doesn't exist already
        if self.iteration in (0, self.checkpoint_interval):
            self.checkpoint_path = "checkpoints"
            if not os.path.exists(self.checkpoint_path):
                os.makedirs(self.checkpoint_path, 0755)
            self.log_name = "%s/checkpoint_%s_%s.log" % (self.checkpoint_path, self.test_method.__name__, self.cur_time)
            with open(self.log_name, 'a') as log_file:
                log_file.write('%s Endurance Test: %s\n' % (self.cur_time, self.test_method.__name__))
                log_file.write('%s Checkpoint after iteration %d of %d:\n' % (self.cur_time, self.iteration, self.iterations))
        else:
            with open(self.log_name, 'a') as log_file:
                log_file.write('%s Checkpoint after iteration %d of %d:\n' % (self.cur_time, self.iteration, self.iterations))

        for function in self.checkpoint_functions:
            function()

    def process_checkpoint_data(self):
        # Process checkpoint data into .json
        self.marionette.log("processing checkpoint data")
        for function in self.process_checkpoint_functions:
            function()


class MemoryEnduranceTestCaseMixin(object):

    def __init__(self, *args, **kwargs):
        # TODO: add desktop support
        if self.device_manager:
            self.add_checkpoint_function(self.memory_b2g_checkpoint)
            self.add_process_checkpoint_function(self.memory_b2g_process_checkpoint)

    def memory_b2g_checkpoint(self):
        # Sleep to give device idle time (for gc)
        idle_time = 30
        self.marionette.log("sleeping %d seconds to give the device some idle time" % idle_time)
        time.sleep(idle_time)

        # Dump out some memory status info
        self.marionette.log("checkpoint")
        output_str = self.device_manager.shellCheckOutput(["b2g-ps"])
        with open(self.log_name, 'a') as log_file:
            log_file.write('%s\n' % output_str)

    def memory_b2g_process_checkpoint(self):
        # Process checkpoint data into .json
        self.marionette.log("processing checkpoint data from %s" % self.log_name)

        # Open the checkpoint file
        checkpoint_file = open(self.log_name, 'r')

        # Grab every b2g rss reading for each checkpoint
        b2g_rss_list = []
        for next_line in checkpoint_file:
            if next_line.startswith("b2g"):
                b2g_rss_list.append(next_line.split()[5])

        # Close the checkpoint file
        checkpoint_file.close()

        # Calculate the average b2g_rss
        total = 0
        for b2g_mem_value in b2g_rss_list:
            total += int(b2g_mem_value)
        avg_rss = total / len(b2g_rss_list)

        # Create a summary text file
        summary_name = self.log_name.replace('.log', '_summary.log')
        summary_file = open(summary_name, 'w')

        # Write the summarized checkpoint data
        summary_file.write('test_name: %s\n' % self.test_method.__name__)
        summary_file.write('completed: %s\n' % self.cur_time)
        summary_file.write('app_under_test: %s\n' % self.app_under_test.lower())
        summary_file.write('total_iterations: %d\n' % self.iterations)
        summary_file.write('checkpoint_interval: %d\n' % self.checkpoint_interval)
        summary_file.write('b2g_rss: ')
        summary_file.write(', '.join(b2g_rss_list))
        summary_file.write('\navg_rss: %d\n\n' % avg_rss)

        # Close the summary file
        summary_file.close()

        # Write to suite summary file
        suite_summary_file_name = '%s/avg_b2g_rss_suite_summary.log' % self.checkpoint_path
        suite_summary_file = open(suite_summary_file_name, 'a')
        suite_summary_file.write('%s: %s\n' % (self.test_method.__name__, avg_rss))
        suite_summary_file.close()

