# Improved JPEG decoder implementation

This subdirectory contains a JPEG decoder implementation that is API and ABI
compatible with libjpeg62.

*NOTE*: This is still a work in progress, currently only API functions called
from libjxl's benchmark_xl tool are implemented.

To decompress an ```input.jpg``` file with this new library:

```
(from the libjxl root directory)
$ ./ci.sh opt
$ LD_PRELOAD=./build/libjpeg.so.62 ./build/tools/benchmark_xl --input input.jpg --codec=jpeg --decode_only --save_decompressed --output_dir .
```

The decompressed file will be saved as ```input.jpg.jpeg.png```.

To benchmark the jpeg encoding-decoding round-trip on an ```input.png``` with
the new library, first build a statically linked ```cjpeg-static``` binary,
which is found in ```$PATH```, and then run:

```
(from the libjxl root directory)
$ ./ci.sh opt
$ LD_PRELOAD=./build/libjpeg.so.62 ./build/tools/benchmark_xl --input input.png --codec=jpeg:cjpeg-static:q90
```

