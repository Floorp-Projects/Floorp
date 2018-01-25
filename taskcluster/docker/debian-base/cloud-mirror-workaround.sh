#!/bin/sh

# The apt in Debian stretch mishandles the HTTP redirects from queue.taskcluster.net to
# cloud-mirror.taskcluster.net, and unescapes the url. This apt method wrapper
# strips redirections and sends directly to the S3 bucket. This has the downside of
# always hitting us-west-2 independently of the zone this runs on, but this is only
# used when creating docker images, so shouldn't generate huge cross-AWS-zone S3
# transfers.

/usr/lib/apt/methods/https | sed -u 's,^New-URI: https://cloud-mirror.taskcluster.net/v1/redirect/[^/]*/[^/]*/https://,New-URI: https://,'
