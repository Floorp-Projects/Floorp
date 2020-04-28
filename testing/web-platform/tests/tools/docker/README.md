This docker images is used for testing Chrome, Firefox, WebKitGTK and running
other tasks on Taskcluster. When any of the files in this directory change, the
images must be updated as well. Doing this requires you be part of the
'webplatformtests' organization on Docker Hub; ping @Hexcles or @stephenmcgruer
if you are not a member.

In this directory, run the following, where `<tag>` is of the form
`webplatformtests/wpt:{current-version + 0.01}`:

```sh
# --pull forces Docker to get the newest base image.
docker build --pull -t <tag> .
docker push <tag>
```

Then update `.taskcluster.yml` in the top-level directory to point to the new
image you have uploaded.
