import random
import os
import asyncio

from condprof.util import get_logger
from condprof.helpers import TabSwitcher


WORDS = os.path.join(os.path.dirname(__file__), "words.txt")
with open(WORDS) as f:
    WORDS = f.readlines()


URLS = os.path.join(os.path.dirname(__file__), "urls.txt")
with open(URLS) as f:
    URLS = f.readlines()


URL_LIST = []


def _build_url_list():
    for url in URLS:
        url = url.strip()
        if url.startswith("#"):
            continue
        for word in WORDS:
            word = word.strip()
            if word.startswith("#"):
                continue
            URL_LIST.append(url.format(word))
    random.shuffle(URL_LIST)


_build_url_list()


async def heavy(session, options):
    metadata = {}
    max = options.get("max_urls", 150)
    # see Bug 1608604 - on GV we get OOM killed if we do too much...
    platform = options.get("platform", "")
    if "gecko" in platform:
        max = 30
    tabs = TabSwitcher(session, options)
    await tabs.create_windows()
    visited = 0

    for current, url in enumerate(URL_LIST):
        get_logger().visit_url(index=current + 1, total=max, url=url)
        retries = 0
        while retries < 3:
            try:
                await asyncio.wait_for(session.get(url), 5)
                visited += 1
                break
            except asyncio.TimeoutError:
                retries += 1

        if max != -1 and current + 1 == max:
            break

        # switch to the next tab
        await tabs.switch()

    metadata["visited_url"] = visited
    return metadata
