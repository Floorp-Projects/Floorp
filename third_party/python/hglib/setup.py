import os, time
from distutils.core import setup

# query Mercurial for version number, or pull from PKG-INFO
version = 'unknown'
if os.path.isdir('.hg'):
    cmd = "hg id -i -t"
    l = os.popen(cmd).read().split()
    while len(l) > 1 and l[-1][0].isalpha(): # remove non-numbered tags
        l.pop()
    if len(l) > 1: # tag found
        version = l[-1]
        if l[0].endswith('+'): # propagate the dirty status to the tag
            version += '+'
    elif len(l) == 1: # no tag found
        cmd = 'hg parents --template "{latesttag}+{latesttagdistance}-"'
        version = os.popen(cmd).read() + l[0]
    if version.endswith('+'):
        version += time.strftime('%Y%m%d')
elif os.path.exists('.hg_archival.txt'):
    kw = dict([[t.strip() for t in l.split(':', 1)]
               for l in open('.hg_archival.txt')])
    if 'tag' in kw:
        version =  kw['tag']
    elif 'latesttag' in kw:
        version = '%(latesttag)s+%(latesttagdistance)s-%(node).12s' % kw
    else:
        version = kw.get('node', '')[:12]
elif os.path.exists('PKG-INFO'):
    kw = dict([[t.strip() for t in l.split(':', 1)]
               for l in open('PKG-INFO') if ':' in l])
    version = kw.get('Version', version)

setup(
    name='python-hglib',
    version=version,
    author='Idan Kamara',
    author_email='idankk86@gmail.com',
    url='http://selenic.com/repo/python-hglib',
    description='Mercurial Python library',
    long_description=open(os.path.join(os.path.dirname(__file__),
                                       'README')).read(),
    classifiers=[
        'Programming Language :: Python',
        'Programming Language :: Python :: 2.4',
        'Programming Language :: Python :: 2.5',
        'Programming Language :: Python :: 2.6',
        'Programming Language :: Python :: 2.7',
        'Programming Language :: Python :: 3',
        'Programming Language :: Python :: 3.4',

    ],
    license='MIT',
    packages=['hglib'])
