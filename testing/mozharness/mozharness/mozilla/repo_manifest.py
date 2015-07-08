"""
Module for handling repo style XML manifests
"""
import xml.dom.minidom
import os
import re


def load_manifest(filename):
    """
    Loads manifest from `filename` and returns a single flattened manifest
    Processes any <include name="..." /> nodes recursively
    Removes projects referenced by <remove-project name="..." /> nodes
    Abort on unsupported manifest tags
    Returns the root node of the resulting DOM
    """
    doc = xml.dom.minidom.parse(filename)

    # Check that we don't have any unsupported tags
    to_visit = list(doc.childNodes)
    while to_visit:
        node = to_visit.pop()
        # Skip text nodes
        if node.nodeType in (node.TEXT_NODE, node.COMMENT_NODE):
            continue

        if node.tagName not in ('include', 'project', 'remote', 'default', 'manifest', 'copyfile', 'remove-project'):
            raise ValueError("Unsupported tag: %s" % node.tagName)
        to_visit.extend(node.childNodes)

    # Find all <include> nodes
    for i in doc.getElementsByTagName('include'):
        p = i.parentNode

        # The name attribute is relative to where the original manifest lives
        inc_filename = i.getAttribute('name')
        inc_filename = os.path.join(os.path.dirname(filename), inc_filename)

        # Parse the included file
        inc_doc = load_manifest(inc_filename).documentElement
        # For all the child nodes in the included manifest, insert into our
        # manifest just before the include node
        # We operate on a copy of childNodes because when we reparent `c`, the
        # list of childNodes is modified.
        for c in inc_doc.childNodes[:]:
            p.insertBefore(c, i)
        # Now we can remove the include node
        p.removeChild(i)

    # Remove all projects referenced by <remove-project>
    projects = {}
    manifest = doc.documentElement
    to_remove = []
    for node in manifest.childNodes:
        # Skip text nodes
        if node.nodeType in (node.TEXT_NODE, node.COMMENT_NODE):
            continue

        if node.tagName == 'project':
            projects[node.getAttribute('name')] = node

        elif node.tagName == 'remove-project':
            project_node = projects[node.getAttribute('name')]
            to_remove.append(project_node)
            to_remove.append(node)

    for r in to_remove:
        r.parentNode.removeChild(r)

    return doc


def rewrite_remotes(manifest, mapping_func, force_all=True):
    """
    Rewrite manifest remotes in place
    Returns the same manifest, with the remotes transformed by mapping_func
    mapping_func should return a modified remote node, or None if no changes
    are required
    If force_all is True, then it is an error for mapping_func to return None;
    a ValueError is raised in this case
    """
    for r in manifest.getElementsByTagName('remote'):
        m = mapping_func(r)
        if not m:
            if force_all:
                raise ValueError("Wasn't able to map %s" % r.toxml())
            continue

        r.parentNode.replaceChild(m, r)


def add_project(manifest, name, path, remote=None, revision=None):
    """
    Adds a project to the manifest in place
    """

    project = manifest.createElement("project")
    project.setAttribute('name', name)
    project.setAttribute('path', path)
    if remote:
        project.setAttribute('remote', remote)
    if revision:
        project.setAttribute('revision', revision)

    manifest.documentElement.appendChild(project)


def remove_project(manifest, name=None, path=None):
    """
    Removes a project from manifest.
    One of name or path must be set. If path is specified, then the project
    with the given path is removed, otherwise the project with the given name
    is removed.
    """
    assert name or path
    node = get_project(manifest, name, path)
    if node:
        node.parentNode.removeChild(node)
    return node


def get_project(manifest, name=None, path=None):
    """
    Gets a project node from the manifest.
    One of name or path must be set. If path is specified, then the project
    with the given path is returned, otherwise the project with the given name
    is returned.
    """
    assert name or path
    for node in manifest.getElementsByTagName('project'):
        if path is not None and node.getAttribute('path') == path:
            return node
        if node.getAttribute('name') == name:
            return node


def get_remote(manifest, name):
    for node in manifest.getElementsByTagName('remote'):
        if node.getAttribute('name') == name:
            return node


def get_default(manifest):
    default = manifest.getElementsByTagName('default')[0]
    return default


def get_project_remote_url(manifest, project):
    """
    Gets the remote URL for the given project node. Will return the default
    remote if the project doesn't explicitly specify one.
    """
    if project.hasAttribute('remote'):
        remote = get_remote(manifest, project.getAttribute('remote'))
    else:
        default = get_default(manifest)
        remote = get_remote(manifest, default.getAttribute('remote'))
    fetch = remote.getAttribute('fetch')
    if not fetch.endswith('/'):
        fetch += '/'
    return "%s%s" % (fetch, project.getAttribute('name'))


def get_project_revision(manifest, project):
    """
    Gets the revision for the given project node. Will return the default
    revision if the project doesn't explicitly specify one.
    """
    if project.hasAttribute('revision'):
        return project.getAttribute('revision')
    else:
        default = get_default(manifest)
        return default.getAttribute('revision')


def remove_group(manifest, group):
    """
    Removes all projects with groups=`group`
    """
    retval = []
    for node in manifest.getElementsByTagName('project'):
        if group in node.getAttribute('groups').split(","):
            node.parentNode.removeChild(node)
            retval.append(node)
    return retval


def map_remote(r, mappings):
    """
    Helper function for mapping git remotes
    """
    remote = r.getAttribute('fetch')
    if remote in mappings:
        r.setAttribute('fetch', mappings[remote])
        # Add a comment about where our original remote was
        comment = r.ownerDocument.createComment("original fetch url was %s" % remote)
        line = r.ownerDocument.createTextNode("\n")
        r.parentNode.insertBefore(comment, r)
        r.parentNode.insertBefore(line, r)
        return r
    return None


COMMIT_PATTERN = re.compile("[0-9a-f]{40}")


def is_commitid(revision):
    """
    Returns True if revision looks like a commit id
    i.e. 40 character string made up of 0-9a-f
    """
    return bool(re.match(COMMIT_PATTERN, revision))


def cleanup(manifest, depth=0):
    """
    Remove any empty text nodes
    """
    for n in manifest.childNodes[:]:
        if n.childNodes:
            n.normalize()
        if n.nodeType == n.TEXT_NODE and not n.data.strip():
            if not n.nextSibling:
                depth -= 2
            n.data = "\n" + (" " * depth)
        cleanup(n, depth + 2)
