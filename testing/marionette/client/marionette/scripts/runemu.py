from marionette import Marionette, Emulator
from optparse import OptionParser


def runemulator(homedir=None, url=None, pidfile=None, arch='x86', noWindow=False,
                userdata=None):
    qemu = Emulator(homedir=homedir, arch=arch, noWindow=noWindow,
                    userdata=userdata)
    qemu.start()
    port = qemu.setup_port_forwarding(2828)
    assert(qemu.wait_for_port())
    if pidfile:
        f = open(pidfile, 'w')
        f.write("%d" % qemu.proc.pid)
        f.close()
    print 'emulator launched, pid:', qemu.proc.pid

    if url:
        marionette = Marionette(port=port)
        marionette.start_session()
        marionette.navigate(url)
        marionette.delete_session()

if __name__ == '__main__':
    parser = OptionParser()
    parser.add_option('--repo', dest='repo_path', action='store',
                      help='directory of the B2G repo')
    parser.add_option('--arch', dest='arch', action='store',
                      default='x86',
                      help='the emulator cpu architecture (x86 or arm)')
    parser.add_option('--url', dest='url', action='store',
                      help='url to navigate to after launching emulator')
    parser.add_option('--pidfile', dest='pidfile', action='store',
                      help='file in which to store emulator pid')
    parser.add_option('--no-window', dest='noWindow', action='store_true',
                      help='pass -no-window to the emulator')
    parser.add_option('--userdata', dest='userdata', action='store',
                      help='path to userdata.img file to use')

    options, args = parser.parse_args()
    if not options.repo_path:
        raise Exception ("must specify the --repo /path/to/B2G/repo argument")

    runemulator(homedir=options.repo_path,
                url=options.url,
                pidfile=options.pidfile,
                arch=options.arch,
                noWindow=options.noWindow,
                userdata=options.userdata)

