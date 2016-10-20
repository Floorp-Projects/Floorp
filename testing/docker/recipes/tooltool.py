#!/usr/bin/env python

# tooltool is a lookaside cache implemented in Python
# Copyright (C) 2011 John H. Ford <john@johnford.info>
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation version 2
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
# 02110-1301, USA.

# A manifest file specifies files in that directory that are stored
# elsewhere. This file should only list files in the same directory
# in which the manifest file resides and it should be called
# 'manifest.tt'

import hashlib
import httplib
import json
import logging
import optparse
import os
import shutil
import sys
import tarfile
import tempfile
import threading
import time
import urllib2
import urlparse
import zipfile

from subprocess import PIPE
from subprocess import Popen

__version__ = '1'

DEFAULT_MANIFEST_NAME = 'manifest.tt'
TOOLTOOL_PACKAGE_SUFFIX = '.TOOLTOOL-PACKAGE'


log = logging.getLogger(__name__)


class FileRecordJSONEncoderException(Exception):
    pass


class InvalidManifest(Exception):
    pass


class ExceptionWithFilename(Exception):

    def __init__(self, filename):
        Exception.__init__(self)
        self.filename = filename


class BadFilenameException(ExceptionWithFilename):
    pass


class DigestMismatchException(ExceptionWithFilename):
    pass


class MissingFileException(ExceptionWithFilename):
    pass


class FileRecord(object):

    def __init__(self, filename, size, digest, algorithm, unpack=False,
                 visibility=None, setup=None):
        object.__init__(self)
        if '/' in filename or '\\' in filename:
            log.error(
                "The filename provided contains path information and is, therefore, invalid.")
            raise BadFilenameException(filename=filename)
        self.filename = filename
        self.size = size
        self.digest = digest
        self.algorithm = algorithm
        self.unpack = unpack
        self.visibility = visibility
        self.setup = setup

    def __eq__(self, other):
        if self is other:
            return True
        if self.filename == other.filename and \
           self.size == other.size and \
           self.digest == other.digest and \
           self.algorithm == other.algorithm and \
           self.visibility == other.visibility:
            return True
        else:
            return False

    def __ne__(self, other):
        return not self.__eq__(other)

    def __str__(self):
        return repr(self)

    def __repr__(self):
        return "%s.%s(filename='%s', size=%s, digest='%s', algorithm='%s', visibility=%r)" % (
            __name__, self.__class__.__name__, self.filename, self.size,
            self.digest, self.algorithm, self.visibility)

    def present(self):
        # Doesn't check validity
        return os.path.exists(self.filename)

    def validate_size(self):
        if self.present():
            return self.size == os.path.getsize(self.filename)
        else:
            log.debug(
                "trying to validate size on a missing file, %s", self.filename)
            raise MissingFileException(filename=self.filename)

    def validate_digest(self):
        if self.present():
            with open(self.filename, 'rb') as f:
                return self.digest == digest_file(f, self.algorithm)
        else:
            log.debug(
                "trying to validate digest on a missing file, %s', self.filename")
            raise MissingFileException(filename=self.filename)

    def validate(self):
        if self.validate_size():
            if self.validate_digest():
                return True
        return False

    def describe(self):
        if self.present() and self.validate():
            return "'%s' is present and valid" % self.filename
        elif self.present():
            return "'%s' is present and invalid" % self.filename
        else:
            return "'%s' is absent" % self.filename


def create_file_record(filename, algorithm):
    fo = open(filename, 'rb')
    stored_filename = os.path.split(filename)[1]
    fr = FileRecord(stored_filename, os.path.getsize(
        filename), digest_file(fo, algorithm), algorithm)
    fo.close()
    return fr


