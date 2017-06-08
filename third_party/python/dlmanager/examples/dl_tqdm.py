import argparse
import tqdm

from six.moves.urllib.parse import urlparse

from dlmanager import Download


def parse_args(argv=None):
    parser = argparse.ArgumentParser()
    parser.add_argument("url", help="url to download")
    return parser.parse_args(argv)


def download_progress(bar):
    last_b = [0]

    def inner(_, current, total):
        if total is not None:
            bar.total = total
        delta = current - last_b[0]
        last_b[0] = current

        if delta > 0:
            bar.update(delta)
    return inner


def download_file(url, dest=None):
    if dest is None:
        dest = urlparse(url).path.split('/')[-1]

    with tqdm.tqdm(unit='B', unit_scale=True, miniters=1, dynamic_ncols=True,
                   desc=dest) as bar:
        dl = Download(url, dest, progress=download_progress(bar))
        dl.start()
        dl.wait()


if __name__ == '__main__':
    options = parse_args()
    try:
        download_file(options.url)
    except KeyboardInterrupt:
        print("\nInterrupted.")
