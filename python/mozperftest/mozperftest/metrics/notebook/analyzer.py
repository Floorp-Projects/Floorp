# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
from .constant import Constant
from .logger import logger


class NotebookAnalyzer(object):
    """Analyze the standardized data.

    The methods in these functions will be injected in an Iodide page in the future.
    """

    def __init__(self, data):
        """Initialize the Analyzer.

        :param dict data: Standardized data, post-transformation.
        """
        self.data = data
        self.const = Constant()

    def split_subtests(self):
        """If the subtest field exists, split the data based
        on it, grouping data into subtest groupings.
        """
        if "subtest" not in self.data[0]:
            return {"": self.data}

        split_data = {}
        for entry in self.data:
            subtest = entry["subtest"]
            if subtest not in split_data:
                split_data[subtest] = []
            split_data[subtest].append(entry)

        return split_data

    def get_header(self):
        template_header_path = str(self.const.here / "notebook-sections" / "header")
        with open(template_header_path, "r") as f:
            template_header_content = f.read()
            return template_header_content

    def get_notebook_section(self, func):
        template_function_folder_path = self.const.here / "notebook-sections"
        template_function_file_path = template_function_folder_path / func
        if not template_function_file_path.exists():
            logger.warning(f"Could not find the notebook-section called {func}")
            return ""
        with open(str(template_function_file_path), "r") as f:
            return f.read()
