# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, # You can obtain one at http://mozilla.org/MPL/2.0/.

# Utility package for working with moz.yaml files.
#
# Requires `pyyaml` and `voluptuous`
# (both are in-tree under third_party/python)

from __future__ import absolute_import, print_function, unicode_literals


"""
Problem:
    ./mach vendor needs to be able to add or remove files from moz.build files automatically to
    be able to effectively update a library automatically and send useful try runs in.

    So far, it has been difficult to do that.
    Why:
        Some files need to go into UNIFIED_SOURCES vs SOURCES
        Some files are os-specific, and need to go into per-OS conditionals
        Some files are both UNIFIED_SOURCES/SOURCES sensitive and OS-specific.

Proposal:
    Design an algorithm that maps a third party library file to a suspected moz.build location.
    Run the algorithm on all files specified in all third party libraries' moz.build files.
    See if the proposed place in the moz.build file matches the actual place.

Initial Algorithm
    Given a file, which includes the filename and the path from gecko root, we want to find the
        correct moz.build file and location within that file.
    Take the path of the file, and iterate up the directory tree, looking for moz.build files as
    we go.
    Consider each of these moz.build files, starting with the one closest to the file.
    Within a moz.build file, identify the SOURCES or UNIFIED_SOURCES block(s) that contains a file
        in the same directory path as the file to be added.
    If there is only one such block, use that one.
    If there are multiple blocks, look at the files within each block and note the longest length
        of a common prefix (including partial filenames - if we just did full directories the
        result would be the same as the prior step and we would not narrow the results down). Use
        the block containing the longest prefix. (We call this 'guessing'.)

Result of the proposal:
    The initial implementation works on 1675 of 1977 elligible files.
    The files it does not work on include:
        - general failures. Such as when we find that avutil.cpp wants to be next to adler32.cpp
          but avutil.cpp is in SOURCES and adler32.cpp is in UNIFIED_SOURCES. (And many similar
          cases.)
        - per-cpu-feature files, where only a single file is added under a conditional
        - When guessing, because of a len(...) > longest_so_far comparison, we would prefer the
          first block we found.
          -  Changing this to prefer UNIFIED_SOURCES in the event of a tie
             yielded 17 additional correct assignments (about a 1% improvement)
        - As a result of the change immediately above, when guessing, because given equal
          prefixes, we would prefer a UNIFIED_SOURCES block over other blocks, even if the other
          blocks are longer
          - Changing this (again) to prefer the block containing more files yielded 49 additional
            correct assignments (about a 2.5% improvement)

    The files that are ineligible for consideration are:
        - Those in libwebrtc
        - Those specified in source assignments composed of generators (e.g. [f for f in '%.c'])
        - Those specified in source assignments to subscripted variables
          (e.g. SOURCES += foo['x86_files'])
    We needed to iterate up the directory and look at a different moz.build file _zero_ times.
        This indicates this code is probably not needed, and therefore we will remove it from the
        algorithm.
    We needed to guess base on the longest prefix 944 times, indicating that this code is
        absolutely crucial and should be double-checked. (And indeed, upon double-checking it,
        bugs were identified.)

    After some initial testing, it was determined that this code completely fell down when the
    vendoring directory differed from the moz.yaml directory (definitions below.) The code was
    slightly refactored to handle this case, primarily by (a) re-inserting the logic to check
    multiple moz.build files instead of the first and (b) handling some complicated normalization
    notions (details in comments).

Slightly Improved Algorithm Changes:
    Don't bother iterating up the directory tree looking for moz.build files, just take the first.
    When guessing, in the event of a common-prefix tie, prefer the block containing more files

    With these changes, we now Successfully Matched 1724 of 1977 files

CODE CONCEPTS

source-assignment
    An assignment of files to a SOURCES or UNIFIED_SOURCES variable, such as
    SOURCES += ['ffpvx.cpp']

    We specifically look only for these two variable names to avoid identifying things
    such as CXX_FLAGS.

    Sometimes; however, there is an intermediary variable, such as SOURCES += celt_filenames
    In this situation we find the celt_filenames assignment, and treat it as a 'source-assignment'

source-assignment-location
    source-assignment-location is a human readable string that identifies where in the moz.build
    file the source-assignment is. It can used to visually match the location upon manual
    inspection; and given a source-assignment-location, re-identify it when iterating over all
    source-assignments in a file.

    The actual string consists of the path from the root of the moz.build file to the
    source-assignment, plus a suffix number.

    We suffix the final value with an incrementing counter. This is to support moz.build files
    that, for whatever reason, use multiple SOURCES += [] list in the same basic block. This index
    is per-file, so no two assignments in the same file (even if they have separate locations)
    should have the same suffix.

    For example:

    When SOURCES += ['ffpvx.xpp'] appears as the first line of the file (or any other
    unindented-location) its source-assignment-location will be "> SOURCES 1".

    When SOURCES += ['ffpvx.xpp'] appears inside a conditional such as
    `CONFIG['OS_TARGET'] == 'WINNT'` then its source-assignment-location will be
    "> if CONFIG['OS_TARGET'] == 'WINNT' > SOURCES 1"

    When SOURCES += ['ffpvx.xpp'] appears as the second line of the file, and a different
    SOURCES += [] was the first line, then its source-assignment-location will be "> SOURCES 2".

    No two source-assignments may have the same source-assignment-location. If they do, we raise
    an assert.

file vs filename
    a 'filename' is a string specifing the name and sometimes the path of a file.
    a 'file' is an object you get from open()-ing a filename

    A variable that is a string should always use 'filename'

vendoring directory vs moz.yaml directory
    In many cases, a library's moz.yaml file, moz.build file(s), and sources files will all live
    under a single directory. e.g. libjpeg

    In other cases, a library's source files are in one directory (we call this the 'vendoring
    directory') and the moz.yaml file and moz.build file(s) are in another directory (we call this
    the moz.yaml directory).  e.g. libdav1d

normalized-filename
    A filename is 'normalized' if it has been expanded to the full path from the gecko root. This
    requires a moz.build file.

    For example a filename `lib/opus.c` may be specified inside the `media/libopus/moz.build`
    file. The filename is normalized by os.path.join()-ing the dirname of the moz.build file
    (i.e. `media/libopus`) to the filename, resulting in `media/libopus/lib/opus.c`

    A filename that begins with '/' is presumed to already be specified relative to the gecko
    root, and therefore is not modified.

    Normalization gets more complicated when dealing with separate vendoring and moz.yaml
    directories. This is because a file can be considered normalized when it looks like
       third_party/libdav1d/src/a.cpp
    _or_ when it looks like
       media/libdav1d/../../third_party/libdav1d/src/a.cpp
    This is because in the moz.build file, it will be specified as
    `../../third_party/libdav1d/src/a.cpp` and we 'normalize' it by prepending the path to the
    moz.build file.

    Normalization is not just about having an 'absolute' path from gecko_root to file. In fact
    it's not really about that at all - it's about matching filenames. Therefore when we are
    dealing with separate vendoring and moz.yaml directories we will very quickly 're-normalize'
    a normalized filename to get it into one of those foo/bar/../../third_party/... paths that
    will make sense for the moz.build file we are interested in.

    Whenever a filename is normalized, it should be specified as such in the variable name,
    either as a prefix (normalized_filename) or a suffix (target_filename_normalized)

statistic_
    Using some hacky stuff, we report statistics about how many times we hit certain branches of
    the code.
    e.g.
        "How many times did we refine a guess based on prefix length"
        "How many times did we refine a guess based on the number of files in the block"
        "What is the histogram of guess candidates"

    We do this to identify how frequently certain code paths were taken, allowing us to identify
    strange behavior and investigate outliers. This process lead to identifying bugs and small
    improvements.
"""

