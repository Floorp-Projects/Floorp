# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
import importlib.util
import inspect
import json
import pathlib

from jsonschema import validate

from mozperftest.metrics.exceptions import (
    NotebookDuplicateTransformsError,
    NotebookInvalidPathError,
    NotebookInvalidTransformError,
)
from mozperftest.runner import HERE
from mozperftest.utils import load_class


class Transformer(object):
    """Abstract class for data transformers."""

    def __init__(self, files=None, custom_transformer=None, logger=None, prefix=None):
        """Initialize the transformer with files.

        :param list files: A list of files containing data to transform.
        :param object custom_transformer: A custom transformer instance.
            Must implement `transform` and `merge` methods.
        """
        self._files = files
        self.logger = logger
        self.prefix = prefix

        if custom_transformer:
            valid = (
                hasattr(custom_transformer, "transform")
                and hasattr(custom_transformer, "merge")
                and callable(custom_transformer.transform)
                and callable(custom_transformer.merge)
            )

            if not valid:
                raise NotebookInvalidTransformError(
                    "The custom transformer must contain `transform` and `merge` methods."
                )

        self._custom_transformer = custom_transformer

        with pathlib.Path(HERE, "schemas", "transformer_schema.json").open() as f:
            self.schema = json.load(f)

    @property
    def files(self):
        return self._files

    @files.setter
    def files(self, val):
        if not isinstance(val, list):
            self.logger.warning(
                "`files` must be a list, got %s" % type(val), self.prefix
            )
            return
        self._files = val

    @property
    def custom_transformer(self):
        return self._custom_transformer

    def open_data(self, file):
        """Opens a file of data.

        If it's not a JSON file, then the data
        will be opened as a text file.

        :param str file: Path to the data file.
        :return: Data contained in the file.
        """
        with open(file) as f:
            if file.endswith(".json"):
                return json.load(f)
            return f.readlines()

    def process(self, name, **kwargs):
        """Process all the known data into a merged, and standardized data format.

        :param str name: Name of the merged data.
        :return dict: Merged data.
        """
        trfmdata = []

        for file in self.files:
            data = {}

            # Open data
            try:
                if hasattr(self._custom_transformer, "open_data"):
                    data = self._custom_transformer.open_data(file)
                else:
                    data = self.open_data(file)
            except Exception as e:
                self.logger.warning(
                    "Failed to open file %s, skipping" % file, self.prefix
                )
                self.logger.warning("%s %s" % (e.__class__.__name__, e), self.prefix)

            # Transform data
            try:
                data = self._custom_transformer.transform(data, **kwargs)
                if not isinstance(data, list):
                    data = [data]
                for entry in data:
                    for ele in entry["data"]:
                        if "file" not in ele:
                            ele.update({"file": file})
                trfmdata.extend(data)
            except Exception as e:
                self.logger.warning(
                    "Failed to transform file %s, skipping" % file, self.prefix
                )
                self.logger.warning("%s %s" % (e.__class__.__name__, e), self.prefix)

        merged = self._custom_transformer.merge(trfmdata)

        if isinstance(merged, dict):
            merged["name"] = name
        else:
            for e in merged:
                e["name"] = name

        validate(instance=merged, schema=self.schema)
        return merged


class SimplePerfherderTransformer:
    """Transforms perfherder data into the standardized data format."""

    entry_number = 0

    def transform(self, data):
        self.entry_number += 1
        return {
            "data": [{"value": data["suites"][0]["value"], "xaxis": self.entry_number}]
        }

    def merge(self, sde):
        merged = {"data": []}
        for entry in sde:
            if isinstance(entry["data"], list):
                merged["data"].extend(entry["data"])
            else:
                merged["data"].append(entry["data"])

        self.entry_number = 0
        return merged


def get_transformer(path, ret_members=False):
    """This function returns a Transformer class with the given path.

    :param str path: The path points to the custom transformer.
    :param bool ret_members: If true then return inspect.getmembers().
    :return Transformer if not ret_members else inspect.getmembers().
    """
    file = pathlib.Path(path)

    if file.suffix != ".py":
        return load_class(path)

    if not file.exists():
        raise NotebookInvalidPathError(f"The path {path} does not exist.")

    # Importing a source file directly
    spec = importlib.util.spec_from_file_location(name=file.name, location=path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)

    members = inspect.getmembers(
        module,
        lambda c: inspect.isclass(c)
        and hasattr(c, "transform")
        and hasattr(c, "merge")
        and callable(c.transform)
        and callable(c.merge),
    )

    if not members and not ret_members:
        raise NotebookInvalidTransformError(
            f"The path {path} was found but it was not a valid transformer."
        )

    return members if ret_members else members[0][-1]


def get_transformers(dirpath=None):
    """This function returns a dict of transformers under the given path.

    If more than one transformers have the same class name, an exception will be raised.

    :param pathlib.Path dirpath: Path to a directory containing the transformers.
    :return dict: {"Transformer class name": Transformer class}.
    """

    ret = {}

    if not dirpath.exists():
        raise NotebookInvalidPathError(f"The path {dirpath.as_posix()} does not exist.")

    if not dirpath.is_dir():
        raise NotebookInvalidPathError(
            f"Path given is not a directory: {dirpath.as_posix()}"
        )

    tfm_files = list(dirpath.glob("*.py"))
    importlib.machinery.SOURCE_SUFFIXES.append("")

    for file in tfm_files:
        members = get_transformer(file.resolve().as_posix(), True)

        for name, tfm_class in members:
            if name in ret:
                raise NotebookDuplicateTransformsError(
                    f"Duplicated transformer {name} "
                    + f"is found in the directory {dirpath.as_posix()}."
                    + "Please define each transformer class with a unique class name.",
                )
            ret.update({name: tfm_class})

    return ret
