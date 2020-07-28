# HACKING 

## release

ensure correct version in CHANGES.md and appdirs.py, and:

```
python setup.py register sdist bdist_wheel upload
```

## docker image

```
docker build -t appdirs .
```

