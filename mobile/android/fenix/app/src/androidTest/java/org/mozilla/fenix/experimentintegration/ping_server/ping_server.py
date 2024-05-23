# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import json
import os
import zlib

from flask import Flask, request
from flask.json import jsonify

app = Flask("ping_server")

PINGS = []


@app.route("/pings", methods=["GET", "DELETE"])
def pings():
    if request.method == "GET":
        return jsonify(PINGS)

    if request.method == "DELETE":
        PINGS.clear()
        return ""


@app.route(
    "/submit/<path:telemetry>",
    methods=["POST"],
)
def submit(telemetry):
    if request.method == "POST":
        request_data = request.get_data()

        if request.headers.get("Content-Encoding") == "gzip":
            request_data = zlib.decompress(request_data, zlib.MAX_WBITS | 16)

        ping_data = json.loads(request_data)

        # Store JSON data to self.pings to be used by wait_for_pings()
        PINGS.append(ping_data)
        return ""
    return ""


if __name__ == "__main__":
    port = int(os.environ.get("PORT", 5000))
    app.run(debug=True, host="0.0.0.0", port=port)
