.. _cloudsync_dataformat:

===========
Data Format
===========

All fields are required unless noted otherwise.

Bookmarks
=========

Record
------

type:
    record type; one of CloudSync.bookmarks.{BOOKMARK, FOLDER, SEPARATOR, QUERY, LIVEMARK}

id:
    GUID for this bookmark item

parent:
    id of parent folder

index:
    item index in parent folder; should be unique and contiguous, or they will be adjusted internally

title:
    bookmark or folder title; not meaningful for separators

dateAdded:
    timestamp (in milliseconds) for item added

lastModified:
    timestamp (in milliseconds) for last modification

uri:
    bookmark URI; not meaningful for folders or separators

version:
    data layout version

Tabs
====

ClientRecord
------------

id:
    GUID for this client

name:
    name for this client; not guaranteed to be unique

tabs:
    list of tabs open on this client; see TabRecord

version:
    data layout version


TabRecord
---------

title:
    name for this tab

url:
    URL for this tab; only one tab for each URL is stored

icon:
    favicon URL for this tab; optional

lastUsed:
    timetamp (in milliseconds) for last use

version:
    data layout version
