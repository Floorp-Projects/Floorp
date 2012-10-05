import sys
import _winreg

def main():
    for version in ["7.1", "7.0"]:
        try:
            key = _winreg.OpenKey(_winreg.HKEY_LOCAL_MACHINE, r"SOFTWARE\Microsoft\Microsoft SDKs\Windows\v%s\WinSDKSamples" % version)
            path, typ = _winreg.QueryValueEx(key, "InstallationFolder")
            print path.replace("\\", "/")
            return 0
        except:
            pass
    print >>sys.stderr, "Could not locate Windows SDK Samples directory via the registry"
    return 1

if __name__ == '__main__':
    sys.exit(main())
