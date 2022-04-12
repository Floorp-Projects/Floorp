#!/usr/bin/python
#
# Copyright (c) 2019 Martin Storsjo
#
# Permission to use, copy, modify, and/or distribute this software for any
# purpose with or without fee is hereby granted, provided that the above
# copyright notice and this permission notice appear in all copies.
#
# THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
# WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
# MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
# ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
# WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
# ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
# OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.

import argparse
import functools
import hashlib
import os
import multiprocessing.pool
try:
    import simplejson
except ModuleNotFoundError:
    import json as simplejson
import six
import shutil
import socket
import subprocess
import sys
import tempfile
import zipfile

def getArgsParser():
    parser = argparse.ArgumentParser(description = "Download and install Visual Studio")
    parser.add_argument("--manifest", metavar="manifest", help="A predownloaded manifest file")
    parser.add_argument("--save-manifest", const=True, action="store_const", help="Store the downloaded manifest to a file")
    parser.add_argument("--major", default=17, metavar="version", help="The major version to download (defaults to 17)")
    parser.add_argument("--preview", dest="type", default="release", const="pre", action="store_const", help="Download the preview version instead of the release version")
    parser.add_argument("--cache", metavar="dir", help="Directory to use as a persistent cache for downloaded files")
    parser.add_argument("--dest", metavar="dir", help="Directory to install into")
    parser.add_argument("package", metavar="package", help="Package to install. If omitted, installs the default command line tools.", nargs="*")
    parser.add_argument("--ignore", metavar="component", help="Package to skip", action="append")
    parser.add_argument("--accept-license", const=True, action="store_const", help="Don't prompt for accepting the license")
    parser.add_argument("--print-version", const=True, action="store_const", help="Stop after fetching the manifest")
    parser.add_argument("--list-workloads", const=True, action="store_const", help="List high level workloads")
    parser.add_argument("--list-components", const=True, action="store_const", help="List available components")
    parser.add_argument("--list-packages", const=True, action="store_const", help="List all individual packages, regardless of type")
    parser.add_argument("--include-optional", const=True, action="store_const", help="Include all optional dependencies")
    parser.add_argument("--skip-recommended", const=True, action="store_const", help="Don't include recommended dependencies")
    parser.add_argument("--print-deps-tree", const=True, action="store_const", help="Print a tree of resolved dependencies for the given selection")
    parser.add_argument("--print-reverse-deps", const=True, action="store_const", help="Print a tree of packages that depend on the given selection")
    parser.add_argument("--print-selection", const=True, action="store_const", help="Print a list of the individual packages that are selected to be installed")
    parser.add_argument("--only-download", const=True, action="store_const", help="Stop after downloading package files")
    parser.add_argument("--only-unpack", const=True, action="store_const", help="Unpack the selected packages and keep all files, in the layout they are unpacked, don't restructure and prune files other than what's needed for MSVC CLI tools")
    parser.add_argument("--keep-unpack", const=True, action="store_const", help="Keep the unpacked files that aren't otherwise selected as needed output")
    parser.add_argument("--msvc-version", metavar="version", help="Install a specific MSVC toolchain version")
    parser.add_argument("--sdk-version", metavar="version", help="Install a specific Windows SDK version")
    return parser

def setPackageSelectionMSVC16(args, packages, userversion, sdk, toolversion, defaultPackages):
    if findPackage(packages, "Microsoft.VisualStudio.Component.VC." + toolversion + ".x86.x64", None, warn=False):
        args.package.extend(["Win10SDK_" + sdk, "Microsoft.VisualStudio.Component.VC." + toolversion + ".x86.x64", "Microsoft.VisualStudio.Component.VC." + toolversion + ".ARM", "Microsoft.VisualStudio.Component.VC." + toolversion + ".ARM64"])
    else:
        # Options for toolchains for specific versions. The latest version in
        # each manifest isn't available as a pinned version though, so if that
        # version is requested, try the default version.
        print("Didn't find exact version packages for " + userversion + ", assuming this is provided by the default/latest version")
        args.package.extend(defaultPackages)

