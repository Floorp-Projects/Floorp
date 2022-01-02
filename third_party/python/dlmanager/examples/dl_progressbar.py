import argparse

# for python 3, use https://github.com/coagulant/progressbar-python3
from progressbar import ProgressBar, Percentage, RotatingMarker, ETA, \
    FileTransferSpeed, Bar

from six.moves.urllib.parse import urlparse

from dlmanager import Download


def parse_args(argv=None):
    parser = argparse.ArgumentParser()
    parser.add_argument("url", help="url to download")
    return parser.parse_args(argv)


def download_file(url, dest=None):
    if dest is None:
        dest = urlparse(url).path.split('/')[-1]

    widgets = ['Download: ', Percentage(), ' ', Bar(marker=RotatingMarker()),
               ' ', ETA(), ' ', FileTransferSpeed()]
    bar = ProgressBar(widgets=widgets).start()

    def download_progress(_, current, total):
        bar.maxval = total
        bar.update(current)

    dl = Download(url, dest, progress=download_progress)
    dl.start()
    dl.wait()
    bar.finish()


if __name__ == '__main__':
    options = parse_args()
    try:
        download_file(options.url)
    except KeyboardInterrupt:
        print("\nInterrupted.")
