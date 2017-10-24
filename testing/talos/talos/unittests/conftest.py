import os

import pytest

here = os.path.realpath(__file__)
__TESTS_DIR = os.path.join(os.path.dirname(os.path.dirname(here)), 'tests')


def remove_develop_files(starting_dir=__TESTS_DIR):
    for file_name in os.listdir(starting_dir):

        file_path = os.path.join(starting_dir, file_name)

        if file_name.endswith('.develop') and os.path.isfile(file_path):
            os.remove(file_path)
        elif os.path.isdir(file_path):
            remove_develop_files(file_path)
