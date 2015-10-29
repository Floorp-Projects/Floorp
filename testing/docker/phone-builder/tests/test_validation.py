#!/usr/bin/env python

import unittest
import sys
import yaml
sys.path.append('../bin')
from validate_task import check_task

def load_task(task_file):
    content = open(task_file, 'r')
    return yaml.load(content)['task']

class TaskValidationTest(unittest.TestCase):
    def test_valid_task(self):
        task = load_task('valid.yml')
        self.assertEquals(check_task(task), 0)

    def test_invalid_base_repo(self):
        task = load_task('invalid_base_repo.yml')
        self.assertEquals(check_task(task), -1)

    def test_invalid_head_repo(self):
        task = load_task('invalid_head_repo.yml')
        self.assertEquals(check_task(task), -1)

    def test_public_artifact(self):
        task = load_task('public.yml')
        self.assertEquals(check_task(task), -1)

if __name__ == '__main__':
    unittest.main()
