# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
import json
import os
import shutil
from datetime import datetime
from shutil import copyfile
from zipfile import ZipFile

from .utils import LOG


class RecordingFile:
    def __init__(self, path_to_zip_file):
        self._recording_zip_path = path_to_zip_file

        self._base_name = os.path.splitext(os.path.basename(self._recording_zip_path))[
            0
        ]
        if not os.path.splitext(path_to_zip_file)[1] == ".zip":
            LOG.error(
                "Wrong file type! The provided recording should be a zip file. %s"
                % path_to_zip_file
            )
            raise Exception(
                "Wrong file type! The provided recording should be a zip file."
            )

        # create a temp dir
        self._mozproxy_dir = os.environ["MOZPROXY_DIR"]
        self._tempdir = os.path.join(self._mozproxy_dir, self._base_name)

        if os.path.exists(self._tempdir):
            LOG.info(
                "The recording dir already exists! Resetting the existing dir and data."
            )
            shutil.rmtree(self._tempdir)
        os.mkdir(self._tempdir)

        self._metadata_path = self._get_temp_path("metadata.json")
        self._recording = self._get_temp_path("dump.mp")

        if os.path.exists(path_to_zip_file):
            with ZipFile(path_to_zip_file, "r") as zipObj:
                # Extract all the contents of zip file in different directory
                zipObj.extractall(self._tempdir)

            if not os.path.exists(self._recording):
                self._convert_to_new_format()

            if not os.path.exists(self._metadata_path):
                LOG.error("metadata file is missing!")
                raise Exception("metadata file is missing!")

            with open(self._metadata_path) as json_file:
                self._metadata = json.load(json_file)
            self.validate_recording()
            LOG.info(
                "Loaded recoording generated on %s" % self.metadata("recording_date")
            )
        else:
            LOG.info("Recording file does not exists!!! Generating base structure")
            self._metadata = {"content": [], "recording_date": str(datetime.now())}

    def _convert_to_new_format(self):
        # Convert zip recording to new format

        LOG.info("Convert zip recording to new format")

        for tmp_file in os.listdir(self._tempdir):
            if tmp_file.endswith(".mp"):
                LOG.info("Renaming %s to dump.mp file" % tmp_file)
                os.rename(self._get_temp_path(tmp_file), self._get_temp_path("dump.mp"))
            elif tmp_file.endswith(".json"):
                if tmp_file.startswith("mitm_netlocs_"):
                    LOG.info("Renaming %s to netlocs.json file" % tmp_file)
                    os.rename(
                        self._get_temp_path("%s.json" % os.path.splitext(tmp_file)[0]),
                        self._get_temp_path("netlocs.json"),
                    )
                else:
                    LOG.info("Renaming %s to metadata.json file" % tmp_file)
                    os.rename(
                        self._get_temp_path("%s.json" % os.path.splitext(tmp_file)[0]),
                        self._get_temp_path("metadata.json"),
                    )
            elif tmp_file.endswith(".png"):
                LOG.info("Renaming %s to screenshot.png file" % tmp_file)
                os.rename(
                    self._get_temp_path("%s.png" % os.path.splitext(tmp_file)[0]),
                    self._get_temp_path("screenshot.png"),
                )

    def _get_temp_path(self, file_name):
        return os.path.join(self._tempdir, file_name)

    def validate_recording(self):
        # Validates that minimum zip file content exists
        if not os.path.exists(self._recording):
            LOG.error("Recording file is missing!")
            raise Exception("Recording file is missing!")

        if not os.path.exists(self._metadata_path):
            LOG.error("Metadata file is missing!")
            raise Exception("Metadata file is missing!")

        if "content" in self._metadata:
            # check that all extra files specified in the recording are present
            for content_file in self._metadata["content"]:
                if not os.path.exists(self._get_temp_path(content_file)):
                    LOG.error("File %s does not exist!!" % content_file)
                    raise Exception("Recording file is missing!")
        else:
            LOG.info("Old file type! Not confirming content!")

    def metadata(self, name):
        # Return metadata value
        return self._metadata[name]

    def set_metadata(self, entry, value):
        # Set metadata value
        self._metadata[entry] = value

    @property
    def recording_path(self):
        # Returns the path of the recoring.mp file included in the zip
        return self._recording

    def get_file(self, file_name):
        # Returns the path to a specified file included in the recording zip
        return self._get_temp_path(file_name)

    def add_file(self, path):
        # Adds file to Zip
        if os.path.exists(path):
            copyfile(path, self._tempdir)
            self._metadata["content"].append(os.path.basename(path))
        else:
            LOG.error("Target file %s does not exist!!" % path)
            raise Exception("File does not exist!!!")

    def update_metadata(self):
        # Update metadata with information generated by HttpProtocolExtractor plugin
        # This data is geerated after closing the MitMPtoxy process

        dump_file = self._get_temp_path("dump.json")
        if os.path.exists(dump_file):
            with open(dump_file) as dump:
                dump_info = json.load(dump)
                self._metadata.update(dump_info)

    def generate_zip_file(self):
        self.update_metadata()
        with open(self._get_temp_path(self._metadata_path), "w") as metadata_file:
            json.dump(self._metadata, metadata_file, sort_keys=True, indent=4)

        with ZipFile(self._recording_zip_path, "w") as zf:
            zf.write(self._metadata_path, "metadata.json")
            zf.write(self.recording_path, "dump.mp")
            for file in self._metadata["content"]:
                zf.write(self._get_temp_path(file), file)
        LOG.info("Generated new recording file at: %s" % self._recording_zip_path)
