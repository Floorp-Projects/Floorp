#!/usr/bin/env python3
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

import aiohttp
import asyncio
import logging
import os
from os import path
import glob
import sys
import xml.etree.ElementTree as ET

logging.basicConfig(stream=sys.stdout, level=logging.INFO, format="%(message)s")
log = logging.getLogger(__name__)

UV_CACHE_PATH = os.getenv(
    "UV_CACHE_PATH", os.path.join(path.dirname(__file__), "../release/updates/cache/")
)
UV_PARALLEL_DOWNLOADS = os.getenv("UV_PARALLEL_DOWNLOADS", 20)

FTP_SERVER_TO = os.getenv("ftp_server_to", "http://stage.mozilla.org/pub/mozilla.org")
FTP_SERVER_FROM = os.getenv(
    "ftp_server_from", "http://stage.mozilla.org/pub/mozilla.org"
)
AUS_SERVER = os.getenv("aus_server", "https://aus5.mozilla.org")


def create_cache():
    if not os.path.isdir(UV_CACHE_PATH):
        os.mkdir(UV_CACHE_PATH)


def remove_cache():
    """
    Removes all files in the cache folder
    We don't support folders or .dot(hidden) files
    By not deleting the cache directory, it allows us to use Docker tmpfs mounts,
    which are the only workaround to poor mount r/w performance on MacOS
    Bug Reference:
    https://forums.docker.com/t/file-access-in-mounted-volumes-extremely-slow-cpu-bound/8076/288
    """
    files = glob.glob(f"{UV_CACHE_PATH}/*")
    for f in files:
        os.remove(f)


def _cachepath(i, ext):
    # Helper function: given an index, return a cache file path
    return path.join(UV_CACHE_PATH, f"obj_{i:0>5}.{ext}")


async def fetch_url(url, path, connector):
    """
    Fetch/download a file to a specific path

    Parameters
    ----------
    url : str
        URL to be fetched

    path : str
        Path to save binary

    Returns
    -------
    dict
        Request result. If error result['error'] is True
    """

    def _result(response, error=False):
        data = {
            "headers": dict(response.headers),
            "status": response.status,
            "reason": response.reason,
            "_request_info": str(response._request_info),
            "url": url,
            "path": path,
            "error": error,
        }
        return data

    # Set connection timeout to 15 minutes
    timeout = aiohttp.ClientTimeout(total=900)

    try:
        async with aiohttp.ClientSession(
            connector=connector, connector_owner=False, timeout=timeout
        ) as session:
            log.info(f"Retrieving {url}")
            async with session.get(
                url, headers={"Cache-Control": "max-stale=0"}
            ) as response:
                # Any response code > 299 means something went wrong
                if response.status > 299:
                    log.info(f"Failed to download {url} with status {response.status}")
                    return _result(response, True)

                with open(path, "wb") as fd:
                    while True:
                        chunk = await response.content.read()
                        if not chunk:
                            break
                        fd.write(chunk)
                result = _result(response)
                log.info(f'Finished downloading {url}\n{result["headers"]}')
                return result

    except (
        UnicodeDecodeError,  # Data parsing
        asyncio.TimeoutError,  # Async timeout
        aiohttp.ClientError,  # aiohttp error
    ) as e:
        log.error("=============")
        log.error(f"Error downloading {url}")
        log.error(e)
        log.error("=============")
        return {"path": path, "url": url, "error": True}


async def download_multi(targets, sourceFunc):
    """
    Download list of targets

    Parameters
    ----------
    targets : list
        List of urls to download

    sourceFunc : str
        Source function name (for filename)

    Returns
    -------
    tuple
        List of responses (Headers)
    """

    targets = set(targets)
    amount = len(targets)

    connector = aiohttp.TCPConnector(
        limit=UV_PARALLEL_DOWNLOADS,  # Simultaneous connections, per host
        ttl_dns_cache=600,  # Cache DNS for 10 mins
    )

    log.info(f"\nDownloading {amount} files ({UV_PARALLEL_DOWNLOADS} async limit)")

    # Transform targets into {url, path} objects
    payloads = [
        {"url": url, "path": _cachepath(i, sourceFunc)}
        for (i, url) in enumerate(targets)
    ]

    downloads = []

    fetches = [fetch_url(t["url"], t["path"], connector) for t in payloads]

    downloads.extend(await asyncio.gather(*fetches))
    connector.close()

    results = []
    # Remove file if download failed
    for fetch in downloads:
        # If there's an error, try to remove the file, but keep going if file not present
        if fetch["error"]:
            try:
                os.unlink(fetch.get("path", None))
            except (TypeError, FileNotFoundError) as e:
                log.info(f"Unable to cleanup error file: {e} continuing...")
            continue

        results.append(fetch)

    return results


