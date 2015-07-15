from setuptools import setup, find_packages


setup(
    name='xpidl',
    version='1.0',
    description='Parser and header generator for xpidl files.',
    author='Mozilla Foundation',
    license='MPL 2.0',
    packages=find_packages(),
    install_requires=['ply>=3.6,<4.0'],
    url='https://github.com/pelmers/xpidl',
    entry_points={'console_scripts': ['header.py = xpidl.header:main']},
    keywords=['xpidl', 'parser']
)
