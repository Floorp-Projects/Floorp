def parse_checksums_file(checksums):
    """Parses checksums files that the build system generates and uploads:
        https://hg.mozilla.org/mozilla-central/file/default/build/checksums.py"""
    fileInfo = {}
    for line in checksums.splitlines():
        hash_, type_, size, file_ = line.split(None, 3)
        size = int(size)
        if size < 0:
            raise ValueError("Found negative value (%d) for size." % size)
        if file_ not in fileInfo:
            fileInfo[file_] = {"hashes": {}}
        # If the file already exists, make sure that the size matches the
        # previous entry.
        elif fileInfo[file_]['size'] != size:
            raise ValueError("Found different sizes for same file %s (%s and %s)" % (file_, fileInfo[file_]['size'], size))
        # Same goes for the hash.
        elif type_ in fileInfo[file_]['hashes'] and fileInfo[file_]['hashes'][type_] != hash_:
            raise ValueError("Found different %s hashes for same file %s (%s and %s)" % (type_, file_, fileInfo[file_]['hashes'][type_], hash_))
        fileInfo[file_]['size'] = size
        fileInfo[file_]['hashes'][type_] = hash_
    return fileInfo
