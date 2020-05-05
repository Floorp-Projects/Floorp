#!/usr/bin/env python3
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
import cv2
import json
import numpy as np
import os
import pathlib
import shutil
import socket
import tarfile
import tempfile
import urllib

from functools import wraps
from matplotlib import pyplot as plt
from scipy.stats import spearmanr


def open_data(file):
    return cv2.VideoCapture(str(file))


def socket_timeout(value=120):
    """Decorator for socket timeouts."""
    def _socket_timeout(func):
        @wraps(func)
        def __socket_timeout(*args, **kw):
            old = socket.getdefaulttimeout()
            socket.setdefaulttimeout(value)
            try:
                return func(*args, **kw)
            finally:
                socket.setdefaulttimeout(old)
        return __socket_timeout
    return _socket_timeout


@socket_timeout(120)
def query_activedata(query_json, log):
    """Used to run queries on active data."""
    active_data_url = "http://activedata.allizom.org/query"

    req = urllib.request.Request(active_data_url)
    req.add_header("Content-Type", "application/json")
    jsondata = json.dumps(query_json)

    jsondataasbytes = jsondata.encode("utf-8")
    req.add_header("Content-Length", len(jsondataasbytes))

    log.info("Querying Active-data...")
    response = urllib.request.urlopen(req, jsondataasbytes)
    log.info("Status: %s" % {str(response.getcode())})

    data = json.loads(response.read().decode("utf8").replace("'", '"'))["data"]
    return data


@socket_timeout(120)
def download(url, loc, log):
    """Downloads from a url (with a timeout)."""
    log.info("Downloading %s" % url)
    try:
        urllib.request.urlretrieve(url, loc)
    except Exception as e:
        log.info(str(e))
        return False
    return True


def get_frames(video):
    """Gets all frames from a video into a list."""
    allframes = []
    while video.isOpened():
        ret, frame = video.read()
        if ret:
            # Convert to gray to simplify the process
            allframes.append(cv2.cvtColor(frame, cv2.COLOR_BGR2GRAY))
        else:
            video.release()
            break
    return allframes


