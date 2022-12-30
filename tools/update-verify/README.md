Mozilla Build Verification Scripts
==================================

Contents
--------

updates -> AUS and update verification

l10n -> l10n vs. en-US verification

common -> useful utility scripts

Update Verification
-------------------

`verify.sh`

>  Does a low-level check of all advertised MAR files. Expects to have a
>  file named all-locales, but does not (yet) handle platform exceptions, so
>  these should be removed from the locales file.
>
>  Prints errors on both STDOUT and STDIN, the intention is to run the
>  script with STDOUT redirected to an output log. If there is not output
>  on the console and an exit code of 0 then all tests pass; otherwise one
>  or more tests failed.
>
>  Does the following:
>
>  1) download update.xml from AUS for a particular release
>  2) download the partial and full mar advertised
>  3) check that the partial and full match the advertised size and sha1sum
>  4) downloads the latest release, and an older release
>  5) applies MAR to the older release, and compares the two releases.
>
>  Step 5 is repeated for both the complete and partial MAR.
>
>  Expects to have an updates.cfg file, describing all releases to try updating
>  from.

Valid Platforms for AUS
-----------------------
- Linux_x86-gcc3
- Darwin_Universal-gcc3
- Linux_x86-gcc3
- WINNT_x86-msvc
- Darwin_ppc-gcc3

---
Running it locally
==================

Requirements:
-------------

- [Docker](https://docs.docker.com/get-docker/)
- [optional | Mac] zstd (`brew install zst`)

Docker Image
------------

1. [Ship-it](https://shipit.mozilla-releng.net/recent) holds the latest builds.
1. Clicking on "Ship task" of latest build will open the task group in
Taskcluster.
1. On the "Name contains" lookup box, search for `release-update-verify-firefox`
and open a `update-verify` task
1. Make note of the `CHANNEL` under Payload. ie: `beta-localtest`
1. Click "See more" under Task Details and open the `docker-image-update-verify`
task.

Download the image artifact from *docker-image-update-verify* task and load it
manually
```
zstd -d image.tar.zst
docker image load -i image.tar
```

**OR**

Load docker image using mach and a task
```
# Replace TASK-ID with the ID of a docker-image-update-verify task
./mach taskcluster-load-image --task-id=<TASK-ID>
```

Update Verify Config
--------------------

1. Open Taskcluster Task Group
1. Search for `update-verify-config` and open the task
1. Under Artifacts, download `update-verify.cfg` file

Run Docker
----------

To run the container interactively:
> Replace `<MOZ DIRECTORY>` with gecko repository path on local host <br />
> Replace `<UVC PATH>` with path to `update-verify.cfg` file on local host.
ie.: `~/Downloads/update-verify.cfg`
> Replace `<CHANNEL>` with value from `update-verify` task (Docker steps)

```
docker run \
  -it \
  --rm \
  -e CHANNEL=beta-localtest \
  -e MOZ_FETCHES_DIR=/builds/worker/fetches \
  -e MOZBUILD_STATE_PATH=/builds/worker/.mozbuild \
  -v <UVC PATH>:/builds/worker/fetches/update-verify.cfg
  -v <MOZ DIRECTORY>:/builds/worker/checkouts/gecko \
  -w /builds/worker/checkouts/gecko \
  update-verify
```
> Note that `MOZ_FETCHES_DIR` here is different from what is used in production.

`total-chunks` and `this-chunk` refer to the number of lines in `update-verify.cfg`
```
./tools/update-verify/scripts/chunked-verify.sh --total-chunks=228 --this-chunk=4
```
