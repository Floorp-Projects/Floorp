## JPEG XL MIME type

If not already installed by the [Installing section of README.md](../../README.md#installing), then it can be done manually:

### Install
```bash
sudo xdg-mime install --novendor image-jxl.xml
```

Then run:
```
update-mime --local
```


### Uninstall
```bash
sudo xdg-mime uninstall image-jxl.xml
```

