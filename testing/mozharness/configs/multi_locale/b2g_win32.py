import sys

config = {
    "exes": {
        "hg": "c:/mozilla-build/hg/hg",
        "make": [sys.executable, "%(abs_work_dir)s/build/build/pymake/make.py"],
    },
}