class FileRecordJSONEncoder(json.JSONEncoder):

    def encode_file_record(self, obj):
        if not issubclass(type(obj), FileRecord):
            err = "FileRecordJSONEncoder is only for FileRecord and lists of FileRecords, " \
                  "not %s" % obj.__class__.__name__
            log.warn(err)
            raise FileRecordJSONEncoderException(err)
        else:
            rv = {
                'filename': obj.filename,
                'size': obj.size,
                'algorithm': obj.algorithm,
                'digest': obj.digest,
            }
            if obj.unpack:
                rv['unpack'] = True
            if obj.visibility is not None:
                rv['visibility'] = obj.visibility
            if obj.setup:
                rv['setup'] = obj.setup
            return rv

    def default(self, f):
        if issubclass(type(f), list):
            record_list = []
            for i in f:
                record_list.append(self.encode_file_record(i))
            return record_list
        else:
            return self.encode_file_record(f)


class FileRecordJSONDecoder(json.JSONDecoder):

    """I help the json module materialize a FileRecord from
    a JSON file.  I understand FileRecords and lists of
    FileRecords.  I ignore things that I don't expect for now"""
    # TODO: make this more explicit in what it's looking for
    # and error out on unexpected things

    def process_file_records(self, obj):
        if isinstance(obj, list):
            record_list = []
            for i in obj:
                record = self.process_file_records(i)
                if issubclass(type(record), FileRecord):
                    record_list.append(record)
            return record_list
        required_fields = [
            'filename',
            'size',
            'algorithm',
            'digest',
        ]
        if isinstance(obj, dict):
            missing = False
            for req in required_fields:
                if req not in obj:
                    missing = True
                    break

            if not missing:
                unpack = obj.get('unpack', False)
                visibility = obj.get('visibility', None)
                setup = obj.get('setup')
                rv = FileRecord(
                    obj['filename'], obj['size'], obj['digest'], obj['algorithm'],
                    unpack, visibility, setup)
                log.debug("materialized %s" % rv)
                return rv
        return obj

    def decode(self, s):
        decoded = json.JSONDecoder.decode(self, s)
        rv = self.process_file_records(decoded)
        return rv


class Manifest(object):

    valid_formats = ('json',)

    def __init__(self, file_records=None):
        self.file_records = file_records or []

    def __eq__(self, other):
        if self is other:
            return True
        if len(self.file_records) != len(other.file_records):
            log.debug('Manifests differ in number of files')
            return False
        # sort the file records by filename before comparing
        mine = sorted((fr.filename, fr) for fr in self.file_records)
        theirs = sorted((fr.filename, fr) for fr in other.file_records)
        return mine == theirs

    def __ne__(self, other):
        return not self.__eq__(other)

    def __deepcopy__(self, memo):
        # This is required for a deep copy
        return Manifest(self.file_records[:])

    def __copy__(self):
        return Manifest(self.file_records)

    def copy(self):
        return Manifest(self.file_records[:])

    def present(self):
        return all(i.present() for i in self.file_records)

    def validate_sizes(self):
        return all(i.validate_size() for i in self.file_records)

    def validate_digests(self):
        return all(i.validate_digest() for i in self.file_records)

    def validate(self):
        return all(i.validate() for i in self.file_records)

    def load(self, data_file, fmt='json'):
        assert fmt in self.valid_formats
        if fmt == 'json':
            try:
                self.file_records.extend(
                    json.load(data_file, cls=FileRecordJSONDecoder))
            except ValueError:
                raise InvalidManifest("trying to read invalid manifest file")

    def loads(self, data_string, fmt='json'):
        assert fmt in self.valid_formats
        if fmt == 'json':
            try:
                self.file_records.extend(
                    json.loads(data_string, cls=FileRecordJSONDecoder))
            except ValueError:
                raise InvalidManifest("trying to read invalid manifest file")

    def dump(self, output_file, fmt='json'):
        assert fmt in self.valid_formats
        if fmt == 'json':
            rv = json.dump(
                self.file_records, output_file, indent=0, cls=FileRecordJSONEncoder,
                separators=(',', ': '))
            print >> output_file, ''
            return rv

    def dumps(self, fmt='json'):
        assert fmt in self.valid_formats
        if fmt == 'json':
            return json.dumps(self.file_records, cls=FileRecordJSONEncoder)


