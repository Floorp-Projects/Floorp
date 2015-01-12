from setuptools import setup

setup(
    name="redo",
    version="1.4",
    description="Utilities to retry Python callables.",
    author="Ben Hearsum",
    author_email="ben@hearsum.ca",
    packages=["redo"],
    entry_points={
        "console_scripts": ["retry = redo.cmd:main"],
    },
    url="https://github.com/bhearsum/redo",
)