import os
import re
import ast
import sys
import copy
import fileinput
import subprocess

from pprint import pprint

try:
    from mozbuild.frontend.sandbox import alphabetical_sorted
except Exception:

    def alphabetical_sorted(iterable, key=lambda x: x.lower(), reverse=False):
        return sorted(iterable, key=key, reverse=reverse)


statistics = {
    "guess_candidates": {},
    "number_refinements": {},
    "needed_to_guess": 0,
    "length_logic": {},
}


def log(*args, **kwargs):
    # If is helpful to keep some logging statements around, but we don't want to print them
    #  unless we are debugging
    # print(*args, **kwargs)
    pass


# < python 3.8 shims #########################
# Taskcluster currently runs python 3.5 and it's difficult to move to a more recent one
# Once Tskcl moves to 3.8 we could try to remove this. (Or keep it for developers who are < 3.8)
# From https://github.com/python/cpython/blob/44f6b9aa49d562ab7c67952442b8348346b24141/Lib/ast.py


def _splitlines_no_ff(source):
    """Split a string into lines ignoring form feed and other chars.
    This mimics how the Python parser splits source code.
    """
    idx = 0
    lines = []
    next_line = ""
    while idx < len(source):
        c = source[idx]
        next_line += c
        idx += 1
        # Keep \r\n together
        if c == "\r" and idx < len(source) and source[idx] == "\n":
            next_line += "\n"
            idx += 1
        if c in "\r\n":
            lines.append(next_line)
            next_line = ""

    if next_line:
        lines.append(next_line)
    return lines


def _pad_whitespace(source):
    r"""Replace all chars except '\f\t' in a line with spaces."""
    result = ""
    for c in source:
        if c in "\f\t":
            result += c
        else:
            result += " "
    return result


def ast_get_source_segment(source, node, padded=False):
    """Get source code segment of the *source* that generated *node*.
    If some location information (`lineno`, `end_lineno`, `col_offset`,
    or `end_col_offset`) is missing, return None.
    If *padded* is `True`, the first line of a multi-line statement will
    be padded with spaces to match its original position.
    """
    try:
        lineno = node.lineno - 1
        end_lineno = node.end_lineno - 1
        col_offset = node.col_offset
        end_col_offset = node.end_col_offset
    except AttributeError:
        return None

    lines = _splitlines_no_ff(source)
    if end_lineno == lineno:
        return lines[lineno].encode()[col_offset:end_col_offset].decode()

    if padded:
        padding = _pad_whitespace(lines[lineno].encode()[:col_offset].decode())
    else:
        padding = ""

    first = padding + lines[lineno].encode()[col_offset:].decode()
    last = lines[end_lineno].encode()[:end_col_offset].decode()
    lines = lines[lineno + 1 : end_lineno]

    lines.insert(0, first)
    lines.append(last)
    return "".join(lines)


