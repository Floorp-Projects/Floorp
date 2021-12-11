#!/usr/bin/env python2

from __future__ import print_function
import sys

def main():
    if len(sys.argv) < 2:
        raise Exception('Specify either "asan", "msan", "sancov" or "ubsan" as argument.')

    sanitizer = sys.argv[1]
    if sanitizer == "ubsan":
        if len(sys.argv) < 3:
            raise Exception('ubsan requires another argument.')
        print('-fsanitize='+sys.argv[2]+' -fno-sanitize-recover=undefined ', end='')
        return
    if sanitizer == "asan":
        print('-fsanitize=address -fsanitize-address-use-after-scope ', end='')
        print('-fno-omit-frame-pointer -fno-optimize-sibling-calls ', end='')
        return
    if sanitizer == "msan":
        print('-fsanitize=memory -fsanitize-memory-track-origins ', end='')
        print('-fno-omit-frame-pointer -fno-optimize-sibling-calls ', end='')
        return
    if sanitizer == "sancov":
        if len(sys.argv) < 3:
            raise Exception('sancov requires another argument (edge|bb|func).')
        print('-fsanitize-coverage='+sys.argv[2]+' ', end='')
        return

    raise Exception('Specify either "asan", "msan", "sancov" or "ubsan" as argument.')

if __name__ == '__main__':
    main()
