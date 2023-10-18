#!/usr/bin/env python
import os

import mozunit
from mozproxy.recordings import RecordingFile

here = os.path.dirname(__file__)
os.environ["MOZPROXY_DIR"] = os.path.join(here, "files")


def test_recording_generation(*args):
    test_file = os.path.join(here, "files", "new_file.zip")
    file = RecordingFile(test_file)
    with open(file.recording_path, "w") as recording:
        recording.write("This is a recording")

    file.set_metadata("test_file", True)
    file.generate_zip_file()

    assert os.path.exists(test_file)
    os.remove(test_file)
    os.remove(file.recording_path)
    os.remove(file._metadata_path)


def test_recording_content(*args):
    test_file = os.path.join(here, "files", "recording.zip")
    file = RecordingFile(test_file)

    assert file.metadata("test_file") is True
    assert os.path.exists(file.recording_path)
    os.remove(file.recording_path)
    os.remove(file._metadata_path)


if __name__ == "__main__":
    mozunit.main(runwith="pytest")