def setPackageSelectionMSVC15(args, packages, userversion, sdk, toolversion, defaultPackages):
    if findPackage(packages, "Microsoft.VisualStudio.Component.VC.Tools." + toolversion, None, warn=False):
        args.package.extend(["Win10SDK_" + sdk, "Microsoft.VisualStudio.Component.VC.Tools." + toolversion])
    else:
        # Options for toolchains for specific versions. The latest version in
        # each manifest isn't available as a pinned version though, so if that
        # version is requested, try the default version.
        print("Didn't find exact version packages for " + userversion + ", assuming this is provided by the default/latest version")
        args.package.extend(defaultPackages)

def setPackageSelection(args, packages):
    # If no packages are selected, install these versionless packages, which
    # gives the latest/recommended version for the current manifest.
    defaultPackages = ["Microsoft.VisualStudio.Workload.VCTools", "Microsoft.VisualStudio.Component.VC.Tools.ARM", "Microsoft.VisualStudio.Component.VC.Tools.ARM64"]

    # Note, that in the manifest for MSVC version X.Y, only version X.Y-1
    # exists with a package name like "Microsoft.VisualStudio.Component.VC."
    # + toolversion + ".x86.x64".
    if args.msvc_version == "16.0":
        setPackageSelectionMSVC16(args, packages, args.msvc_version, "10.0.17763", "14.20", defaultPackages)
    elif args.msvc_version == "16.1":
        setPackageSelectionMSVC16(args, packages, args.msvc_version, "10.0.18362", "14.21", defaultPackages)
    elif args.msvc_version == "16.2":
        setPackageSelectionMSVC16(args, packages, args.msvc_version, "10.0.18362", "14.22", defaultPackages)
    elif args.msvc_version == "16.3":
        setPackageSelectionMSVC16(args, packages, args.msvc_version, "10.0.18362", "14.23", defaultPackages)
    elif args.msvc_version == "16.4":
        setPackageSelectionMSVC16(args, packages, args.msvc_version, "10.0.18362", "14.24", defaultPackages)
    elif args.msvc_version == "16.5":
        setPackageSelectionMSVC16(args, packages, args.msvc_version, "10.0.18362", "14.25", defaultPackages)
    elif args.msvc_version == "16.6":
        setPackageSelectionMSVC16(args, packages, args.msvc_version, "10.0.18362", "14.26", defaultPackages)
    elif args.msvc_version == "16.7":
        setPackageSelectionMSVC16(args, packages, args.msvc_version, "10.0.18362", "14.27", defaultPackages)
    elif args.msvc_version == "16.8":
        setPackageSelectionMSVC16(args, packages, args.msvc_version, "10.0.18362", "14.28", defaultPackages)
    elif args.msvc_version == "16.9":
        setPackageSelectionMSVC16(args, packages, args.msvc_version, "10.0.19041", "14.28.16.9", defaultPackages)
    elif args.msvc_version == "16.10":
        setPackageSelectionMSVC16(args, packages, args.msvc_version, "10.0.19041", "14.29.16.10", defaultPackages)
    elif args.msvc_version == "16.11":
        setPackageSelectionMSVC16(args, packages, args.msvc_version, "10.0.19041", "14.29.16.11", defaultPackages)
    elif args.msvc_version == "17.0":
        setPackageSelectionMSVC16(args, packages, args.msvc_version, "10.0.19041", "14.30.17.0", defaultPackages)

    elif args.msvc_version == "15.4":
        setPackageSelectionMSVC15(args, packages, args.msvc_version, "10.0.16299", "14.11", defaultPackages)
    elif args.msvc_version == "15.5":
        setPackageSelectionMSVC15(args, packages, args.msvc_version, "10.0.16299", "14.12", defaultPackages)
    elif args.msvc_version == "15.6":
        setPackageSelectionMSVC15(args, packages, args.msvc_version, "10.0.16299", "14.13", defaultPackages)
    elif args.msvc_version == "15.7":
        setPackageSelectionMSVC15(args, packages, args.msvc_version, "10.0.17134", "14.14", defaultPackages)
    elif args.msvc_version == "15.8":
        setPackageSelectionMSVC15(args, packages, args.msvc_version, "10.0.17134", "14.15", defaultPackages)
    elif args.msvc_version == "15.9":
        setPackageSelectionMSVC15(args, packages, args.msvc_version, "10.0.17763", "14.16", defaultPackages)
    elif args.msvc_version != None:
        print("Unsupported MSVC toolchain version " + args.msvc_version)
        sys.exit(1)

    if len(args.package) == 0:
        args.package = defaultPackages

    if args.sdk_version != None:
        for key in packages:
            if key.startswith("win10sdk") or key.startswith("win11sdk"):
                base = key[0:8]
                sdkname = base + "_" + args.sdk_version
                if key == sdkname:
                    args.package.append(key)
                else:
                    args.ignore.append(key)
        p = packages[key][0]

