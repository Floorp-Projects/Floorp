from __future__ import absolute_import
from io import StringIO
from lxml import etree
from .utils import read_file


def get_dtds(sources, base_path):
    entries = {}
    for source in sources:
        dtd = get_dtd(source, base_path)
        for entry in dtd:
            entries[entry] = {
                "value": dtd[entry],
                "file": source
            }
    return entries


def get_dtd(dtd_source, base_path):
    entries = {}

    source = read_file(dtd_source, base_path)

    dtd = etree.DTD(StringIO(source.decode("utf-8")))
    for entity in dtd.entities():
        entries[entity.name] = entity.content
    return entries