##############################################


def node_to_readable_file_location(code, node, child_node=None):
    location = ""

    if isinstance(node.parent, ast.Module):
        # The next node up is the root, don't go higher.
        pass
    else:
        location += node_to_readable_file_location(code, node.parent, node)

    location += " > "
    if isinstance(node, ast.Module):
        raise Exception("We shouldn't see a Module")
    elif isinstance(node, ast.If):
        assert child_node
        if child_node in node.body:
            location += "if " + ast_get_source_segment(code, node.test)
        else:
            location += "else-of-if " + ast_get_source_segment(code, node.test)
    elif isinstance(node, ast.For):
        location += (
            "for "
            + ast_get_source_segment(code, node.target)
            + " in "
            + ast_get_source_segment(code, node.iter)
        )
    elif isinstance(node, ast.AugAssign):
        if isinstance(node.target, ast.Name):
            location += node.target.id
        else:
            location += ast_get_source_segment(code, node.target)
    elif isinstance(node, ast.Assign):
        # This assert would fire if we did e.g. some_sources = all_sources = [ ... ]
        assert len(node.targets) == 1, "Assignment node contains more than one target"
        if isinstance(node.targets[0], ast.Name):
            location += node.targets[0].id
        else:
            location += ast_get_source_segment(code, node.targets[0])
    else:
        raise Exception("Got a node type I don't know how to handle: " + str(node))

    return location


def assignment_node_to_source_filename_list(code, node):
    """
    If the list of filenames is not a list of constants (e.g. it's a generated list)
    it's (probably) infeasible to try and figure it out. At least we're not going to try
    right now. Maybe in the future?

    If this happens, we'll return an empty list. The consequence of this is that we
    won't be able to match a file against this list, so we may not be able to add it.

    (But if the file matches a generated list, perhaps it will be included in the
    Sources list automatically?)
    """
    if isinstance(node.value, ast.List) and "elts" in node.value._fields:
        for f in node.value.elts:
            if not isinstance(f, ast.Constant):
                log(
                    "Found non-constant source file name in list: ",
                    ast_get_source_segment(code, f),
                )
                return []
        return [f.value for f in node.value.elts]
    elif isinstance(node.value, ast.ListComp):
        # SOURCES += [f for f in foo if blah]
        log("Could not find the files for " + ast_get_source_segment(code, node.value))
    elif isinstance(node.value, ast.Name) or isinstance(node.value, ast.Subscript):
        # SOURCES += other_var
        # SOURCES += files['X64_SOURCES']
        log("Could not find the files for " + ast_get_source_segment(code, node))
    elif isinstance(node.value, ast.Call):
        # SOURCES += sorted(...)
        log("Could not find the files for " + ast_get_source_segment(code, node))
    else:
        raise Exception(
            "Unexpected node received in assignment_node_to_source_filename_list: "
            + str(node)
        )
    return []


def mozbuild_file_to_source_assignments(normalized_mozbuild_filename):
    """
    Returns a dictionary of 'source-assignment-location' -> 'normalized source filename list'
    contained in the moz.build file specified

    normalized_mozbuild_filename: the moz.build file to read
    """
    source_assignments = {}

    # Parse the AST of the moz.build file
    code = open(normalized_mozbuild_filename).read()
    root = ast.parse(code)

    # Populate node parents. This allows us to walk up from a node to the root.
    # (Really I think python's ast class should do this, but it doesn't, so we monkey-patch it)
    for node in ast.walk(root):
        for child in ast.iter_child_nodes(node):
            child.parent = node

    # Find all the assignments of SOURCES or UNIFIED_SOURCES
    source_assignment_nodes = [
        node
        for node in ast.walk(root)
        if isinstance(node, ast.AugAssign)
        and isinstance(node.target, ast.Name)
        and node.target.id in ["SOURCES", "UNIFIED_SOURCES"]
    ]
    assert (
        len([n for n in source_assignment_nodes if not isinstance(n.op, ast.Add)]) == 0
    ), "We got a Source assignment that wasn't +="

    # Recurse and find nodes where we do SOURCES += other_var or SOURCES += FILES['foo']
    recursive_assignment_nodes = [
        node
        for node in source_assignment_nodes
        if isinstance(node.value, ast.Name) or isinstance(node.value, ast.Subscript)
    ]

    recursive_assignment_nodes_names = [
        node.value.id
        for node in recursive_assignment_nodes
        if isinstance(node.value, ast.Name)
    ]

    # TODO: We do not dig into subscript variables. These are currently only used by two libraries
    #       that use external sources.mozbuild files.
    # recursive_assignment_nodes_names.extend([something<node> for node in
    #    recursive_assignment_nodes if isinstance(node.value, ast.Subscript)]

    additional_assignment_nodes = [
        node
        for node in ast.walk(root)
        if isinstance(node, ast.Assign)
        and isinstance(node.targets[0], ast.Name)
        and node.targets[0].id in recursive_assignment_nodes_names
    ]

    # Remove the original, useless assignment node (the SOURCES += other_var)
    for node in recursive_assignment_nodes:
        source_assignment_nodes.remove(node)
    # Add the other_var += [''] source-assignment
    source_assignment_nodes.extend(additional_assignment_nodes)

    # Get the source-assignment-location for the node:
    assignment_index = 1
    for a in source_assignment_nodes:
        source_assignment_location = (
            node_to_readable_file_location(code, a) + " " + str(assignment_index)
        )
        source_filename_list = assignment_node_to_source_filename_list(code, a)

        if not source_filename_list:
            # In some cases (like generated source file lists) we will have an empty list.
            # If that is the case, just omit the source assignment
            continue

        normalized_source_filename_list = [
            normalize_filename(normalized_mozbuild_filename, f)
            for f in source_filename_list
        ]

        if source_assignment_location in source_assignments:
            source_assignment_location = node_to_readable_file_location(code, a)

        assert (
            source_assignment_location not in source_assignments
        ), "In %s, two assignments have the same key ('%s')" % (
            normalized_mozbuild_filename,
            source_assignment_location,
        )
        source_assignments[source_assignment_location] = normalized_source_filename_list
        assignment_index += 1

    return (source_assignments, root, code)