def lowercaseIgnores(args):
    ignore = []
    if args.ignore != None:
        for i in args.ignore:
            ignore.append(i.lower())
    args.ignore = ignore

def getManifest(args):
    if args.manifest == None:
        url = "https://aka.ms/vs/%s/%s/channel" % (args.major, args.type)
        print("Fetching %s" % (url))
        manifest = simplejson.loads(six.moves.urllib.request.urlopen(url).read())
        print("Got toplevel manifest for %s" % (manifest["info"]["productDisplayVersion"]))
        for item in manifest["channelItems"]:
            if "type" in item and item["type"] == "Manifest":
                args.manifest = item["payloads"][0]["url"]
        if args.manifest == None:
            print("Unable to find an intaller manifest!")
            sys.exit(1)

    if not args.manifest.startswith("http"):
        args.manifest = "file:" + args.manifest

    manifestdata = six.moves.urllib.request.urlopen(args.manifest).read()
    manifest = simplejson.loads(manifestdata)
    print("Loaded installer manifest for %s" % (manifest["info"]["productDisplayVersion"]))

    if args.save_manifest:
        filename = "%s.manifest" % (manifest["info"]["productDisplayVersion"])
        if os.path.isfile(filename):
            oldfile = open(filename, "rb").read()
            if oldfile != manifestdata:
                print("Old saved manifest in \"%s\" differs from newly downloaded one, not overwriting!" % (filename))
            else:
                print("Old saved manifest in \"%s\" is still current" % (filename))
        else:
            f = open(filename, "wb")
            f.write(manifestdata)
            f.close()
            print("Saved installer manifest to \"%s\"" % (filename))

    return manifest

def prioritizePackage(a, b):
    if "chip" in a and "chip" in b:
        ax64 = a["chip"].lower() == "x64"
        bx64 = b["chip"].lower() == "x64"
        if ax64 and not bx64:
            return -1
        elif bx64 and not ax64:
            return 1
    if "language" in a and "language" in b:
        aeng = a["language"].lower().startswith("en-")
        beng = b["language"].lower().startswith("en-")
        if aeng and not beng:
            return -1
        if beng and not aeng:
            return 1
    return 0

def getPackages(manifest):
    packages = {}
    for p in manifest["packages"]:
        id = p["id"].lower()
        if not id in packages:
            packages[id] = []
        packages[id].append(p)
    for key in packages:
        packages[key] = sorted(packages[key], key=functools.cmp_to_key(prioritizePackage))
    return packages

def listPackageType(packages, type):
    if type != None:
        type = type.lower()
    ids = []
    for key in packages:
        p = packages[key][0]
        if type == None:
            ids.append(p["id"])
        elif "type" in p and p["type"].lower() == type:
            ids.append(p["id"])
    for id in sorted(ids):
        print(id)

def findPackage(packages, id, chip, warn=True):
    origid = id
    id = id.lower()
    candidates = None
    if not id in packages:
        if warn:
            print("WARNING: %s not found" % (origid))
        return None
    candidates = packages[id]
    if chip != None:
        chip = chip.lower()
        for a in candidates:
            if "chip" in a and a["chip"].lower() == chip:
                return a
    return candidates[0]

