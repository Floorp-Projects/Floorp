from setuptools import setup

setup(
    name="fake_with_deps",
    version=0.1,
    install_requires=[
        "python-dateutil>=2.4.2,<2.5",
        "colorama<0.4.0,>=0.3.7",
        "cornice<1.1,>=1.0.0",
        "enum34<1.1.7,>=1.0.4",
        "six>1.5,<=1.8",
        "futures<3.1,>=3.0.3",
        "ipaddress<1.1,>=1.0.16",
        "jsonschema<3.0,>=2.4.0",
        "pyramid<1.6,>=1.5.7",
        "pyzmq<14.8,>=14.7.0",
        "simplejson>=3.5,!=3.8,>3.9",
        "SQLAlchemy!=0.9.5,<2.0.0,>=0.7.8,>=1.0.0",
        "python-memcached>=1.57,<2.0",
        "xmltodict<=0.11,>=0.4.6",
    ],
)
