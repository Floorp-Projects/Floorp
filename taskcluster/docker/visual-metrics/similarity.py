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
import structlog
import tarfile
import tempfile
import urllib

from functools import wraps
from matplotlib import pyplot as plt
from scipy.stats import spearmanr


log = None


# We add the `and` conditions to it later
base_ad_query = {
    "from": "task",
    "limit": 1000,
    "where": {"and": []},
    "select": [
        "action.start_time",
        "run.name",
        "task.artifacts",
        "task.group.id",
        "task.id",
    ],
}


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


def _open_data(file):
    return cv2.VideoCapture(str(file))


@socket_timeout(120)
def _query_activedata(query_json):
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
def _download(url, loc):
    """Downloads from a url (with a timeout)."""
    log.info("Downloading %s" % url)
    try:
        urllib.request.urlretrieve(url, loc)
    except Exception as e:
        log.info(str(e))
        return False
    return True


def _get_frames(video):
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


def _get_browsertime_results(query):
    """Used to run an AD query and extract the browsertime results if they exist."""
    failed = False
    try:
        data = _query_activedata(query)
    except Exception as e:
        log.info(str(e))
        failed = True
    if failed or not data:
        log.info("Couldn't get activedata data")
        return None

    # Find the newest browsertime task
    log.info("Found %s datums" % str(len(data["action.start_time"])))
    maxind = np.argmax([float(t) for t in data["action.start_time"]])
    artifacts = data["task.artifacts"][maxind]
    btime_artifact = None
    for art in artifacts:
        if "browsertime-results" in art["name"]:
            btime_artifact = art["url"]
            break
    if not btime_artifact:
        log.info("Can't find an older site test")
        return None

    log.info(
        "Comparing videos to TASK_GROUP=%s, TASK_ID=%s"
        % (data["task.group.id"][maxind], data["task.id"][maxind])
    )

    # Download the browsertime videos and untar them
    tmpdir = tempfile.mkdtemp()
    loc = os.path.join(tmpdir, "tmpfile.tgz")
    if not _download(btime_artifact, loc):
        log.info(
            "Failed to download browsertime-results artifact from %s" % btime_artifact
        )
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

    return tmploc


def _data_from_last_task(label):
    """Gets the data from the last PGO/OPT task with the same label.

    We look for both OPT and PGO tasks. The difference
    between them should be minimal. This method also provides
    a way to compare recordings from this task to another
    known task based on the TC_GROUP_ID environment varible.
    """
    label_opt = label.replace("/pgo", "/opt")
    label_pgo = label.replace("/opt", "/pgo")

    base_ad_query["where"]["and"] = [
        {"in": {"task.run.state": ["completed"]}},
        {"or": [{"eq": {"run.name": label_pgo}}, {"eq": {"run.name": label_opt}}]},
    ]

    task_group_id = os.getenv("TC_GROUP_ID", "")
    if task_group_id:
        base_ad_query["where"]["and"].append({"eq": {"task.group.id": task_group_id}})
    else:
        base_ad_query["where"]["and"].extend(
            [
                {"in": {"repo.branch.name": ["mozilla-central"]}},
                {"gte": {"action.start_time": {"date": "today-week-week"}}},
            ]
        )

    return _get_browsertime_results(base_ad_query)


def _data_from_last_live_task(label):
    """Gets the data from the last live site PGO task."""
    label_live = label.replace("/opt", "/pgo").replace("tp6m", "tp6m-live")

    base_ad_query["where"]["and"] = [
        {"in": {"repo.branch.name": ["mozilla-central"]}},
        {"gte": {"action.start_time": {"date": "today-week-week"}}},
        {"in": {"task.run.state": ["completed"]}},
        {"eq": {"run.name": label_live}},
    ]

    return _get_browsertime_results(base_ad_query)


