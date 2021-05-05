## JPEG XL GDK Pixbuf


If already installed by the [Installing section of README.md](../../README.md#installing), then the plugin should be in the correct place, e.g.

```/usr/lib/x86_64-linux-gnu/gdk-pixbuf-2.0/2.10.0/loaders/libpixbufloader-jxl.so```

Otherwise we can link them manually:

```bash
sudo cp $your_build_directory/plugins/gdk-pixbuf/libpixbufloader-jxl.so /usr/lib/x86_64-linux-gnu/gdk-pixbuf-2.0/2.10.0/loaders/libpixbufloader-jxl.so
```


Then we update the cache, for example with:

```bash
sudo /usr/lib/x86_64-linux-gnu/gdk-pixbuf-2.0/gdk-pixbuf-query-loaders --update-cache
```

In order to get thumbnails with this, first one has to add the jxl MIME type, see [../mime/README.md](../mime/README.md).

Update the Mime database with
```bash
update-mime --local
```
or
```bash
sudo update-desktop-database
```

Then possibly delete the thumbnail cache with
```bash
rm -r ~/.cache/thumbnails
```
and restart the application displaying thumbnails, e.g. `nautilus -q` to display thumbnails.