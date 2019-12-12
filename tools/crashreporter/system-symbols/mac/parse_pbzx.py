#!/usr/bin/env python
# v2 pbzx stream handler
# My personal writeup on the differences here: https://gist.github.com/pudquick/29fcfe09c326a9b96cf5
#
# Pure python reimplementation of .cpio.xz content extraction from pbzx file payload originally here:
# http://www.tonymacx86.com/general-help/135458-pbzx-stream-parser.html
#
# Cleaned up C version (as the basis for my code) here, thanks to Pepijn Bruienne / @bruienne
# https://gist.github.com/bruienne/029494bbcfb358098b41

import struct, sys

def seekread(f, offset=None, length=0, relative=True):
    if (offset != None):
        # offset provided, let's seek
        f.seek(offset, [0,1,2][relative])
    if (length != 0):
        return f.read(length)

def parse_pbzx(pbzx_path):
    section = 0
    xar_out_path = '%s.part%02d.cpio.xz' % (pbzx_path, section)
    f = open(pbzx_path, 'rb')
    # pbzx = f.read()
    # f.close()
    magic = seekread(f,length=4)
    if magic != 'pbzx':
        raise "Error: Not a pbzx file"
    # Read 8 bytes for initial flags
    flags = seekread(f,length=8)
    # Interpret the flags as a 64-bit big-endian unsigned int
    flags = struct.unpack('>Q', flags)[0]
    xar_f = open(xar_out_path, 'wb')
    while (flags & (1 << 24)):
        # Read in more flags
        flags = seekread(f,length=8)
        flags = struct.unpack('>Q', flags)[0]
        # Read in length
        f_length = seekread(f,length=8)
        f_length = struct.unpack('>Q', f_length)[0]
        xzmagic = seekread(f,length=6)
        if xzmagic != '\xfd7zXZ\x00':
            # This isn't xz content, this is actually _raw decompressed cpio_ chunk of 16MB in size...
            # Let's back up ...
            seekread(f,offset=-6,length=0)
            # ... and split it out ...
            f_content = seekread(f,length=f_length)
            section += 1
            decomp_out = '%s.part%02d.cpio' % (pbzx_path, section)
            g = open(decomp_out, 'wb')
            g.write(f_content)
            g.close()
            # Now to start the next section, which should hopefully be .xz (we'll just assume it is ...)
            xar_f.close()
            section += 1
            new_out = '%s.part%02d.cpio.xz' % (pbzx_path, section)
            xar_f = open(new_out, 'wb')
        else:
            f_length -= 6
            # This part needs buffering
            f_content = seekread(f,length=f_length)
            tail = seekread(f,offset=-2,length=2)
            xar_f.write(xzmagic)
            xar_f.write(f_content)
            if tail != 'YZ':
                xar_f.close()
                raise "Error: Footer is not xar file footer"
    try:
        f.close()
        xar_f.close()
    except:
        pass

def main():
    parse_pbzx(sys.argv[1])

if __name__ == '__main__':
    main()
