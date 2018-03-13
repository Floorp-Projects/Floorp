#!/usr/bin/python
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# This script processes a `refcount' log, and finds out if any object leaked.
# It simply goes through the log, finds `AddRef' or `Ctor' lines, and then
# sees if they `Release' or `Dtor'. If not, it reports them as leaks.
# Please see README file in the same directory.


import sys

def print_output(allocation, obj_to_class):
    '''Formats and prints output.'''
    items = []
    for obj, count, in allocation.iteritems():
        # Adding items to a list, so we can sort them.
        items.append((obj, count))
    # Sorting by count.
    items.sort(key=lambda item: item[1])

    for obj, count, in items:
        print "{obj} ({count}) @ {class_name}".format(obj=obj,
                                                      count=count,
                                                      class_name=obj_to_class[obj])

def process_log(log_lines):
    '''Process through the log lines, and print out the result.

    @param log_lines: List of strings.
    '''
    allocation = {}
    class_count = {}
    obj_to_class = {}

    for log_line in log_lines:
        if not log_line.startswith('<'):
            continue

        (class_name,
         obj,
         ignore,
         operation,
         count,) = log_line.strip('\r\n').split(' ')[:5]

        # for AddRef/Release `count' is the refcount,
        # for Ctor/Dtor it's the size.

        if ((operation == 'AddRef' and count == '1') or
           operation == 'Ctor'):
            # Examples:
            #     <nsStringBuffer> 0x01AFD3B8 1 AddRef 1
            #     <PStreamNotifyParent> 0x08880BD0 8 Ctor (20)
            class_count[class_name] = class_count.setdefault(class_name, 0) + 1
            allocation[obj] = class_count[class_name]
            obj_to_class[obj] = class_name

        elif ((operation == 'Release' and count == '0') or
             operation == 'Dtor'):
            # Examples:
            #     <nsStringBuffer> 0x01AFD3B8 1 Release 0
            #     <PStreamNotifyParent> 0x08880BD0 8 Dtor (20)
            if obj not in allocation:
                print "An object was released that wasn't allocated!",
                print obj, "@", class_name
            else:
                allocation.pop(obj)
            obj_to_class.pop(obj)

    # Printing out the result.
    print_output(allocation, obj_to_class)


def print_usage():
    print
    print "Usage: find-leakers.py [log-file]"
    print
    print "If `log-file' provided, it will read that as the input log."
    print "Else, it will read the stdin as the input log."
    print

def main():
    '''Main method of the script.'''
    if len(sys.argv) == 1:
        # Reading log from stdin.
        process_log(sys.stdin.readlines())
    elif len(sys.argv) == 2:
        # Reading log from file.
        with open(sys.argv[1], 'r') as log_file:
            log_lines = log_file.readlines()
        process_log(log_lines)
    else:
        print 'ERROR: Invalid number of arguments'
        print_usage()

if __name__ == '__main__':
    main()