async def download_builds(verifyConfig):
    """
    Given UpdateVerifyConfig, download and cache all necessary updater files
    Include "to" and "from"/"updater_pacakge"

    Parameters
    ----------
    verifyConfig : UpdateVerifyConfig
        Chunked config

    Returns
    -------
    list : List of file paths and urls to each updater file
    """

    updaterUrls = set()
    for release in verifyConfig.releases:
        ftpServerFrom = release["ftp_server_from"]
        ftpServerTo = release["ftp_server_to"]

        for locale in release["locales"]:
            toUri = verifyConfig.to
            if toUri is not None and ftpServerTo is not None:
                toUri = toUri.replace("%locale%", locale)
                updaterUrls.add(f"{ftpServerTo}{toUri}")

            for reference in ("updater_package", "from"):
                uri = release.get(reference, None)
                if uri is None:
                    continue
                uri = uri.replace("%locale%", locale)
                # /ja-JP-mac/ locale is replaced with /ja/ for updater packages
                uri = uri.replace("ja-JP-mac", "ja")
                updaterUrls.add(f"{ftpServerFrom}{uri}")

    log.info(f"About to download {len(updaterUrls)} updater packages")

    updaterResults = await download_multi(list(updaterUrls), "updater.async.cache")
    return updaterResults


def get_mar_urls_from_update(path):
    """
    Given an update.xml file, return MAR URLs

    If update.xml doesn't have URLs, returns empty list

    Parameters
    ----------
    path : str
        Path to update.xml file

    Returns
    -------
    list : List of URLs
    """

    result = []
    root = ET.parse(path).getroot()
    for patch in root.findall("update/patch"):
        url = patch.get("URL")
        if url:
            result.append(url)
    return result


async def download_mars(updatePaths):
    """
    Given list of update.xml paths, download MARs for each

    Parameters
    ----------
    update_paths : list
        List of paths to update.xml files
    """

    patchUrls = set()
    for updatePath in updatePaths:
        for url in get_mar_urls_from_update(updatePath):
            patchUrls.add(url)

    log.info(f"About to download {len(patchUrls)} MAR packages")
    marResults = await download_multi(list(patchUrls), "mar.async.cache")
    return marResults


async def download_update_xml(verifyConfig):
    """
    Given UpdateVerifyConfig, download and cache all necessary update.xml files

    Parameters
    ----------
    verifyConfig : UpdateVerifyConfig
        Chunked config

    Returns
    -------
    list : List of file paths and urls to each update.xml file
    """

    xmlUrls = set()
    product = verifyConfig.product
    urlTemplate = (
        "{server}/update/3/{product}/{release}/{build}/{platform}/"
        "{locale}/{channel}/default/default/default/update.xml?force=1"
    )

    for release in verifyConfig.releases:
        for locale in release["locales"]:
            xmlUrls.add(
                urlTemplate.format(
                    server=AUS_SERVER,
                    product=product,
                    release=release["release"],
                    build=release["build_id"],
                    platform=release["platform"],
                    locale=locale,
                    channel=verifyConfig.channel,
                )
            )

    log.info(f"About to download {len(xmlUrls)} update.xml files")
    xmlResults = await download_multi(list(xmlUrls), "update.xml.async.cache")
    return xmlResults


async def _download_from_config(verifyConfig):
    """
    Given an UpdateVerifyConfig object, download all necessary files to cache

    Parameters
    ----------
    verifyConfig : UpdateVerifyConfig
        The config - already chunked
    """
    remove_cache()
    create_cache()

    downloadList = []
    ##################
    # Download files #
    ##################
    xmlFiles = await download_update_xml(verifyConfig)
    downloadList.extend(xmlFiles)
    downloadList += await download_mars(x["path"] for x in xmlFiles)
    downloadList += await download_builds(verifyConfig)

    #####################
    # Create cache.list #
    #####################
    cacheLinks = []

    # Rename files and add to cache_links
    for download in downloadList:
        cacheLinks.append(download["url"])
        fileIndex = len(cacheLinks)
        os.rename(download["path"], _cachepath(fileIndex, "cache"))

    cacheIndexPath = path.join(UV_CACHE_PATH, "urls.list")
    with open(cacheIndexPath, "w") as cache:
        cache.writelines(f"{l}\n" for l in cacheLinks)

    # Log cache
    log.info("Cache index urls.list contents:")
    with open(cacheIndexPath, "r") as cache:
        for ln, url in enumerate(cache.readlines()):
            line = url.replace("\n", "")
            log.info(f"Line {ln+1}: {line}")

    return None


def download_from_config(verifyConfig):
    """
    Given an UpdateVerifyConfig object, download all necessary files to cache
    (sync function that calls the async one)

    Parameters
    ----------
    verifyConfig : UpdateVerifyConfig
        The config - already chunked
    """
    return asyncio.run(_download_from_config(verifyConfig))
