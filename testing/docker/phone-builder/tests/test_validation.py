#!/usr/bin/env python

import unittest
import sys
import yaml
sys.path.append('../bin')
from bootstrap import check_task
import glob

def load_task(task_file):
    content = open(task_file, 'r')
    task = yaml.load(content)['task']
    sys.argv[1:] = task['payload']['command']
    return task

class TaskValidationTest(unittest.TestCase):
    def __init__(self, methodName='runTest'):
        super(TaskValidationTest, self).__init__(methodName)
        sys.argv.append('')

    def test_valid_tasks(self):
        valid_tasks = glob.glob('valid*.yml')
        for t in valid_tasks:
            task = load_task(t)
            self.assertEqual(check_task(task), 0)

    def test_invalid_tasks(self):
        invalid_tasks = glob.glob('invalid*.yml')
        for t in invalid_tasks:
            task = load_task(t)
            self.assertNotEquals(check_task(task), 0)

if __name__ == '__main__':
    unittest.main()