def digest_file(f, a):
    """I take a file like object 'f' and return a hex-string containing
    of the result of the algorithm 'a' applied to 'f'."""
    h = hashlib.new(a)
    chunk_size = 1024 * 10
    data = f.read(chunk_size)
    while data:
        h.update(data)
        data = f.read(chunk_size)
    name = repr(f.name) if hasattr(f, 'name') else 'a file'
    log.debug('hashed %s with %s to be %s', name, a, h.hexdigest())
    return h.hexdigest()


def execute(cmd):
    """Execute CMD, logging its stdout at the info level"""
    process = Popen(cmd, shell=True, stdout=PIPE)
    while True:
        line = process.stdout.readline()
        if not line:
            break
        log.info(line.replace('\n', ' '))
    return process.wait() == 0


def open_manifest(manifest_file):
    """I know how to take a filename and load it into a Manifest object"""
    if os.path.exists(manifest_file):
        manifest = Manifest()
        with open(manifest_file) as f:
            manifest.load(f)
            log.debug("loaded manifest from file '%s'" % manifest_file)
        return manifest
    else:
        log.debug("tried to load absent file '%s' as manifest" % manifest_file)
        raise InvalidManifest(
            "manifest file '%s' does not exist" % manifest_file)


def list_manifest(manifest_file):
    """I know how print all the files in a location"""
    try:
        manifest = open_manifest(manifest_file)
    except InvalidManifest as e:
        log.error("failed to load manifest file at '%s': %s" % (
            manifest_file,
            str(e),
        ))
        return False
    for f in manifest.file_records:
        print "%s\t%s\t%s" % ("P" if f.present() else "-",
                              "V" if f.present() and f.validate() else "-",
                              f.filename)
    return True


def validate_manifest(manifest_file):
    """I validate that all files in a manifest are present and valid but
    don't fetch or delete them if they aren't"""
    try:
        manifest = open_manifest(manifest_file)
    except InvalidManifest as e:
        log.error("failed to load manifest file at '%s': %s" % (
            manifest_file,
            str(e),
        ))
        return False
    invalid_files = []
    absent_files = []
    for f in manifest.file_records:
        if not f.present():
            absent_files.append(f)
        else:
            if not f.validate():
                invalid_files.append(f)
    if len(invalid_files + absent_files) == 0:
        return True
    else:
        return False


def add_files(manifest_file, algorithm, filenames, visibility, unpack):
    # returns True if all files successfully added, False if not
    # and doesn't catch library Exceptions.  If any files are already
    # tracked in the manifest, return will be False because they weren't
    # added
    all_files_added = True
    # Create a old_manifest object to add to
    if os.path.exists(manifest_file):
        old_manifest = open_manifest(manifest_file)
    else:
        old_manifest = Manifest()
        log.debug("creating a new manifest file")
    new_manifest = Manifest()  # use a different manifest for the output
    for filename in filenames:
        log.debug("adding %s" % filename)
        path, name = os.path.split(filename)
        new_fr = create_file_record(filename, algorithm)
        new_fr.visibility = visibility
        new_fr.unpack = unpack
        log.debug("appending a new file record to manifest file")
        add = True
        for fr in old_manifest.file_records:
            log.debug("manifest file has '%s'" % "', ".join(
                [x.filename for x in old_manifest.file_records]))
            if new_fr == fr:
                log.info("file already in old_manifest")
                add = False
            elif filename == fr.filename:
                log.error("manifest already contains a different file named %s" % filename)
                add = False
        if add:
            new_manifest.file_records.append(new_fr)
            log.debug("added '%s' to manifest" % filename)
        else:
            all_files_added = False
    # copy any files in the old manifest that aren't in the new one
    new_filenames = set(fr.filename for fr in new_manifest.file_records)
    for old_fr in old_manifest.file_records:
        if old_fr.filename not in new_filenames:
            new_manifest.file_records.append(old_fr)
    with open(manifest_file, 'wb') as output:
        new_manifest.dump(output, fmt='json')
    return all_files_added