def unnormalize_filename(normalized_mozbuild_filename, normalized_filename):
    if normalized_filename[0] == "/":
        return normalized_filename

    mozbuild_path = os.path.dirname(normalized_mozbuild_filename) + "/"
    return normalized_filename.replace(mozbuild_path, "")


def normalize_filename(normalized_mozbuild_filename, filename):
    if filename[0] == "/":
        return filename

    mozbuild_path = os.path.dirname(normalized_mozbuild_filename)
    return os.path.join(mozbuild_path, filename)


def get_mozbuild_file_search_order(
    normalized_filename,
    moz_yaml_dir=None,
    vendoring_dir=None,
    all_mozbuild_filenames_normalized=None,
):
    """
    Returns an ordered list of normalized moz.build filenames to consider for a given filename

    normalized_filename: a source filename normalized to the gecko root
    moz_yaml_dir: the path from gecko_root to the moz.yaml file (which is the root of the
                  moz.build files)
    moz_yaml_dir: the path to where the library's source files are
    all_mozbuild_filenames_normalized: (optional) the list of all third-party moz.build files

    If all_mozbuild_filenames_normalized is not specified, we look in the filesystem.

    The list is built out of two distinct steps.

    In Step 1 we will walk up a directory tree, looking for moz.build files. We append moz.build
    files in this order, preferring the lowest moz.build we find, then moving on to one in a
    higher directory.
    The directory we start in is a little complicated. We take the series of subdirectories
    between vendoring_dir and the file in question, and then append them to the moz.yaml
    directory.

    Example:
        When moz_yaml directory != vendoring_directory:
            moz_yaml_dir = foo/bar/
            vendoring_dir = third_party/baz/
            normalized_filename = third_party/baz/asm/arm/a.S
            starting_directory: foo/bar/asm/arm/
        When moz_yaml directory == vendoring_directory
            (In this case, these variables will actually be 'None' but the algorthm is the same)
            moz_yaml_dir = foo/bar/
            vendoring_dir = foo/bar/
            normalized_filename = foo/bar/asm/arm/a.S
            starting_directory: foo/bar/asm/arm/

    In Step 2 we get a bit desparate. When the vendoring directory and the moz_yaml directory are
    not the same, there is no guarentee that the moz_yaml directory will adhere to the same
    directory structure as the vendoring directory.  And indeed it doesn't in some cases
    (e.g. libdav1d.)
    So in this situation we start at the root of the moz_yaml directory and walk downwards, adding
    _any_ moz.build file we encounter to the list. Later on (in all cases, not just
    moz_yaml_dir != vendoring_dir) we only consider a moz.build file if it has source files whose
    directory matches the normalized_filename, so this step, though desparate, is safe-ish and
    believe it or not has worked for some file additions.
    """
    ordered_list = []

    if all_mozbuild_filenames_normalized is None:
        assert os.path.isfile(
            ".arcconfig"
        ), "We do not seem to be running from the gecko root"

    # The first time around, this variable name is incorrect.
    #    It's actually the full path+filename, not a directory.
    test_directory = None
    if (moz_yaml_dir, vendoring_dir) == (None, None):
        # In this situation, the library is vendored into the same directory as
        # the moz.build files. We can start traversing directories up from the file to
        # add to find the correct moz.build file
        test_directory = normalized_filename
    elif moz_yaml_dir and vendoring_dir:
        # In this situation, the library is vendored in a different place (typically
        # third_party/foo) from the moz.build files.
        subdirectory_path = normalized_filename.replace(vendoring_dir, "")
        test_directory = os.path.join(moz_yaml_dir, subdirectory_path)
    else:
        raise Exception("If moz_yaml_dir or vendoring_dir are specified, both must be")

    # Step 1
    while len(os.path.dirname(test_directory)) > 1:  # While we are not at '/'
        containing_directory = os.path.dirname(test_directory)

        possible_normalized_mozbuild_filename = os.path.join(
            containing_directory, "moz.build"
        )

        if not all_mozbuild_filenames_normalized:
            if os.path.isfile(possible_normalized_mozbuild_filename):
                ordered_list.append(possible_normalized_mozbuild_filename)
        elif possible_normalized_mozbuild_filename in all_mozbuild_filenames_normalized:
            ordered_list.append(possible_normalized_mozbuild_filename)

        test_directory = containing_directory

    # Step 2
    if moz_yaml_dir:
        for root, dirs, files in os.walk(moz_yaml_dir):
            for f in files:
                if f == "moz.build":
                    ordered_list.append(os.path.join(root, f))

    return ordered_list


