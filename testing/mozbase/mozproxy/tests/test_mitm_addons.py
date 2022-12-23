#!/usr/bin/env python
import json
import os
from unittest import mock

import mozunit

here = os.path.dirname(__file__)
os.environ["MOZPROXY_DIR"] = os.path.join(here, "files")

protocol = {
    "http_protocol": {"aax-us-iad.amazon.com": "HTTP/1.1"},
    "recorded_requests": 4,
    "recorded_requests_unique": 1,
}


@mock.patch(
    "mozproxy.backends.mitm.scripts.http_protocol_extractor.HttpProtocolExtractor.get_ctx"
)
def test_http_protocol_generate_json_file(ctx_mock):
    ctx_mock.return_value.options.save_stream_file = os.path.join(
        os.environ["MOZPROXY_DIR"], "http_protocol_recording_done.mp"
    )

    from mozproxy.backends.mitm.scripts.http_protocol_extractor import (
        HttpProtocolExtractor,
    )

    test_http_protocol = HttpProtocolExtractor()
    test_http_protocol.ctx = test_http_protocol.get_ctx()

    # test data
    test_http_protocol.request_protocol = protocol["http_protocol"]
    test_http_protocol.hashes = ["Hash string"]
    test_http_protocol.request_count = protocol["recorded_requests"]

    test_http_protocol.done()

    json_path = os.path.join(
        os.environ["MOZPROXY_DIR"], "http_protocol_recording_done.json"
    )
    assert os.path.exists(json_path)
    with open(json_path) as json_file:
        output_data = json.load(json_file)

        assert output_data["recorded_requests"] == protocol["recorded_requests"]
        assert (
            output_data["recorded_requests_unique"]
            == protocol["recorded_requests_unique"]
        )
        assert output_data["http_protocol"] == protocol["http_protocol"]


@mock.patch(
    "mozproxy.backends.mitm.scripts.http_protocol_extractor.HttpProtocolExtractor.get_ctx"
)
def test_http_protocol_response(ctx_mock):
    ctx_mock.return_value.options.save_stream_file = os.path.join(
        os.environ["MOZPROXY_DIR"], "http_protocol_recording_done.mp"
    )

    from mozproxy.backends.mitm.scripts.http_protocol_extractor import (
        HttpProtocolExtractor,
    )

    test_http_protocol = HttpProtocolExtractor()
    test_http_protocol.ctx = test_http_protocol.get_ctx()

    # test data
    flow = mock.MagicMock()
    flow.type = "http"
    flow.request.url = "https://www.google.com/complete/search"
    flow.request.port = 33
    flow.response.data.http_version = b"HTTP/1.1"

    test_http_protocol.request_protocol = {}
    test_http_protocol.hashes = []
    test_http_protocol.request_count = 0

    test_http_protocol.response(flow)

    assert test_http_protocol.request_count == 1
    assert len(test_http_protocol.hashes) == 1
    assert test_http_protocol.request_protocol["www.google.com"] == "HTTP/1.1"


if __name__ == "__main__":
    mozunit.main(runwith="pytest")
