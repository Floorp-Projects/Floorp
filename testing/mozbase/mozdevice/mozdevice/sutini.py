# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

import ConfigParser
import StringIO
import os
import sys
import tempfile

from mozdevice.droid import DroidSUT
from mozdevice.devicemanager import DMError

USAGE = '%s <host>'
INI_PATH_JAVA = '/data/data/com.mozilla.SUTAgentAndroid/files/SUTAgent.ini'
INI_PATH_NEGATUS = '/data/local/SUTAgent.ini'
SCHEMA = {'Registration Server': (('IPAddr', ''),
                                  ('PORT', '28001'),
                                  ('HARDWARE', ''),
                                  ('POOL', '')),
          'Network Settings': (('SSID', ''),
                               ('AUTH', ''),
                               ('ENCR', ''),
                               ('EAP', ''))}

def get_cfg(d, ini_path):
    cfg = ConfigParser.RawConfigParser()
    try:
        cfg.readfp(StringIO.StringIO(d.pullFile(ini_path)), 'SUTAgent.ini')
    except DMError:
        # assume this is due to a missing file...
        pass
    return cfg


def put_cfg(d, cfg, ini_path):
    print 'Writing modified SUTAgent.ini...'
    t = tempfile.NamedTemporaryFile(delete=False)
    cfg.write(t)
    t.close()
    try:
        d.pushFile(t.name, ini_path)
    except DMError, e:
        print e
    else:
        print 'Done.'
    finally:
        os.unlink(t.name)


def set_opt(cfg, s, o, dflt):
    prompt = '  %s' % o
    try:
        curval = cfg.get(s, o)
    except (ConfigParser.NoSectionError, ConfigParser.NoOptionError):
        curval = ''
    if curval:
        dflt = curval
    prompt += ': '
    if dflt:
        prompt += '[%s] ' % dflt
    newval = raw_input(prompt)
    if not newval:
        newval = dflt
    if newval == curval:
        return False
    cfg.set(s, o, newval)
    return True


def bool_query(prompt, dflt):
    while True:
        i = raw_input('%s [%s] ' % (prompt, 'y' if dflt else 'n')).lower()
        if not i or i[0] in ('y', 'n'):
            break
        print 'Enter y or n.'
    return (not i and dflt) or (i and i[0] == 'y')


def edit_sect(cfg, sect, opts):
    changed_vals = False
    if bool_query('Edit section %s?' % sect, False):
        if not cfg.has_section(sect):
            cfg.add_section(sect)
        print '%s settings:' % sect
        for opt, dflt in opts:
            changed_vals |= set_opt(cfg, sect, opt, dflt)
        print
    else:
        if cfg.has_section(sect) and bool_query('Delete section %s?' % sect,
                                                False):
            cfg.remove_section(sect)
            changed_vals = True
    return changed_vals


def main():
    try:
        host = sys.argv[1]
    except IndexError:
        print USAGE % sys.argv[0]
        sys.exit(1)
    try:
        d = DroidSUT(host, retryLimit=1)
    except DMError, e:
        print e
        sys.exit(1)
    # check if using Negatus and change path accordingly
    ini_path = INI_PATH_JAVA
    if 'Negatus' in d.agentProductName:
        ini_path = INI_PATH_NEGATUS
    cfg = get_cfg(d, ini_path)
    if not cfg.sections():
        print 'Empty or missing ini file.'
    changed_vals = False
    for sect, opts in SCHEMA.iteritems():
        changed_vals |= edit_sect(cfg, sect, opts)
    if changed_vals:
        put_cfg(d, cfg, ini_path)
    else:
        print 'No changes.'


if __name__ == '__main__':
    main()