def get_closest_mozbuild_file(
    normalized_filename,
    moz_yaml_dir=None,
    vendoring_dir=None,
    all_mozbuild_filenames_normalized=None,
):
    """
    Returns the closest moz.build file in the directory tree to a normalized filename
    """
    r = get_mozbuild_file_search_order(
        normalized_filename,
        moz_yaml_dir,
        vendoring_dir,
        all_mozbuild_filenames_normalized,
    )
    return r[0] if r else None


def filenames_directory_is_in_filename_list(
    filename_normalized, list_of_normalized_filenames
):
    """
    Given a normalized filename and a list of normalized filenames, first turn them into a
    containing directory, and a list of containing directories. Then test if the containing
    directory of the filename is in the list.

    ex:
        f = filenames_directory_is_in_filename_list
        f("foo/bar/a.c", ["foo/b.c"]) -> false
        f("foo/bar/a.c", ["foo/b.c", "foo/bar/c.c"]) -> true
        f("foo/bar/a.c", ["foo/b.c", "foo/bar/baz/d.c"]) -> false
    """
    path_list = set([os.path.dirname(f) for f in list_of_normalized_filenames])
    return os.path.dirname(filename_normalized) in path_list


def find_all_posible_assignments_from_filename(source_assignments, filename_normalized):
    """
    Given a list of source assignments and a normalized filename, narrow the list to assignments
    that contain a file whose directory matches the filename's directory.
    """
    possible_assignments = {}
    for key, list_of_normalized_filenames in source_assignments.items():
        if not list_of_normalized_filenames:
            continue
        if filenames_directory_is_in_filename_list(
            filename_normalized, list_of_normalized_filenames
        ):
            possible_assignments[key] = list_of_normalized_filenames
    return possible_assignments


def guess_best_assignment(source_assignments, filename_normalized):
    """
    Given several assignments, all of which contain the same directory as the filename, pick one
    we think is best and return its source-assignment-location.

    We do this by looking at the filename itself (not just its directory) and picking the
    assignment which contains a filename with the longest matching prefix.

    e.g: "foo/asm_neon.c" compared to ["foo/main.c", "foo/all_utility.c"], ["foo/asm_arm.c"]
            -> ["foo/asm_arm.c"] (match of foo/asm_)
    """
    length_of_longest_match = 0
    source_assignment_location_of_longest_match = None
    statistic_number_refinements = 0
    statistic_length_logic = 0

    for key, list_of_normalized_filenames in source_assignments.items():
        for f in list_of_normalized_filenames:
            if filename_normalized == f:
                # Do not cheat by matching the prefix of the exact file
                continue

            prefix = os.path.commonprefix([filename_normalized, f])
            if len(prefix) > length_of_longest_match:
                statistic_number_refinements += 1
                length_of_longest_match = len(prefix)
                source_assignment_location_of_longest_match = key
            elif len(prefix) == length_of_longest_match and len(
                source_assignments[key]
            ) > len(source_assignments[source_assignment_location_of_longest_match]):
                statistic_number_refinements += 1
                statistic_length_logic += 1
                length_of_longest_match = len(prefix)
                source_assignment_location_of_longest_match = key
    return (
        source_assignment_location_of_longest_match,
        (statistic_number_refinements, statistic_length_logic),
    )