def touch(f):
    """Used to modify mtime in cached files;
    mtime is used by the purge command"""
    try:
        os.utime(f, None)
    except OSError:
        log.warn('impossible to update utime of file %s' % f)


def fetch_file(base_urls, file_record, grabchunk=1024 * 4, auth_file=None, region=None):
    # A file which is requested to be fetched that exists locally will be
    # overwritten by this function
    fd, temp_path = tempfile.mkstemp(dir=os.getcwd())
    os.close(fd)
    fetched_path = None
    for base_url in base_urls:
        # Generate the URL for the file on the server side
        url = urlparse.urljoin(base_url,
                               '%s/%s' % (file_record.algorithm, file_record.digest))
        if region is not None:
            url += '?region=' + region

        log.info("Attempting to fetch from '%s'..." % base_url)

        # Well, the file doesn't exist locally.  Let's fetch it.
        try:
            req = urllib2.Request(url)
            _authorize(req, auth_file)
            f = urllib2.urlopen(req)
            log.debug("opened %s for reading" % url)
            with open(temp_path, 'wb') as out:
                k = True
                size = 0
                while k:
                    # TODO: print statistics as file transfers happen both for info and to stop
                    # buildbot timeouts
                    indata = f.read(grabchunk)
                    out.write(indata)
                    size += len(indata)
                    if indata == '':
                        k = False
                log.info("File %s fetched from %s as %s" %
                         (file_record.filename, base_url, temp_path))
                fetched_path = temp_path
                break
        except (urllib2.URLError, urllib2.HTTPError, ValueError) as e:
            log.info("...failed to fetch '%s' from %s" %
                     (file_record.filename, base_url))
            log.debug("%s" % e)
        except IOError:  # pragma: no cover
            log.info("failed to write to temporary file for '%s'" %
                     file_record.filename, exc_info=True)

    # cleanup temp file in case of issues
    if fetched_path:
        return os.path.split(fetched_path)[1]
    else:
        try:
            os.remove(temp_path)
        except OSError:  # pragma: no cover
            pass
        return None


def clean_path(dirname):
    """Remove a subtree if is exists. Helper for unpack_file()."""
    if os.path.exists(dirname):
        log.info('rm tree: %s' % dirname)
        shutil.rmtree(dirname)


def unpack_file(filename, setup=None):
    """Untar `filename`, assuming it is uncompressed or compressed with bzip2,
    xz, gzip, or unzip a zip file. The file is assumed to contain a single
    directory with a name matching the base of the given filename.
    Xz support is handled by shelling out to 'tar'."""
    if tarfile.is_tarfile(filename):
        tar_file, zip_ext = os.path.splitext(filename)
        base_file, tar_ext = os.path.splitext(tar_file)
        clean_path(base_file)
        log.info('untarring "%s"' % filename)
        tar = tarfile.open(filename)
        tar.extractall()
        tar.close()
    elif filename.endswith('.tar.xz'):
        base_file = filename.replace('.tar.xz', '')
        clean_path(base_file)
        log.info('untarring "%s"' % filename)
        if not execute('tar -Jxf %s 2>&1' % filename):
            return False
    elif zipfile.is_zipfile(filename):
        base_file = filename.replace('.zip', '')
        clean_path(base_file)
        log.info('unzipping "%s"' % filename)
        z = zipfile.ZipFile(filename)
        z.extractall()
        z.close()
    else:
        log.error("Unknown archive extension for filename '%s'" % filename)
        return False

    if setup and not execute(os.path.join(base_file, setup)):
        return False
    return True


