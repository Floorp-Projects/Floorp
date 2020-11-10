# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
""" Collects visualmetrics dependencies.
"""
import os
import subprocess
import sys
import time
import contextlib
from distutils.spawn import find_executable

from mozperftest.utils import host_platform


_PILLOW_VERSION = "7.2.0"
_PYSSIM_VERSION = "0.4"


def _start_xvfb():
    old_display = os.environ.get("DISPLAY")
    xvfb = find_executable("Xvfb")
    if xvfb is None:
        raise FileNotFoundError("Xvfb")
    cmd = [xvfb, ":99"]
    proc = subprocess.Popen(
        cmd, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, close_fds=True
    )
    os.environ["DISPLAY"] = ":99"
    time.sleep(0.2)
    return proc, old_display


def _stop_xvfb(proc, old_display):
    proc, old_display
    if old_display is None:
        del os.environ["DISPLAY"]
    else:
        os.environ["DISPLAY"] = old_display
    if proc is not None:
        try:
            proc.terminate()
            proc.wait()
        except OSError:
            pass


@contextlib.contextmanager
def xvfb():
    proc, old_display = _start_xvfb()
    try:
        yield
    finally:
        _stop_xvfb(proc, old_display)


def get_plat():
    return host_platform(), f"{sys.version_info.major}.{sys.version_info.minor}"


NUMPY = {
    ("linux64", "3.8",): (
        "41/6e/919522a6e1d067ddb5959c5716a659a05719e2f27487695d2a539b51d66e/"
        "numpy-1.19.2-cp38-cp38-manylinux1_x86_64.whl"
    ),
    ("linux64", "3.7",): (
        "d6/2e/a2dbcff6f46bb65645d18538d67183a1cf56b006ba96a12575c282a976bc/"
        "numpy-1.19.2-cp37-cp37m-manylinux1_x86_64.whl"
    ),
    ("linux64", "3.6",): (
        "b8/e5/a64ef44a85397ba3c377f6be9c02f3cb3e18023f8c89850dd319e7945521/"
        "numpy-1.19.2-cp36-cp36m-manylinux1_x86_64.whl"
    ),
    ("darwin", "3.8",): (
        "33/1a/d10d1c23d21c289a3e87e751a9daf0907e91665cab08d0c35033fd4f5b55/"
        "numpy-1.19.2-cp38-cp38-macosx_10_9_x86_64.whl"
    ),
    ("darwin", "3.7",): (
        "c1/a9/f04a5b7db30cc30b41fe516b8914c5049264490a34a49d977937606fbb23/"
        "numpy-1.19.2-cp37-cp37m-macosx_10_9_x86_64.whl"
    ),
    ("darwin", "3.6",): (
        "be/8e/800113bd3a0c9195b24574b8922ad92be96278028833c389b69a8b14f657/"
        "numpy-1.19.2-cp36-cp36m-macosx_10_9_x86_64.whl"
    ),
    ("win64", "3.6",): (
        "dc/8e/a78d4e4a28adadbf693a9c056a0d5955a906889fa0dc3768b88deb236e22/"
        "numpy-1.19.2-cp36-cp36m-win_amd64.whl"
    ),
    ("win64", "3.7",): (
        "82/4e/61764556b7ec13f5bd441b04530e2f9f11bb164308ef0e6951919bb846cb/"
        "numpy-1.19.2-cp37-cp37m-win_amd64.whl"
    ),
    ("win64", "3.8"): (
        "69/89/d8fc61a51ded540bd4b8859510b4ae44a0762c8b61dd81eb2c36f5e853ef/"
        "numpy-1.19.2-cp38-cp38-win_amd64.whl"
    ),
}


SCIPY = {
    ("darwin", "3.8",): (
        "8a/84/568ec7727bc789a9b623ec2652043ad3311d7939f152e81cb5d699bfb9b1"
        "/scipy-1.5.2-cp38-cp38-macosx_10_9_x86_64.whl"
    ),
    ("darwin", "3.7",): (
        "bc/47/e71e7f198a0b547fe861520a0240e3171256822dae81fcc97a36b772303e"
        "/scipy-1.5.2-cp37-cp37m-macosx_10_9_x86_64.whl"
    ),
    ("darwin", "3.6",): (
        "00/c0/ddf03baa7ee2a3540d8fbab0fecff7cdd0595dffd91cda746caa95cb686d"
        "/scipy-1.5.2-cp36-cp36m-macosx_10_9_x86_64.whl"
    ),
    ("linux64", "3.8",): (
        "8f/be/8625045311a3ed58af5f7ca8a51f20b1bbf72e288d34b48cc81cca550166"
        "/scipy-1.5.2-cp38-cp38-manylinux1_x86_64.whl"
    ),
    ("linux64", "3.7",): (
        "65/f9/f7a7e5009711579c72da2725174825e5056741bf4001815d097eef1b2e17"
        "/scipy-1.5.2-cp37-cp37m-manylinux1_x86_64.whl"
    ),
    ("linux64", "3.6",): (
        "2b/a8/f4c66eb529bb252d50e83dbf2909c6502e2f857550f22571ed8556f62d95"
        "/scipy-1.5.2-cp36-cp36m-manylinux1_x86_64.whl"
    ),
    ("win64", "3.6",): (
        "fc/f6/3d455f8b376a0faf1081dbba38bbd594c074292bdec08feaac589f53bc06/"
        "numpy-1.19.2-cp36-cp36m-win_amd64.whl"
    ),
    ("win64", "3.7",): (
        "66/80/d8a5050df5b4d8229e018f3222fe603ce7f92c026b78f4e05d69c3a6c43b/"
        "numpy-1.19.2-cp37-cp37m-win_amd64.whl"
    ),
    ("win64", "3.8"): (
        "9e/66/57d6cfa52dacd9a57d0289f8b8a614b2b4f9c401c2ac154d6b31ed8257d6/"
        "numpy-1.19.2-cp38-cp38-win_amd64.whl"
    ),
}


def get_dependencies():
    return (
        "https://files.pythonhosted.org/packages/" + NUMPY[get_plat()],
        "https://files.pythonhosted.org/packages/" + SCIPY[get_plat()],
        "Pillow==%s" % _PILLOW_VERSION,
        "pyssim==%s" % _PYSSIM_VERSION,
        "influxdb==5.3.0",
        "grafana_api==1.0.3",
    )