def edit_moz_build_file_to_add_file(
    normalized_mozbuild_filename,
    unnormalized_filename_to_add,
    unnormalized_list_of_files,
):
    """
    This function edits the moz.build file in-place

    I had _really_ hoped to replace this whole damn thing with something that adds a
    node to the AST, dumps the AST out, and then runs black on the file but there are
    some issues:
      - third party moz.build files (or maybe all moz.build files) aren't always run
        through black
      - dumping the ast out losing comments
    """

    # add the file into the list, and then sort it in the same way the moz.build validator
    # expects
    unnormalized_list_of_files.append(unnormalized_filename_to_add)
    unnormalized_list_of_files = alphabetical_sorted(unnormalized_list_of_files)

    # we're going to add our file by doing a find/replace of an adjacent file in the list
    indx_of_addition = unnormalized_list_of_files.index(unnormalized_filename_to_add)
    indx_of_addition
    if indx_of_addition == 0:
        target_indx = 1
        replace_before = False
    else:
        target_indx = indx_of_addition - 1
        replace_before = True

    find_str = unnormalized_list_of_files[target_indx]

    # We will only perform the first replacement. This is because sometimes there's moz.build
    # code like:
    #   SOURCES += ['file.cpp']
    #   SOURCES['file.cpp'].flags += ['-Winline']
    # If we replaced every time we found the target, we would be inserting into that second
    # line.
    did_replace = False

    # FileInput is a strange class that lets you edit a file in-place, but does so by hijacking
    # stdout, so you just print() the output you want as you go through
    file = fileinput.FileInput(normalized_mozbuild_filename, inplace=True)
    for line in file:
        if not did_replace and find_str in line:
            did_replace = True

            # Okay, we found the line we need to edit, now we need to be ugly about it
            # Grab the type of quote used in this moz.build file: single or double
            quote_type = line[line.index(find_str) - 1]

            if "[" not in line:
                # We'll want to put our new file onto its own line
                newline_to_add = "\n"
                # And copy the indentation of the line we're adding adjacent to
                indent_value = line[0 : line.index(quote_type)]
            else:
                # This is frustrating, we have the start of the array here. We aren't
                # going to be able to indent things onto a newline properly. We're just
                # going to have to stick it in on the same line.
                newline_to_add = ""
                indent_value = ""

            find_str = "%s%s%s" % (quote_type, find_str, quote_type)
            if replace_before:
                replacement_tuple = (
                    find_str,
                    newline_to_add,
                    indent_value,
                    quote_type,
                    unnormalized_filename_to_add,
                    quote_type,
                )
                replace_str = "%s,%s%s%s%s%s" % replacement_tuple
            else:
                replacement_tuple = (
                    quote_type,
                    unnormalized_filename_to_add,
                    quote_type,
                    newline_to_add,
                    indent_value,
                    find_str,
                )
                replace_str = "%s%s%s,%s%s%s" % replacement_tuple

            line = line.replace(find_str, replace_str)

        print(line, end="")  # line has its own newline on it, don't add a second
    file.close()


def edit_moz_build_file_to_remove_file(
    normalized_mozbuild_filename, unnormalized_filename_to_remove
):
    """
    This function edits the moz.build file in-place
    """

    simple_file_line = re.compile(
        "^\s*['\"]" + unnormalized_filename_to_remove + "['\"],*$"
    )
    did_replace = False

    # FileInput is a strange class that lets you edit a file in-place, but does so by hijacking
    # stdout, so you just print() the output you want as you go through
    file = fileinput.FileInput(normalized_mozbuild_filename, inplace=True)
    for line in file:
        if not did_replace and unnormalized_filename_to_remove in line:
            did_replace = True

            # If the line consists of just a single source file on it, then we're in the clear
            # we can just skip this line.
            if simple_file_line.match(line):
                # Do not output anything, just keep going.
                continue

            # Okay, so the line is a little more complicated.
            quote_type = line[line.index(unnormalized_filename_to_remove) - 1]

            if "[" in line or "]" in line:
                find_str = "%s%s%s,*" % (
                    quote_type,
                    unnormalized_filename_to_remove,
                    quote_type,
                )
                line = re.sub(find_str, "", line)
            else:
                raise Exception(
                    "Got an unusual type of line we're trying to remove a file from:",
                    line,
                )

        print(line, end="")

    file.close()


def validate_directory_parameters(moz_yaml_dir, vendoring_dir):
    # Validate the parameters
    assert (moz_yaml_dir, vendoring_dir) == (None, None) or (
        moz_yaml_dir and vendoring_dir
    ), "If either moz_yaml_dir or vendoring_dir are specified, they both must be"

    # Ensure they are provided with trailing slashes
    moz_yaml_dir += "/" if moz_yaml_dir[-1] != "/" else ""
    vendoring_dir += "/" if vendoring_dir[-1] != "/" else ""

    return (moz_yaml_dir, vendoring_dir)


#########################################################
# PUBLIC API
#########################################################


def remove_file_from_moz_build_file(
    normalized_filename_to_remove, moz_yaml_dir=None, vendoring_dir=None
):
    """
    Given a filename, relative to the gecko root (aka normalized), we look for the nearest
    moz.build file, look in that file for the file, and then edit that moz.build file in-place.
    """
    moz_yaml_dir, vendoring_dir = validate_directory_parameters(
        moz_yaml_dir, vendoring_dir
    )

    all_possible_normalized_mozbuild_filenames = get_mozbuild_file_search_order(
        normalized_filename_to_remove, moz_yaml_dir, vendoring_dir, None
    )

    # normalized_filename_to_remove is the path from gecko_root to the file. However, if we vendor
    #    separate from moz.yaml; then 'normalization' gets more complicated as explained above.
    # We will need to re-normalize the filename for each moz.build file we want to test, so we
    #    save the original normalized filename for this purpose
    original_normalized_filename_to_remove = normalized_filename_to_remove

    for normalized_mozbuild_filename in all_possible_normalized_mozbuild_filenames:
        if moz_yaml_dir and vendoring_dir:
            # Here is where we re-normalize the filename. For the rest of the algorithm, we
            #    will be using this re-normalized filename.
            # To re-normalize it, we:
            #   (a) get the path from gecko_root to the moz.build file we are considering
            #   (b) compute a relative path from that directory to the file we want
            #   (c) because (b) started at the moz.build file's directory, it is not
            #       normalized to the gecko_root. Therefore we need to normalize it by
            #       prepending (a)
            a = os.path.dirname(normalized_mozbuild_filename)
            b = os.path.relpath(normalized_filename_to_remove, start=a)
            c = os.path.join(a, b)
            normalized_filename_to_remove = c

        source_assignments, root, code = mozbuild_file_to_source_assignments(
            normalized_mozbuild_filename
        )

        for key in source_assignments:
            normalized_source_filename_list = source_assignments[key]
            if normalized_filename_to_remove in normalized_source_filename_list:
                unnormalized_filename_to_remove = unnormalize_filename(
                    normalized_mozbuild_filename, normalized_filename_to_remove
                )
                edit_moz_build_file_to_remove_file(
                    normalized_mozbuild_filename, unnormalized_filename_to_remove
                )
                return

        normalized_filename_to_remove = original_normalized_filename_to_remove
    raise Exception("Could not remove file")