def fetch_files(manifest_file, base_urls, filenames=[], cache_folder=None,
                auth_file=None, region=None):
    # Lets load the manifest file
    try:
        manifest = open_manifest(manifest_file)
    except InvalidManifest as e:
        log.error("failed to load manifest file at '%s': %s" % (
            manifest_file,
            str(e),
        ))
        return False

    # we want to track files already in current working directory AND valid
    # we will not need to fetch these
    present_files = []

    # We want to track files that fail to be fetched as well as
    # files that are fetched
    failed_files = []
    fetched_files = []

    # Files that we want to unpack.
    unpack_files = []

    # Setup for unpacked files.
    setup_files = {}

    # Lets go through the manifest and fetch the files that we want
    for f in manifest.file_records:
        # case 1: files are already present
        if f.present():
            if f.validate():
                present_files.append(f.filename)
                if f.unpack:
                    unpack_files.append(f.filename)
            else:
                # we have an invalid file here, better to cleanup!
                # this invalid file needs to be replaced with a good one
                # from the local cash or fetched from a tooltool server
                log.info("File %s is present locally but it is invalid, so I will remove it "
                         "and try to fetch it" % f.filename)
                os.remove(os.path.join(os.getcwd(), f.filename))

        # check if file is already in cache
        if cache_folder and f.filename not in present_files:
            try:
                shutil.copy(os.path.join(cache_folder, f.digest),
                            os.path.join(os.getcwd(), f.filename))
                log.info("File %s retrieved from local cache %s" %
                         (f.filename, cache_folder))
                touch(os.path.join(cache_folder, f.digest))

                filerecord_for_validation = FileRecord(
                    f.filename, f.size, f.digest, f.algorithm)
                if filerecord_for_validation.validate():
                    present_files.append(f.filename)
                    if f.unpack:
                        unpack_files.append(f.filename)
                else:
                    # the file copied from the cache is invalid, better to
                    # clean up the cache version itself as well
                    log.warn("File %s retrieved from cache is invalid! I am deleting it from the "
                             "cache as well" % f.filename)
                    os.remove(os.path.join(os.getcwd(), f.filename))
                    os.remove(os.path.join(cache_folder, f.digest))
            except IOError:
                log.info("File %s not present in local cache folder %s" %
                         (f.filename, cache_folder))

        # now I will try to fetch all files which are not already present and
        # valid, appending a suffix to avoid race conditions
        temp_file_name = None
        # 'filenames' is the list of filenames to be managed, if this variable
        # is a non empty list it can be used to filter if filename is in
        # present_files, it means that I have it already because it was already
        # either in the working dir or in the cache
        if (f.filename in filenames or len(filenames) == 0) and f.filename not in present_files:
            log.debug("fetching %s" % f.filename)
            temp_file_name = fetch_file(base_urls, f, auth_file=auth_file, region=region)
            if temp_file_name:
                fetched_files.append((f, temp_file_name))
            else:
                failed_files.append(f.filename)
        else:
            log.debug("skipping %s" % f.filename)

        if f.setup:
            if f.unpack:
                setup_files[f.filename] = f.setup
            else:
                log.error("'setup' requires 'unpack' being set for %s" % f.filename)
                failed_files.append(f.filename)

    # lets ensure that fetched files match what the manifest specified
    for localfile, temp_file_name in fetched_files:
        # since I downloaded to a temp file, I need to perform all validations on the temp file
        # this is why filerecord_for_validation is created

        filerecord_for_validation = FileRecord(
            temp_file_name, localfile.size, localfile.digest, localfile.algorithm)

        if filerecord_for_validation.validate():
            # great!
            # I can rename the temp file
            log.info("File integrity verified, renaming %s to %s" %
                     (temp_file_name, localfile.filename))
            os.rename(os.path.join(os.getcwd(), temp_file_name),
                      os.path.join(os.getcwd(), localfile.filename))

            if localfile.unpack:
                unpack_files.append(localfile.filename)

            # if I am using a cache and a new file has just been retrieved from a
            # remote location, I need to update the cache as well
            if cache_folder:
                log.info("Updating local cache %s..." % cache_folder)
                try:
                    if not os.path.exists(cache_folder):
                        log.info("Creating cache in %s..." % cache_folder)
                        os.makedirs(cache_folder, 0700)
                    shutil.copy(os.path.join(os.getcwd(), localfile.filename),
                                os.path.join(cache_folder, localfile.digest))
                    log.info("Local cache %s updated with %s" % (cache_folder,
                                                                 localfile.filename))
                    touch(os.path.join(cache_folder, localfile.digest))
                except (OSError, IOError):
                    log.warning('Impossible to add file %s to cache folder %s' %
                                (localfile.filename, cache_folder), exc_info=True)
        else:
            failed_files.append(localfile.filename)
            log.error("'%s'" % filerecord_for_validation.describe())
            os.remove(temp_file_name)

    # Unpack files that need to be unpacked.
    for filename in unpack_files:
        if not unpack_file(filename, setup_files.get(filename)):
            failed_files.append(filename)

    # If we failed to fetch or validate a file, we need to fail
    if len(failed_files) > 0:
        log.error("The following files failed: '%s'" %
                  "', ".join(failed_files))
        return False
    return True