def calculate_similarity(jobs_json, fetch_dir, output, log):
    """Calculates the similarity score against the last live site test.

    The technique works as follows:
        1. Get the last live site test.
        2. For each 15x15 video pairings, build a cross-correlation matrix:
            1. Get each of the videos and calculate their histograms
               across the full videos.
            2. Calculate the correlation coefficient between these two.
        3. Average the cross-correlation matrix to obtain the score.

    The 2D similarity score is the same, except that it builds a histogram
    from the final frame instead of the full video.

    For finding the last live site, we use active-data. We search for
    PGO android builds since this metric is only available for live sites that
    run on android in mozilla-cental. Given that live sites currently
    run on cron 3 days a week, then it's also reasonable to look for tasks
    which have occurred before today and within the last two weeks at most.
    But this is a TODO for future work, since we need to determine a better
    way of selecting the last task (HG push logs?) - there's a lot that factors
    into these choices, so it might require a multi-faceted approach.

    Args:
        jobs_json: The jobs JSON that holds extra information.
        fetch_dir: The fetch directory that holds the new videos.
        log: The logger.
    Returns:
        Two similarity scores (3D, 2D) as a float, or None if there was an issue.
    """
    app = jobs_json["application"]["name"]
    test = jobs_json["jobs"][0]["test_name"]
    splittest = test.split("-cold")

    cold = ""
    if len(splittest) > 0:
        cold = ".*cold"
    test = splittest[0]

    # PGO vs. OPT shouldn't matter much, but we restrict it to PGO builds here
    # for android, and desktop tests have the opt/pgo restriction removed
    plat = os.getenv("TC_PLATFORM", "")
    if "android" in plat:
        plat = plat.replace("/opt", "/pgo")
    else:
        plat = plat.replace("/opt", "").replace("/pgo", "")
    ad_query = {
        "from": "task",
        "limit": 1000,
        "where": {
            "and": [
                {
                    "regexp": {
                        "run.name": ".*%s.*browsertime.*-live.*%s%s.*%s.*"
                        % (plat, app, cold, test)
                    }
                },
                {"not": {"prefix": {"run.name": "test-vismet"}}},
                {"in": {"repo.branch.name": ["mozilla-central"]}},
                {"gte": {"action.start_time": {"date": "today-week-week"}}},
                {"lt": {"action.start_time": {"date": "today-1day"}}},
                {"in": {"task.run.state": ["completed"]}},
            ]
        },
        "select": ["action.start_time", "run.name", "task.artifacts"],
    }

    # Run the AD query and find the browsertime videos to download
    failed = False
    try:
        data = query_activedata(ad_query, log)
    except Exception as e:
        log.info(str(e))
        failed = True
    if failed or not data:
        log.info("Couldn't get activedata data")
        return None

    log.info("Found %s datums" % str(len(data["action.start_time"])))
    maxind = np.argmax([float(t) for t in data["action.start_time"]])
    artifacts = data["task.artifacts"][maxind]
    btime_artifact = None
    for art in artifacts:
        if "browsertime-results" in art["name"]:
            btime_artifact = art["url"]
            break
    if not btime_artifact:
        log.info("Can't find an older live site")
        return None

    # Download the browsertime videos and untar them
    tmpdir = tempfile.mkdtemp()
    loc = os.path.join(tmpdir, "tmpfile.tgz")
    if not download(btime_artifact, loc, log):
        return None
    tmploc = tempfile.mkdtemp()
    try:
        with tarfile.open(str(loc)) as tar:
            tar.extractall(path=tmploc)
    except Exception:
        log.info(
            "Could not read/extract old browsertime results archive",
            path=loc,
            exc_info=True,
        )
        return None

    # Find all the videos
    oldmp4s = [str(f) for f in pathlib.Path(tmploc).rglob("*.mp4")]
    log.info("Found %s old videos" % str(len(oldmp4s)))
    newmp4s = [str(f) for f in pathlib.Path(fetch_dir).rglob("*.mp4")]
    log.info("Found %s new videos" % str(len(newmp4s)))

    # Finally, calculate the 2D/3D score
    nhists = []
    nhists2d = []

    total_vids = min(len(oldmp4s), len(newmp4s))
    xcorr = np.zeros((total_vids, total_vids))
    xcorr2d = np.zeros((total_vids, total_vids))

    for i in range(total_vids):
        datao = np.asarray(get_frames(open_data(oldmp4s[i])))

        histo, _, _ = plt.hist(datao.flatten(), bins=255)
        histo2d, _, _ = plt.hist(datao[-1, :, :].flatten(), bins=255)

        for j in range(total_vids):
            if i == 0:
                # Only calculate the histograms once; it takes time
                datan = np.asarray(get_frames(open_data(newmp4s[j])))

                histn, _, _ = plt.hist(datan.flatten(), bins=255)
                histn2d, _, _ = plt.hist(datan[-1, :, :].flatten(), bins=255)

                nhists.append(histn)
                nhists2d.append(histn2d)
            else:
                histn = nhists[j]
                histn2d = nhists2d[j]

            rho, _ = spearmanr(histn, histo)
            rho2d, _ = spearmanr(histn2d, histo2d)

            xcorr[i, j] = rho
            xcorr2d[i, j] = rho2d

    similarity = np.mean(xcorr)
    similarity2d = np.mean(xcorr2d)

    log.info("Average 3D similarity: %s" % str(np.round(similarity, 5)))
    log.info("Average 2D similarity: %s" % str(np.round(similarity2d, 5)))

    if similarity < 0.5:
        # For really low correlations, output the worst video pairing
        # so that we can visually see what the issue was
        minind = np.unravel_index(np.argmin(xcorr, axis=None), xcorr.shape)

        oldvid = oldmp4s[minind[0]]
        shutil.copyfile(oldvid, str(pathlib.Path(output, "old_video.mp4")))

        newvid = newmp4s[minind[1]]
        shutil.copyfile(newvid, str(pathlib.Path(output, "new_video.mp4")))

    return np.round(similarity, 5), np.round(similarity2d, 5)