def _get_similarity(old_videos_info, new_videos_info, output, prefix=""):
    """Calculates a similarity score for two groupings of videos.

    The technique works as follows:
        1. Get the last live site test.
        2. For each 15x15 video pairings, build a cross-correlation matrix:
            1. Get each of the videos and calculate their histograms
               across the full videos.
            2. Calculate the correlation coefficient between these two.
        3. Average the cross-correlation matrix to obtain the score.

    The 2D similarity score is the same, except that it builds a histogram
    from the final frame instead of the full video.

    Args:
        old_videos: List of old videos.
        new_videos: List of new videos (from this task).
        output: Location to output videos with low similarity scores.
        prefix: Prefix a string to the output.
    Returns:
        Two similarity scores (3D, 2D) as a float.
    """
    nhists = []
    nhists2d = []

    old_videos = [entry["data"] for entry in old_videos_info]
    new_videos = [entry["data"] for entry in new_videos_info]

    total_vids = min(len(old_videos), len(new_videos))
    xcorr = np.zeros((total_vids, total_vids))
    xcorr2d = np.zeros((total_vids, total_vids))

    for i in range(total_vids):
        datao = np.asarray(_get_frames(old_videos[i]))

        histo, _, _ = plt.hist(datao.flatten(), bins=255)
        histo2d, _, _ = plt.hist(datao[-1, :, :].flatten(), bins=255)

        for j in range(total_vids):
            if i == 0:
                # Only calculate the histograms once; it takes time
                datan = np.asarray(_get_frames(new_videos[j]))

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

    if np.round(similarity, 1) <= 0.7 or np.round(similarity2d, 1) <= 0.7:
        # For low correlations, output the worst video pairing
        # so that we can visually see what the issue was
        minind = np.unravel_index(np.argmin(xcorr, axis=None), xcorr.shape)

        oldvid = old_videos_info[minind[0]]["path"]
        shutil.copyfile(oldvid, str(pathlib.Path(output, "%sold_video.mp4" % prefix)))

        newvid = new_videos_info[minind[1]]["path"]
        shutil.copyfile(newvid, str(pathlib.Path(output, "%snew_video.mp4" % prefix)))

    return np.round(similarity, 5), np.round(similarity2d, 5)


def calculate_similarity(jobs_json, fetch_dir, output):
    """Calculates the similarity score for this task.

    Here we use activedata to find the last live site that ran and
    to find the last task (with the same label) that ran. Those two
    tasks are then compared to the current one and 4 metrics are produced.

    For live sites, we only calculate 2 of these metrics, since the
    playback similarity is not applicable to it.

    Args:
        jobs_json: The jobs JSON that holds extra information.
        fetch_dir: The fetch directory that holds the new videos.
        output: The output directory.
    Returns:
        A dictionary containing up to 4 different metrics (their values default
        to None if a metric couldn't be calculated):
            PlaybackSimilarity: Similarity of the full playback to a live site test.
            PlaybackSimilarity2D: - // - (but for the final frame only)
            Similarity: Similarity of the tests video recording to its last run.
            Similarity2D: - // - (but for the final frame only)
    """
    global log
    log = structlog.get_logger()

    label = os.getenv("TC_LABEL", "")
    if not label:
        log.info("TC_LABEL is undefined, cannot calculate similarity metrics")
        return {}

    # Get all the newest videos from this task
    new_btime_videos = [
        {"data": _open_data(str(f)), "path": str(f)}
        for f in pathlib.Path(fetch_dir).rglob("*.mp4")
    ]
    log.info("Found %s new videos" % str(len(new_btime_videos)))

    # Get the similarity against the last task
    old_btime_res = _data_from_last_task(label)
    old_sim = old_sim2d = None
    if old_btime_res:
        old_btime_videos = [
            {"data": _open_data(str(f)), "path": str(f)}
            for f in pathlib.Path(old_btime_res).rglob("*.mp4")
        ]
        log.info("Found %s old videos" % str(len(old_btime_videos)))

        old_sim, old_sim2d = _get_similarity(old_btime_videos, new_btime_videos, output)
    else:
        log.info("Failed to find an older test task")

    # Compare recordings to their live site variant if it exists
    live_sim = live_sim2d = None
    if "live" not in jobs_json["extra_options"]:
        live_btime_res = _data_from_last_live_task(label)
        if live_btime_res:
            live_btime_videos = [
                {"data": _open_data(str(f)), "path": str(f)}
                for f in pathlib.Path(live_btime_res).rglob("*.mp4")
            ]
            log.info("Found %s live videos" % str(len(live_btime_videos)))

            live_sim, live_sim2d = _get_similarity(
                live_btime_videos, new_btime_videos, output, prefix="live_"
            )
        else:
            log.info("Failed to find a live site variant")

    return {
        "PlaybackSimilarity": live_sim,
        "PlaybackSimilarity2D": live_sim2d,
        "Similarity": old_sim,
        "Similarity2D": old_sim2d,
    }