def freespace(p):
    "Returns the number of bytes free under directory `p`"
    if sys.platform == 'win32':  # pragma: no cover
        # os.statvfs doesn't work on Windows
        import win32file

        secsPerClus, bytesPerSec, nFreeClus, totClus = win32file.GetDiskFreeSpace(
            p)
        return secsPerClus * bytesPerSec * nFreeClus
    else:
        r = os.statvfs(p)
        return r.f_frsize * r.f_bavail


def purge(folder, gigs):
    """If gigs is non 0, it deletes files in `folder` until `gigs` GB are free,
    starting from older files.  If gigs is 0, a full purge will be performed.
    No recursive deletion of files in subfolder is performed."""

    full_purge = bool(gigs == 0)
    gigs *= 1024 * 1024 * 1024

    if not full_purge and freespace(folder) >= gigs:
        log.info("No need to cleanup")
        return

    files = []
    for f in os.listdir(folder):
        p = os.path.join(folder, f)
        # it deletes files in folder without going into subfolders,
        # assuming the cache has a flat structure
        if not os.path.isfile(p):
            continue
        mtime = os.path.getmtime(p)
        files.append((mtime, p))

    # iterate files sorted by mtime
    for _, f in sorted(files):
        log.info("removing %s to free up space" % f)
        try:
            os.remove(f)
        except OSError:
            log.info("Impossible to remove %s" % f, exc_info=True)
        if not full_purge and freespace(folder) >= gigs:
            break


def _log_api_error(e):
    if hasattr(e, 'hdrs') and e.hdrs['content-type'] == 'application/json':
        json_resp = json.load(e.fp)
        log.error("%s: %s" % (json_resp['error']['name'],
                              json_resp['error']['description']))
    else:
        log.exception("Error making RelengAPI request:")


def _authorize(req, auth_file):
    if auth_file:
        log.debug("using bearer token in %s" % auth_file)
        req.add_unredirected_header('Authorization',
                                    'Bearer %s' % (open(auth_file).read().strip()))


def _send_batch(base_url, auth_file, batch, region):
    url = urlparse.urljoin(base_url, 'upload')
    if region is not None:
        url += "?region=" + region
    req = urllib2.Request(url, json.dumps(batch), {'Content-Type': 'application/json'})
    _authorize(req, auth_file)
    try:
        resp = urllib2.urlopen(req)
    except (urllib2.URLError, urllib2.HTTPError) as e:
        _log_api_error(e)
        return None
    return json.load(resp)['result']


def _s3_upload(filename, file):
    # urllib2 does not support streaming, so we fall back to good old httplib
    url = urlparse.urlparse(file['put_url'])
    cls = httplib.HTTPSConnection if url.scheme == 'https' else httplib.HTTPConnection
    host, port = url.netloc.split(':') if ':' in url.netloc else (url.netloc, 443)
    port = int(port)
    conn = cls(host, port)
    try:
        req_path = "%s?%s" % (url.path, url.query) if url.query else url.path
        conn.request('PUT', req_path, open(filename),
                     {'Content-type': 'application/octet-stream'})
        resp = conn.getresponse()
        resp_body = resp.read()
        conn.close()
        if resp.status != 200:
            raise RuntimeError("Non-200 return from AWS: %s %s\n%s" %
                               (resp.status, resp.reason, resp_body))
    except Exception:
        file['upload_exception'] = sys.exc_info()
        file['upload_ok'] = False
    else:
        file['upload_ok'] = True


