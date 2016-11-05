# Upload Symbols
Docker worker to upload crashreporter symbols as a separate taskcluster task.

## Building
`$ docker build -t upload_symbols .`

`$ docker run -i -t upload_symbols`

Then from inside the container, run:

`$ ./bin/upload.sh`

In order to run the `upload_symbols.py` script properly, the Dockerfile expects  a text file `socorro_token` embedded with the api token at the root directory before.

The following environmental variables must be set for a sucessful run.
- `ARTIFACT_TASKID` : TaskId of the parent build task
- `GECKO_HEAD_REPOSITORY` : The head repository to download the checkout script
- `GECKO_HEAD_REV` : Revision of the head repository to look for

## Example
The container can be run similar to its production environment with the following command:
```
docker run -ti \
-e ARTIFACT_TASKID=Hh5vLCaTRRO8Ql9X6XBdxg \
-e GECKO_HEAD_REV=beed30cce69bc9783d417d3d29ce2c44989961ed \
-e GECKO_HEAD_REPOSITORY=https://hg.mozilla.org/try/ \
upload_symbols /bin/bash bin/upload.sh
```