def printDepends(packages, target, deptype, chip, indent, args):
    chipstr = ""
    if chip != None:
        chipstr = " (" + chip + ")"
    deptypestr = ""
    if deptype != "":
        deptypestr = " (" + deptype + ")"
    ignorestr = ""
    ignore = False
    if target.lower() in args.ignore:
        ignorestr = " (Ignored)"
        ignore = True
    print(indent + target + chipstr + deptypestr + ignorestr)
    if deptype == "Optional" and not args.include_optional:
        return
    if deptype == "Recommended" and args.skip_recommended:
        return
    if ignore:
        return
    p = findPackage(packages, target, chip)
    if p == None:
        return
    if "dependencies" in p:
        deps = p["dependencies"]
        for key in deps:
            dep = deps[key]
            type = ""
            if "type" in dep:
                type = dep["type"]
            chip = None
            if "chip" in dep:
                chip = dep["chip"]
            printDepends(packages, key, type, chip, indent + "  ", args)

def printReverseDepends(packages, target, deptype, indent, args):
    deptypestr = ""
    if deptype != "":
        deptypestr = " (" + deptype + ")"
    print(indent + target + deptypestr)
    if deptype == "Optional" and not args.include_optional:
        return
    if deptype == "Recommended" and args.skip_recommended:
        return
    target = target.lower()
    for key in packages:
        p = packages[key][0]
        if "dependencies" in p:
            deps = p["dependencies"]
            for k in deps:
                if k.lower() != target:
                    continue
                dep = deps[k]
                type = ""
                if "type" in dep:
                    type = dep["type"]
                printReverseDepends(packages, p["id"], type, indent + "  ", args)

def getPackageKey(p):
    packagekey = p["id"]
    if "version" in p:
        packagekey = packagekey + "-" + p["version"]
    if "chip" in p:
        packagekey = packagekey + "-" + p["chip"]
    return packagekey

def aggregateDepends(packages, included, target, chip, args):
    if target.lower() in args.ignore:
        return []
    p = findPackage(packages, target, chip)
    if p == None:
        return []
    packagekey = getPackageKey(p)
    if packagekey in included:
        return []
    ret = [p]
    included[packagekey] = True
    if "dependencies" in p:
        deps = p["dependencies"]
        for key in deps:
            dep = deps[key]
            if "type" in dep:
                deptype = dep["type"]
                if deptype == "Optional" and not args.include_optional:
                    continue
                if deptype == "Recommended" and args.skip_recommended:
                    continue
            chip = None
            if "chip" in dep:
                chip = dep["chip"]
            ret.extend(aggregateDepends(packages, included, key, chip, args))
    return ret

def getSelectedPackages(packages, args):
    ret = []
    included = {}
    for i in args.package:
        ret.extend(aggregateDepends(packages, included, i, None, args))
    return ret

def sumInstalledSize(l):
    sum = 0
    for p in l:
        if "installSizes" in p:
            sizes = p["installSizes"]
            for location in sizes:
                sum = sum + sizes[location]
    return sum

def sumDownloadSize(l):
    sum = 0
    for p in l:
        if "payloads" in p:
            for payload in p["payloads"]:
                if "size" in payload:
                    sum = sum + payload["size"]
    return sum

def formatSize(s):
    if s > 900*1024*1024:
        return "%.1f GB" % (s/(1024*1024*1024))
    if s > 900*1024:
        return "%.1f MB" % (s/(1024*1024))
    if s > 1024:
        return "%.1f KB" % (s/1024)
    return "%d bytes" % (s)

def printPackageList(l):
    for p in sorted(l, key=lambda p: p["id"]):
        s = p["id"]
        if "type" in p:
            s = s + " (" + p["type"] + ")"
        if "chip" in p:
            s = s + " (" + p["chip"] + ")"
        if "language" in p:
            s = s + " (" + p["language"] + ")"
        s = s + " " + formatSize(sumInstalledSize([p]))
        print(s)

def makedirs(dir):
    try:
        os.makedirs(dir)
    except OSError:
        pass

def sha256File(file):
    sha256Hash = hashlib.sha256()
    with open(file, "rb") as f:
        for byteBlock in iter(lambda: f.read(4096), b""):
                sha256Hash.update(byteBlock)
        return sha256Hash.hexdigest()