def add_file_to_moz_build_file(
    normalized_filename_to_add, moz_yaml_dir=None, vendoring_dir=None
):
    """
    This is the overall function. Given a filename, relative to the gecko root (aka normalized),
    we look for a moz.build file to add it to, look for the place in the moz.build file to add it,
    and then edit that moz.build file in-place.

    It accepted two optional parameters. If one is specified they both must be. If a library is
    vendored in a separate place from the moz.yaml file, these parameters specify those two
    directories.
    """
    moz_yaml_dir, vendoring_dir = validate_directory_parameters(
        moz_yaml_dir, vendoring_dir
    )

    all_possible_normalized_mozbuild_filenames = get_mozbuild_file_search_order(
        normalized_filename_to_add, moz_yaml_dir, vendoring_dir, None
    )

    # normalized_filename_to_add is the path from gecko_root to the file. However, if we vendor
    #    separate from moz.yaml; then 'normalization' gets more complicated as explained above.
    # We will need to re-normalize the filename for each moz.build file we want to test, so we
    #    save the original normalized filename for this purpose
    original_normalized_filename_to_add = normalized_filename_to_add

    for normalized_mozbuild_filename in all_possible_normalized_mozbuild_filenames:
        if moz_yaml_dir and vendoring_dir:
            # Here is where we re-normalize the filename. For the rest of the algorithm, we
            #    will be using this re-normalized filename.
            # To re-normalize it, we:
            #   (a) get the path from gecko_root to the moz.build file we are considering
            #   (b) compute a relative path from that directory to the file we want
            #   (c) because (b) started at the moz.build file's directory, it is not
            #       normalized to the gecko_root. Therefore we need to normalize it by
            #       prepending (a)
            a = os.path.dirname(normalized_mozbuild_filename)
            b = os.path.relpath(normalized_filename_to_add, start=a)
            c = os.path.join(a, b)
            normalized_filename_to_add = c

        source_assignments, root, code = mozbuild_file_to_source_assignments(
            normalized_mozbuild_filename
        )

        possible_assignments = find_all_posible_assignments_from_filename(
            source_assignments, normalized_filename_to_add
        )

        if len(possible_assignments) == 0:
            normalized_filename_to_add = original_normalized_filename_to_add
            continue

        assert (
            len(possible_assignments) > 0
        ), "Could not find a single possible source assignment"
        if len(possible_assignments) > 1:
            best_guess, _ = guess_best_assignment(
                possible_assignments, normalized_filename_to_add
            )
            chosen_source_assignment_location = best_guess
        else:
            chosen_source_assignment_location = list(possible_assignments.keys())[0]

        guessed_list_containing_normalized_filenames = possible_assignments[
            chosen_source_assignment_location
        ]

        # unnormalize filenames so we can edit the moz.build file. They rarely use full paths.
        unnormalized_filename_to_add = unnormalize_filename(
            normalized_mozbuild_filename, normalized_filename_to_add
        )
        unnormalized_list_of_files = [
            unnormalize_filename(normalized_mozbuild_filename, f)
            for f in guessed_list_containing_normalized_filenames
        ]

        edit_moz_build_file_to_add_file(
            normalized_mozbuild_filename,
            unnormalized_filename_to_add,
            unnormalized_list_of_files,
        )
        return
    assert False, "Could not find a single moz.build file to edit"


#########################################################
# TESTING CODE
#########################################################


def get_all_target_filenames_normalized(all_mozbuild_filenames_normalized):
    """
    Given a list of moz.build files, returns all the files listed in all the souce assignments
    in the file.

    This function is only used for debug/testing purposes - there is no reason to call this
    as part of 'the algorithm'
    """
    all_target_filenames_normalized = []
    for normalized_mozbuild_filename in all_mozbuild_filenames_normalized:
        source_assignments, root, code = mozbuild_file_to_source_assignments(
            normalized_mozbuild_filename
        )
        for key in source_assignments:
            list_of_normalized_filenames = source_assignments[key]
            all_target_filenames_normalized.extend(list_of_normalized_filenames)

    return all_target_filenames_normalized


