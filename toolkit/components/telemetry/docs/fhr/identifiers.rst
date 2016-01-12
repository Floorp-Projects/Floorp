.. _healthreport_identifiers:

===========
Identifiers
===========

Firefox Health Report records some identifiers to keep track of clients
and uploaded documents.

Identifier Types
================

Document/Upload IDs
-------------------

A random UUID called the *Document ID* or *Upload ID* is generated when the FHR
client creates or uploads a new document.

When clients generate a new *Document ID*, they persist this ID to disk
**before** the upload attempt.

As part of the upload, the client sends all old *Document IDs* to the server
and asks the server to delete them. In well-behaving clients, the server
has a single record for each client with a randomly-changing *Document ID*.

Client IDs
----------

A *Client ID* is an identifier that **attempts** to uniquely identify an
individual FHR client. Please note the emphasis on *attempts* in that last
sentence: *Client IDs* do not guarantee uniqueness.

The *Client ID* is generated when the client first runs or as needed.

The *Client ID* is transferred to the server as part of every upload. The
server is thus able to affiliate multiple document uploads with a single
*Client ID*.

Client ID Versions
^^^^^^^^^^^^^^^^^^

The semantics for how a *Client ID* is generated are versioned.

Version 1
   The *Client ID* is a randomly-generated UUID.

History of Identifiers
======================

In the beginning, there were just *Document IDs*. The thinking was clients
would clean up after themselves and leave at most 1 active document on the
server.

Unfortunately, this did not work out. Using brute force analysis to
deduplicate records on the server, a number of interesting patterns emerged.

Orphaning
   Clients would upload a new payload while not deleting the old payload.

Divergent records
   Records would share data up to a certain date and then the data would
   almost completely diverge. This appears to be indicative of profile
   copying.

Rollback
   Records would share data up to a certain date. Each record in this set
   would contain data for a day or two but no extra data. This could be
   explained by filesystem rollback on the client.

A significant percentage of the records on the server belonged to
misbehaving clients. Identifying these records was extremely resource
intensive and error-prone. These records were undermining the ability
to use Firefox Health Report data.

Thus, the *Client ID* was born. The intent of the *Client ID* was to
uniquely identify clients so the extreme effort required and the
questionable reliability of deduplicating server data would become
problems of the past.

The *Client ID* was originally a randomly-generated UUID (version 1). This
allowed detection of orphaning and rollback. However, these version 1
*Client IDs* were still susceptible to use on multiple profiles and
machines if the profile was copied.
