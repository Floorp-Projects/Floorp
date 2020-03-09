from setuptools import setup

setup(
    name="small_fake_with_unpinned_deps",
    version=0.1,
    install_requires=["small-fake-a", "small-fake-b"],
)
