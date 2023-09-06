#!/usr/bin/env python
from pathlib import Path
from unittest import mock

import mozunit
import pytest

from mozperftest.metrics.notebook.constant import Constant


def test_init(ptnb, standarized_data):
    assert isinstance(ptnb.data, dict)
    assert isinstance(ptnb.const, Constant)


def test_get_notebook_section(ptnb):
    func = "scatterplot"
    with (ptnb.const.here / "notebook-sections" / func).open() as f:
        assert ptnb.get_notebook_section(func) == f.read()


def test_get_notebook_section_unknown_analysis(ptnb):
    func = "unknown"
    assert ptnb.get_notebook_section(func) == ""


@pytest.mark.parametrize("analysis", [["scatterplot"], None])
def test_post_to_iodide(ptnb, standarized_data, analysis):
    opener = mock.mock_open()

    def mocked_open(self, *args, **kwargs):
        return opener(self, *args, **kwargs)

    with mock.patch.object(Path, "open", mocked_open), mock.patch(
        "mozperftest.metrics.notebook.perftestnotebook.webbrowser.open_new_tab"
    ) as browser, mock.patch(
        "mozperftest.metrics.notebook.perftestnotebook.HTTPServer"
    ) as server:
        ptnb.post_to_iodide(analysis=analysis)

        list_of_calls = opener.mock_calls

        header_path = ptnb.const.here / "notebook-sections" / "header"
        assert mock.call(header_path) in list_of_calls
        index1 = list_of_calls.index(mock.call(header_path))
        assert list_of_calls[index1 + 2] == mock.call().read()

        template_upload_file_path = ptnb.const.here / "template_upload_file.html"
        assert mock.call(template_upload_file_path) in list_of_calls
        index2 = list_of_calls.index(mock.call(template_upload_file_path))
        assert list_of_calls[index2 + 2] == mock.call().read()

        upload_file_path = ptnb.const.here / "upload_file.html"
        assert mock.call(upload_file_path, "w") in list_of_calls
        index3 = list_of_calls.index(mock.call(upload_file_path, "w"))
        assert list_of_calls[index3 + 2] == mock.call().write("")

        assert index1 < index2 < index3

        if analysis:
            section_path = ptnb.const.here / "notebook-sections" / analysis[0]
            assert mock.call(section_path) in list_of_calls
            index4 = list_of_calls.index(mock.call(section_path))
            assert index1 < index4 < index2
        else:
            assert list_of_calls.count(mock.call().__enter__()) == 3

        browser.assert_called_with(str(upload_file_path))
        server.assert_has_calls(
            [mock.call().serve_forever(), mock.call().server_close()]
        )


if __name__ == "__main__":
    mozunit.main()