def try_to_match_target_file(
    all_mozbuild_filenames_normalized, target_filename_normalized
):
    """
    Runs 'the algorithm' on a target file, and returns if the algorithm was successful

    all_mozbuild_filenames_normalized: the list of all third-party moz.build files
    target_filename_normalized - the target filename, normalized to the gecko root
    """

    # We do not update the statistics for failed matches, so save a copy
    global statistics
    backup_statistics = copy.deepcopy(statistics)

    if "" == target_filename_normalized:
        raise Exception("Received an empty target_filename_normalized")

    normalized_mozbuild_filename = get_closest_mozbuild_file(
        target_filename_normalized, None, None, all_mozbuild_filenames_normalized
    )
    if not normalized_mozbuild_filename:
        return (False, "No moz.build file found")

    source_assignments, root, code = mozbuild_file_to_source_assignments(
        normalized_mozbuild_filename
    )
    possible_assignments = find_all_posible_assignments_from_filename(
        source_assignments, target_filename_normalized
    )

    if len(possible_assignments) == 0:
        raise Exception("No possible assignments were found")
    elif len(possible_assignments) > 1:
        (
            best_guess,
            (statistic_number_refinements, statistic_length_logic),
        ) = guess_best_assignment(possible_assignments, target_filename_normalized)
        chosen_source_assignment_location = best_guess

        statistics["needed_to_guess"] += 1

        if len(possible_assignments) not in statistics["guess_candidates"]:
            statistics["guess_candidates"][len(possible_assignments)] = 0
        statistics["guess_candidates"][len(possible_assignments)] += 1

        if statistic_number_refinements not in statistics["number_refinements"]:
            statistics["number_refinements"][statistic_number_refinements] = 0
        statistics["number_refinements"][statistic_number_refinements] += 1

        if statistic_length_logic not in statistics["length_logic"]:
            statistics["length_logic"][statistic_length_logic] = 0
        statistics["length_logic"][statistic_length_logic] += 1

    else:
        chosen_source_assignment_location = list(possible_assignments.keys())[0]

    guessed_list_containing_normalized_filenames = possible_assignments[
        chosen_source_assignment_location
    ]

    if target_filename_normalized in guessed_list_containing_normalized_filenames:
        return (True, None)

    # Restore the copy of the statistics so we don't alter it for failed matches
    statistics = backup_statistics
    return (False, chosen_source_assignment_location)


def get_gecko_root():
    """
    Using __file__ as a base, find the gecko root
    """
    gecko_root = None
    directory_to_check = os.path.dirname(os.path.abspath(__file__))
    while not os.path.isfile(os.path.join(directory_to_check, ".arcconfig")):
        directory_to_check = os.path.dirname(directory_to_check)
        if directory_to_check == "/":
            print("Could not find gecko root")
            sys.exit(1)

    gecko_root = directory_to_check
    return gecko_root


def get_all_mozbuild_filenames(gecko_root):
    """
    Find all the third party moz.build files in the gecko repo
    """
    third_party_paths = open(
        os.path.join(gecko_root, "tools", "rewriting", "ThirdPartyPaths.txt")
    ).readlines()
    all_mozbuild_filenames_normalized = []
    for path in third_party_paths:
        # We need shell=True because some paths are specified as globs
        # We need an exception handler because sometimes the directory doesn't exist and find barfs
        try:
            output = subprocess.check_output(
                "find %s -name moz.build" % os.path.join(gecko_root, path.strip()),
                shell=True,
            ).decode("utf-8")
            for f in output.split("\n"):
                f = f.replace("//", "/").strip().replace(gecko_root, "")[1:]
                if f:
                    all_mozbuild_filenames_normalized.append(f)
        except Exception:
            pass

    return all_mozbuild_filenames_normalized


def test_all_third_party_files(gecko_root, all_mozbuild_filenames_normalized):
    """
    Run the algorithm on every source file in a third party moz.build file and output the results
    """
    all_mozbuild_filenames_normalized = [
        f for f in all_mozbuild_filenames_normalized if "webrtc" not in f
    ]
    all_target_filenames_normalized = get_all_target_filenames_normalized(
        all_mozbuild_filenames_normalized
    )

    total_attempted = 0
    failed_matched = []
    successfully_matched = 0

    print("Going to try to match %i files..." % len(all_target_filenames_normalized))
    for target_filename_normalized in all_target_filenames_normalized:
        result, wrong_guess = try_to_match_target_file(
            all_mozbuild_filenames_normalized, target_filename_normalized
        )

        total_attempted += 1
        if result:
            successfully_matched += 1
        else:
            failed_matched.append((target_filename_normalized, wrong_guess))
        if total_attempted % 100 == 0:
            print("Progress:", total_attempted)

    print(
        "Successfully Matched %i of %i files" % (successfully_matched, total_attempted)
    )
    if failed_matched:
        print("Failed files:")
        for f in failed_matched:
            print("\t", f[0], f[1])
    print("Statistics:")
    pprint(statistics)


if __name__ == "__main__":
    gecko_root = get_gecko_root()
    os.chdir(gecko_root)

    add_file_to_moz_build_file(
        "third_party/jpeg-xl/lib/jxl/enc_photon_noise.cc",
        "media/libjxl",
        "third_party/jpeg-xl",
    )

    # all_mozbuild_filenames_normalized = get_all_mozbuild_filenames(gecko_root)
    # test_all_third_party_files(gecko_root, all_mozbuild_filenames_normalized)