def getPayloadName(payload):
    name = payload["fileName"]
    if "\\" in name:
        name = name.split("\\")[-1]
    if "/" in name:
        name = name.split("/")[-1]
    return name

def downloadPackages(selected, cache, allowHashMismatch = False):
    pool = multiprocessing.Pool(5)
    tasks = []
    makedirs(cache)
    for p in selected:
        if not "payloads" in p:
            continue
        dir = os.path.join(cache, getPackageKey(p))
        makedirs(dir)
        for payload in p["payloads"]:
            name = getPayloadName(payload)
            destname = os.path.join(dir, name)
            fileid = os.path.join(getPackageKey(p), name)
            args = (payload, destname, fileid, allowHashMismatch)
            tasks.append(pool.apply_async(_downloadPayload, args))

    downloaded = sum(task.get() for task in tasks)
    pool.close()
    print("Downloaded %s in total" % (formatSize(downloaded)))

def _downloadPayload(payload, destname, fileid, allowHashMismatch):
    attempts = 5
    for attempt in range(attempts):
        try:
            if os.access(destname, os.F_OK):
                if "sha256" in payload:
                    if sha256File(destname).lower() != payload["sha256"].lower():
                        six.print_("Incorrect existing file %s, removing" % (fileid), flush=True)
                        os.remove(destname)
                    else:
                        six.print_("Using existing file %s" % (fileid), flush=True)
                        return 0
                else:
                    return 0
            size = 0
            if "size" in payload:
                size = payload["size"]
            six.print_("Downloading %s (%s)" % (fileid, formatSize(size)), flush=True)
            six.moves.urllib.request.urlretrieve(payload["url"], destname)
            if "sha256" in payload:
                if sha256File(destname).lower() != payload["sha256"].lower():
                    if allowHashMismatch:
                        six.print_("WARNING: Incorrect hash for downloaded file %s" % (fileid), flush=True)
                    else:
                        raise Exception("Incorrect hash for downloaded file %s, aborting" % fileid)
            return size
        except Exception as e:
            if attempt == attempts - 1:
                raise
            six.print_("%s: %s" % (type(e).__name__, e), flush=True)

def mergeTrees(src, dest):
    if not os.path.isdir(src):
        return
    if not os.path.isdir(dest):
        shutil.move(src, dest)
        return
    names = os.listdir(src)
    destnames = {}
    for n in os.listdir(dest):
        destnames[n.lower()] = n
    for n in names:
        srcname = os.path.join(src, n)
        destname = os.path.join(dest, n)
        if os.path.isdir(srcname):
            if os.path.isdir(destname):
                mergeTrees(srcname, destname)
            elif n.lower() in destnames:
                mergeTrees(srcname, os.path.join(dest, destnames[n.lower()]))
            else:
                shutil.move(srcname, destname)
        else:
            shutil.move(srcname, destname)

def unzipFiltered(zip, dest):
    tmp = os.path.join(dest, "extract")
    for f in zip.infolist():
        name = six.moves.urllib.parse.unquote(f.filename)
        if "/" in name:
            sep = name.rfind("/")
            dir = os.path.join(dest, name[0:sep])
            makedirs(dir)
        extracted = zip.extract(f, tmp)
        shutil.move(extracted, os.path.join(dest, name))
    shutil.rmtree(tmp)

def unpackVsix(file, dest, listing):
    temp = os.path.join(dest, "vsix")
    makedirs(temp)
    with zipfile.ZipFile(file, 'r') as zip:
        unzipFiltered(zip, temp)
        with open(listing, "w") as f:
            for n in zip.namelist():
                f.write(n + "\n")
    contents = os.path.join(temp, "Contents")
    if os.access(contents, os.F_OK):
        mergeTrees(contents, dest)
    shutil.rmtree(temp)

def unpackWin10SDK(src, payloads, dest):
    # We could try to unpack only the MSIs we need here.
    # Note, this extracts some files into Program Files/..., and some
    # files directly in the root unpack directory. The files we need
    # are under Program Files/... though.
    for payload in payloads:
        name = getPayloadName(payload)
        if name.endswith(".msi"):
            print("Extracting " + name)
            srcfile = os.path.join(src, name)
            log = open(os.path.join(dest, "WinSDK-" + getPayloadName(payload) + "-listing.txt"), "w")
            subprocess.check_call(["msiextract", "-C", dest, srcfile], stdout=log)
            log.close()

