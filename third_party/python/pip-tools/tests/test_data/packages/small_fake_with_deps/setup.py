from setuptools import setup

setup(
    name="small_fake_with_deps",
    version=0.1,
    install_requires=["small-fake-a==0.1", "small-fake-b==0.1"],
)