def _notify_upload_complete(base_url, auth_file, file):
    req = urllib2.Request(
        urlparse.urljoin(
            base_url,
            'upload/complete/%(algorithm)s/%(digest)s' % file))
    _authorize(req, auth_file)
    try:
        urllib2.urlopen(req)
    except urllib2.HTTPError as e:
        if e.code != 409:
            _log_api_error(e)
            return
        # 409 indicates that the upload URL hasn't expired yet and we
        # should retry after a delay
        to_wait = int(e.headers.get('X-Retry-After', 60))
        log.warning("Waiting %d seconds for upload URLs to expire" % to_wait)
        time.sleep(to_wait)
        _notify_upload_complete(base_url, auth_file, file)
    except Exception:
        log.exception("While notifying server of upload completion:")


def upload(manifest, message, base_urls, auth_file, region):
    try:
        manifest = open_manifest(manifest)
    except InvalidManifest:
        log.exception("failed to load manifest file at '%s'")
        return False

    # verify the manifest, since we'll need the files present to upload
    if not manifest.validate():
        log.error('manifest is invalid')
        return False

    if any(fr.visibility is None for fr in manifest.file_records):
        log.error('All files in a manifest for upload must have a visibility set')

    # convert the manifest to an upload batch
    batch = {
        'message': message,
        'files': {},
    }
    for fr in manifest.file_records:
        batch['files'][fr.filename] = {
            'size': fr.size,
            'digest': fr.digest,
            'algorithm': fr.algorithm,
            'visibility': fr.visibility,
        }

    # make the upload request
    resp = _send_batch(base_urls[0], auth_file, batch, region)
    if not resp:
        return None
    files = resp['files']

    # Upload the files, each in a thread.  This allows us to start all of the
    # uploads before any of the URLs expire.
    threads = {}
    for filename, file in files.iteritems():
        if 'put_url' in file:
            log.info("%s: starting upload" % (filename,))
            thd = threading.Thread(target=_s3_upload,
                                   args=(filename, file))
            thd.daemon = 1
            thd.start()
            threads[filename] = thd
        else:
            log.info("%s: already exists on server" % (filename,))

    # re-join all of those threads as they exit
    success = True
    while threads:
        for filename, thread in threads.items():
            if not thread.is_alive():
                # _s3_upload has annotated file with result information
                file = files[filename]
                thread.join()
                if file['upload_ok']:
                    log.info("%s: uploaded" % filename)
                else:
                    log.error("%s: failed" % filename,
                              exc_info=file['upload_exception'])
                    success = False
                del threads[filename]

    # notify the server that the uploads are completed.  If the notification
    # fails, we don't consider that an error (the server will notice
    # eventually)
    for filename, file in files.iteritems():
        if 'put_url' in file and file['upload_ok']:
            log.info("notifying server of upload completion for %s" % (filename,))
            _notify_upload_complete(base_urls[0], auth_file, file)

    return success


def process_command(options, args):
    """ I know how to take a list of program arguments and
    start doing the right thing with them"""
    cmd = args[0]
    cmd_args = args[1:]
    log.debug("processing '%s' command with args '%s'" %
              (cmd, '", "'.join(cmd_args)))
    log.debug("using options: %s" % options)

    if cmd == 'list':
        return list_manifest(options['manifest'])
    if cmd == 'validate':
        return validate_manifest(options['manifest'])
    elif cmd == 'add':
        return add_files(options['manifest'], options['algorithm'], cmd_args,
                         options['visibility'], options['unpack'])
    elif cmd == 'purge':
        if options['cache_folder']:
            purge(folder=options['cache_folder'], gigs=options['size'])
        else:
            log.critical('please specify the cache folder to be purged')
            return False
    elif cmd == 'fetch':
        return fetch_files(
            options['manifest'],
            options['base_url'],
            cmd_args,
            cache_folder=options['cache_folder'],
            auth_file=options.get("auth_file"),
            region=options.get('region'))
    elif cmd == 'upload':
        if not options.get('message'):
            log.critical('upload command requires a message')
            return False
        return upload(
            options.get('manifest'),
            options.get('message'),
            options.get('base_url'),
            options.get('auth_file'),
            options.get('region'))
    else:
        log.critical('command "%s" is not implemented' % cmd)
        return False


