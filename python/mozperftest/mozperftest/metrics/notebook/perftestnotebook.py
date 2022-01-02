# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
import json
import webbrowser
from http.server import BaseHTTPRequestHandler, HTTPServer

from .constant import Constant


class PerftestNotebook(object):
    """Controller class for PerftestNotebook."""

    def __init__(self, data, logger, prefix):
        """Initialize the PerftestNotebook.

        :param dict data: Standardized data, post-transformation.
        """
        self.data = data
        self.logger = logger
        self.prefix = prefix
        self.const = Constant()

    def get_notebook_section(self, func):
        """Fetch notebook content based on analysis name.

        :param str func: analysis or notebook section name
        """
        template_path = self.const.here / "notebook-sections" / func
        if not template_path.exists():
            self.logger.warning(
                f"Could not find the notebook-section called {func}", self.prefix
            )
            return ""
        with template_path.open() as f:
            return f.read()

    def post_to_iodide(self, analysis=None, start_local_server=True):
        """Build notebook and post it to iodide.

        :param list analysis: notebook section names, analysis to perform in iodide
        """
        data = self.data
        notebook_sections = ""

        template_header_path = self.const.here / "notebook-sections" / "header"
        with template_header_path.open() as f:
            notebook_sections += f.read()

        if analysis:
            for func in analysis:
                notebook_sections += self.get_notebook_section(func)

        template_upload_file_path = self.const.here / "template_upload_file.html"
        with template_upload_file_path.open() as f:
            html = f.read().replace("replace_me", repr(notebook_sections))

        upload_file_path = self.const.here / "upload_file.html"
        with upload_file_path.open("w") as f:
            f.write(html)

        # set up local server. Iodide will fetch data from localhost:5000/data
        class DataRequestHandler(BaseHTTPRequestHandler):
            def do_GET(self):
                if self.path == "/data":
                    self.send_response(200)
                    self.send_header("Content-type", "application/json")
                    self.send_header("Access-Control-Allow-Origin", "*")
                    self.end_headers()
                    self.wfile.write(bytes(json.dumps(data).encode("utf-8")))

        PORT_NUMBER = 5000
        server = HTTPServer(("", PORT_NUMBER), DataRequestHandler)
        if start_local_server:
            webbrowser.open_new_tab(str(upload_file_path))
            try:
                server.serve_forever()
            finally:
                server.server_close()
