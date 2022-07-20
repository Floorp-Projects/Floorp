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
    ("linux64", "3.10",): (
        "88/cc/92815174c345015a326e3fff8beddcb951b3ef0f7c8296fcc22c622add7c"
        "/numpy-1.23.1-cp310-cp310-manylinux_2_17_x86_64.manylinux2014_x86_64.whl"
    ),
    ("linux64", "3.9",): (
        "8d/d6/cc2330e512936a904a4db1629b71d697fb309115f6d2ede94d183cdfe185"
        "/numpy-1.23.1-cp39-cp39-manylinux_2_17_x86_64.manylinux2014_x86_64.whl"
    ),
    ("linux64", "3.8",): (
        "86/c9/9f9d6812fa8a031a568c2c1c49f207a0a4030ead438644c887410fc49c8a"
        "/numpy-1.23.1-cp38-cp38-manylinux_2_17_x86_64.manylinux2014_x86_64.whl"
    ),
    ("linux64", "3.7",): (
        "d6/2e/a2dbcff6f46bb65645d18538d67183a1cf56b006ba96a12575c282a976bc/"
        "numpy-1.19.2-cp37-cp37m-manylinux1_x86_64.whl"
    ),
    ("linux64", "3.6",): (
        "b8/e5/a64ef44a85397ba3c377f6be9c02f3cb3e18023f8c89850dd319e7945521/"
        "numpy-1.19.2-cp36-cp36m-manylinux1_x86_64.whl"
    ),
    ("darwin", "3.10",): (
        "c0/c2/8d58f3ccd1aa3b1eaa5c333a6748e225b45cf8748b13f052cbb3c811c996"
        "/numpy-1.23.1-cp310-cp310-macosx_10_9_x86_64.whl"
    ),
    ("darwin", "3.9",): (
        "e5/43/b1b80cbcea9f2d0e6adadd27a8da2c71b751d5670a846b444087fab408a1"
        "/numpy-1.23.1-cp39-cp39-macosx_10_9_x86_64.whl"
    ),
    ("darwin", "3.8",): (
        "71/08/bc1e4fb7392aa0721f299c444e8c99fa97c8cb41fe33791eca8e26364639"
        "/numpy-1.23.1-cp38-cp38-macosx_10_9_x86_64.whl"
    ),
    ("darwin", "3.7",): (
        "c1/a9/f04a5b7db30cc30b41fe516b8914c5049264490a34a49d977937606fbb23/"
        "numpy-1.19.2-cp37-cp37m-macosx_10_9_x86_64.whl"
    ),
    ("darwin", "3.6",): (
        "be/8e/800113bd3a0c9195b24574b8922ad92be96278028833c389b69a8b14f657/"
        "numpy-1.19.2-cp36-cp36m-macosx_10_9_x86_64.whl"
    ),
    ("win64", "3.10",): (
        "8b/11/75a93826457f94a4c857a38ea3f178915f27ff38ffee1753e36994be7810"
        "/numpy-1.23.1-cp310-cp310-win_amd64.whl"
    ),
    ("win64", "3.9",): (
        "bd/dd/0610fb49c433fe5987ae312fe672119080fd77be484b5698d6fa7554148b"
        "/numpy-1.23.1-cp39-cp39-win_amd64.whl"
    ),
    ("win64", "3.8",): (
        "d0/19/6e81ed6fe30271ebcf25e5e2a0bdf1fa06ddee03a8cb82625503826970db"
        "/numpy-1.23.1-cp38-cp38-win_amd64.whl"
    ),
    ("win64", "3.7",): (
        "82/4e/61764556b7ec13f5bd441b04530e2f9f11bb164308ef0e6951919bb846cb/"
        "numpy-1.19.2-cp37-cp37m-win_amd64.whl"
    ),
    ("win64", "3.6",): (
        "dc/8e/a78d4e4a28adadbf693a9c056a0d5955a906889fa0dc3768b88deb236e22/"
        "numpy-1.19.2-cp36-cp36m-win_amd64.whl"
    ),
}