def extractPackages(selected, cache, dest):
    makedirs(dest)
    for p in selected:
        type = p["type"]
        dir = os.path.join(cache, getPackageKey(p))
        if type == "Component" or type == "Workload" or type == "Group":
            continue
        if type == "Vsix":
            print("Unpacking " + p["id"])
            for payload in p["payloads"]:
                unpackVsix(os.path.join(dir, getPayloadName(payload)), dest, os.path.join(dest, getPackageKey(p) + "-listing.txt"))
        elif p["id"].startswith("Win10SDK") or p["id"].startswith("Win11SDK"):
            print("Unpacking " + p["id"])
            unpackWin10SDK(dir, p["payloads"], dest)
        else:
            print("Skipping unpacking of " + p["id"] + " of type " + type)

def moveVCSDK(unpack, dest):
    # Move the VC and Program Files\Windows Kits\10 directories
    # out from the unpack directory, allowing the rest of unpacked
    # files to be removed.
    makedirs(os.path.join(dest, "kits"))
    mergeTrees(os.path.join(unpack, "VC"), os.path.join(dest, "VC"))
    mergeTrees(os.path.join(unpack, "Program Files", "Windows Kits", "10"), os.path.join(dest, "kits", "10"))
    # The DIA SDK isn't necessary for normal use, but can be used when e.g.
    # compiling LLVM.
    mergeTrees(os.path.join(unpack, "DIA SDK"), os.path.join(dest, "DIA SDK"))

if __name__ == "__main__":
    parser = getArgsParser()
    args = parser.parse_args()
    lowercaseIgnores(args)

    socket.setdefaulttimeout(15)

    packages = getPackages(getManifest(args))

    if args.print_version:
        sys.exit(0)

    if not args.accept_license:
        response = six.moves.input("Do you accept the license at " + findPackage(packages, "Microsoft.VisualStudio.Product.BuildTools", None)["localizedResources"][0]["license"] + " (yes/no)? ")
        while response != "yes" and response != "no":
            response = six.moves.input("Do you accept the license? Answer \"yes\" or \"no\": ")
        if response == "no":
            sys.exit(0)

    setPackageSelection(args, packages)

    if args.list_components or args.list_workloads or args.list_packages:
        if args.list_components:
            listPackageType(packages, "Component")
        if args.list_workloads:
            listPackageType(packages, "Workload")
        if args.list_packages:
            listPackageType(packages, None)
        sys.exit(0)

    if args.print_deps_tree:
        for i in args.package:
            printDepends(packages, i, "", None, "", args)
        sys.exit(0)

    if args.print_reverse_deps:
        for i in args.package:
            printReverseDepends(packages, i, "", "", args)
        sys.exit(0)

    selected = getSelectedPackages(packages, args)

    if args.print_selection:
        printPackageList(selected)

    print("Selected %d packages, for a total download size of %s, install size of %s" % (len(selected), formatSize(sumDownloadSize(selected)), formatSize(sumInstalledSize(selected))))

    if args.print_selection:
        sys.exit(0)

    tempcache = None
    if args.cache != None:
        cache = os.path.abspath(args.cache)
    else:
        cache = tempfile.mkdtemp(prefix="vsinstall-")
        tempcache = cache

    if not args.only_download and args.dest == None:
        print("No destination directory set!")
        sys.exit(1)

    try:
        downloadPackages(selected, cache, allowHashMismatch=args.only_download)
        if args.only_download:
            sys.exit(0)

        dest = os.path.abspath(args.dest)

        if args.only_unpack:
            unpack = dest
        else:
            unpack = os.path.join(dest, "unpack")

        extractPackages(selected, cache, unpack)

        if not args.only_unpack:
            moveVCSDK(unpack, dest)
            if not args.keep_unpack:
                shutil.rmtree(unpack)
    finally:
        if tempcache != None:
            shutil.rmtree(tempcache)
