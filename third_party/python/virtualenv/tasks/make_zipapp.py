"""https://docs.python.org/3/library/zipapp.html"""
import argparse
import io
import os.path
import zipapp
import zipfile


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", default=".")
    parser.add_argument("--dest")
    args = parser.parse_args()

    if args.dest is not None:
        dest = args.dest
    else:
        dest = os.path.join(args.root, "virtualenv.pyz")

    bio = io.BytesIO()
    with zipfile.ZipFile(bio, "w") as zipf:
        filenames = ["LICENSE.txt", "virtualenv.py"]
        for whl in os.listdir(os.path.join(args.root, "virtualenv_support")):
            filenames.append(os.path.join("virtualenv_support", whl))

        for filename in filenames:
            zipf.write(os.path.join(args.root, filename), filename)

        zipf.writestr("__main__.py", "import virtualenv; virtualenv.main()")

    bio.seek(0)
    zipapp.create_archive(bio, dest)
    print("zipapp created at {}".format(dest))


if __name__ == "__main__":
    exit(main())