def main(argv, _skip_logging=False):
    # Set up option parsing
    parser = optparse.OptionParser()
    parser.add_option('-q', '--quiet', default=logging.INFO,
                      dest='loglevel', action='store_const', const=logging.ERROR)
    parser.add_option('-v', '--verbose',
                      dest='loglevel', action='store_const', const=logging.DEBUG)
    parser.add_option('-m', '--manifest', default=DEFAULT_MANIFEST_NAME,
                      dest='manifest', action='store',
                      help='specify the manifest file to be operated on')
    parser.add_option('-d', '--algorithm', default='sha512',
                      dest='algorithm', action='store',
                      help='hashing algorithm to use (only sha512 is allowed)')
    parser.add_option('--visibility', default=None,
                      dest='visibility', choices=['internal', 'public'],
                      help='Visibility level of this file; "internal" is for '
                           'files that cannot be distributed out of the company '
                           'but not for secrets; "public" files are available to '
                           'anyone withou trestriction')
    parser.add_option('--unpack', default=False,
                      dest='unpack', action='store_true',
                      help='Request unpacking this file after fetch.'
                           ' This is helpful with tarballs.')
    parser.add_option('-o', '--overwrite', default=False,
                      dest='overwrite', action='store_true',
                      help='UNUSED; present for backward compatibility')
    parser.add_option('--url', dest='base_url', action='append',
                      help='RelengAPI URL ending with /tooltool/; default '
                      'is appropriate for Mozilla')
    parser.add_option('-c', '--cache-folder', dest='cache_folder',
                      help='Local cache folder')
    parser.add_option('-s', '--size',
                      help='free space required (in GB)', dest='size',
                      type='float', default=0.)
    parser.add_option('-r', '--region', help='Preferred AWS region for upload or fetch; '
                      'example: --region=us-west-2')
    parser.add_option('--message',
                      help='The "commit message" for an upload; format with a bug number '
                           'and brief comment',
                      dest='message')
    parser.add_option('--authentication-file',
                      help='Use the RelengAPI token found in the given file to '
                           'authenticate to the RelengAPI server.',
                      dest='auth_file')

    (options_obj, args) = parser.parse_args(argv[1:])

    # default the options list if not provided
    if not options_obj.base_url:
        options_obj.base_url = ['https://api.pub.build.mozilla.org/tooltool/']

    # ensure all URLs have a trailing slash
    def add_slash(url):
        return url if url.endswith('/') else (url + '/')
    options_obj.base_url = [add_slash(u) for u in options_obj.base_url]

    # expand ~ in --authentication-file
    if options_obj.auth_file:
        options_obj.auth_file = os.path.expanduser(options_obj.auth_file)

    # Dictionaries are easier to work with
    options = vars(options_obj)

    log.setLevel(options['loglevel'])

    # Set up logging, for now just to the console
    if not _skip_logging:  # pragma: no cover
        ch = logging.StreamHandler()
        cf = logging.Formatter("%(levelname)s - %(message)s")
        ch.setFormatter(cf)
        log.addHandler(ch)

    if options['algorithm'] != 'sha512':
        parser.error('only --algorithm sha512 is supported')

    if len(args) < 1:
        parser.error('You must specify a command')

    return 0 if process_command(options, args) else 1

if __name__ == "__main__":  # pragma: no cover
    sys.exit(main(sys.argv))