SCIPY = {
    ("linux64", "3.10",): (
        "bc/fe/72b611ba221c3367b06163992af4807515d6e0e09b3b9beee8ec22162d6f"
        "/scipy-1.8.1-cp310-cp310-manylinux_2_17_x86_64.manylinux2014_x86_64.whl"
    ),
    ("linux64", "3.9",): (
        "25/82/da07cc3bb40554f1f82d7e24bfa7ffbfb05b50c16eb8d738ebb74b68af8f"
        "/scipy-1.8.1-cp39-cp39-manylinux_2_17_x86_64.manylinux2014_x86_64.whl"
    ),
    ("linux64", "3.8",): (
        "cf/28/5ac0afe5fb473a934ef6bc7953a98a3d2eacf9a8f456524f035f3a844ca4"
        "/scipy-1.8.1-cp38-cp38-manylinux_2_17_x86_64.manylinux2014_x86_64.whl"
    ),
    ("linux64", "3.7",): (
        "65/f9/f7a7e5009711579c72da2725174825e5056741bf4001815d097eef1b2e17"
        "/scipy-1.5.2-cp37-cp37m-manylinux1_x86_64.whl"
    ),
    ("linux64", "3.6",): (
        "2b/a8/f4c66eb529bb252d50e83dbf2909c6502e2f857550f22571ed8556f62d95"
        "/scipy-1.5.2-cp36-cp36m-manylinux1_x86_64.whl"
    ),
    ("darwin", "3.10",): (
        "7c/f3/47b882f8b7a4dbc38e8bc5d7befe3ad2da582ae2229745e1eac77217f3e4"
        "/scipy-1.8.1-cp310-cp310-macosx_10_9_x86_64.whl"
    ),
    ("darwin", "3.9",): (
        "b0/de/e8d273063e1b21ec82e4a09a9654c4dcbc3215abbd59b7038c4ff4272e9e"
        "/scipy-1.8.1-cp39-cp39-macosx_10_9_x86_64.whl"
    ),
    ("darwin", "3.8",): (
        "dd/cc/bb5a9705dd30e7f558358168c793084f80de7cca88b06c82dca9d765b225"
        "/scipy-1.8.1-cp38-cp38-macosx_10_9_x86_64.whl"
    ),
    ("darwin", "3.7",): (
        "bc/47/e71e7f198a0b547fe861520a0240e3171256822dae81fcc97a36b772303e"
        "/scipy-1.5.2-cp37-cp37m-macosx_10_9_x86_64.whl"
    ),
    ("darwin", "3.6",): (
        "00/c0/ddf03baa7ee2a3540d8fbab0fecff7cdd0595dffd91cda746caa95cb686d"
        "/scipy-1.5.2-cp36-cp36m-macosx_10_9_x86_64.whl"
    ),
    ("win64", "3.10"): (
        "31/c2/0b8758ebaeb43e089eb56168390824a830f9f419ae07d755d99a46e5a937"
        "/scipy-1.8.1-cp310-cp310-win_amd64.whl"
    ),
    ("win64", "3.9"): (
        "ba/a1/a8fa291b8ae6523866dd099af377bc508c280c8ca43a42483c76775ce3cd"
        "/scipy-1.8.1-cp39-cp39-win_amd64.whl"
    ),
    ("win64", "3.8"): (
        "8d/3e/e6f6fa6458e03ecd456ae6178529d4bd610a7c4999189f34d0668e4e69a6"
        "/scipy-1.8.1-cp38-cp38-win_amd64.whl"
    ),
    ("win64", "3.7",): (
        "66/80/d8a5050df5b4d8229e018f3222fe603ce7f92c026b78f4e05d69c3a6c43b"
        "/scipy-1.5.2-cp37-cp37m-win_amd64.whl"
    ),
    ("win64", "3.6",): (
        "fc/f6/3d455f8b376a0faf1081dbba38bbd594c074292bdec08feaac589f53bc06"
        "/scipy-1.5.2-cp36-cp36m-win_amd64.whl"
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
