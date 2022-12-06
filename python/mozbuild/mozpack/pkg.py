# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import concurrent.futures
import lzma
import os
import plistlib
import struct
import subprocess
from pathlib import Path
from string import Template
from typing import List
from urllib.parse import quote

import mozfile

TEMPLATE_DIRECTORY = Path(__file__).parent / "apple_pkg"
PBZX_CHUNK_SIZE = 16 * 1024 * 1024  # 16MB chunks


def get_apple_template(name: str) -> Template:
    """
    Given <name>, open file at <TEMPLATE_DIRECTORY>/<name>, read contents and
        return as a Template

    Args:
        name: str, Filename for the template

    Returns:
        Template, loaded from file
    """
    tmpl_path = TEMPLATE_DIRECTORY / name
    if not tmpl_path.is_file():
        raise Exception(f"Could not find template: {tmpl_path}")
    with tmpl_path.open("r") as tmpl:
        contents = tmpl.read()
    return Template(contents)


def save_text_file(content: str, destination: Path):
    """
    Saves a text file to <destination> with provided <content>
    Note: Overwrites contents

    Args:
        content: str, The desired contents of the file
        destination: Path, The file path
    """
    with destination.open("w") as out_fd:
        out_fd.write(content)
    print(f"Created text file at {destination}")
    print(f"Created text file size: {destination.stat().st_size} bytes")


def get_app_info_plist(app_path: Path) -> dict:
    """
    Retrieve most information from Info.plist file of an app.
    The Info.plist file should be located in ?.app/Contents/Info.plist

    Note: Ignores properties that are not <string> type

    Args:
        app_path: Path, the .app file/directory path

    Returns:
        dict, the dictionary of properties found in Info.plist
    """
    info_plist = app_path / "Contents/Info.plist"
    if not info_plist.is_file():
        raise Exception(f"Could not find Info.plist in {info_plist}")

    print(f"Reading app Info.plist from: {info_plist}")

    with info_plist.open("rb") as plist_fd:
        data = plistlib.load(plist_fd)

    return data


def create_payload(destination: Path, root_path: Path, cpio_tool: str):
    """
    Creates a payload at <destination> based on <root_path>

    Args:
        destination: Path, the destination Path
        root_path: Path, the root directory Path
        cpio_tool: str,
    """
    # Files to be cpio'd are root folder + contents
    file_list = ["./"] + get_relative_glob_list(root_path, "**/*")

    with mozfile.TemporaryDirectory() as tmp_dir:
        tmp_payload_path = Path(tmp_dir) / "Payload"
        print(f"Creating Payload with cpio from {root_path} to {tmp_payload_path}")
        print(f"Found {len(file_list)} files")
        with tmp_payload_path.open("wb") as tmp_payload:
            process = subprocess.run(
                [
                    cpio_tool,
                    "-o",  # copy-out mode
                    "--format",
                    "odc",  # old POSIX .1 portable format
                    "--owner",
                    "0:80",  # clean ownership
                ],
                stdout=tmp_payload,
                stderr=subprocess.PIPE,
                input="\n".join(file_list) + "\n",
                encoding="ascii",
                cwd=root_path,
            )
        # cpio outputs number of blocks to stderr
        print(f"[CPIO]: {process.stderr}")
        if process.returncode:
            raise Exception(f"CPIO error {process.returncode}")

        tmp_payload_size = tmp_payload_path.stat().st_size
        print(f"Uncompressed Payload size: {tmp_payload_size // 1024}kb")

        def compress_chunk(chunk):
            compressed_chunk = lzma.compress(chunk)
            return len(chunk), compressed_chunk

        def chunker(fileobj, chunk_size):
            while True:
                chunk = fileobj.read(chunk_size)
                if not chunk:
                    break
                yield chunk

        with tmp_payload_path.open("rb") as f_in, destination.open(
            "wb"
        ) as f_out, concurrent.futures.ThreadPoolExecutor(
            max_workers=os.cpu_count()
        ) as executor:
            f_out.write(b"pbzx")
            f_out.write(struct.pack(">Q", PBZX_CHUNK_SIZE))
            chunks = chunker(f_in, PBZX_CHUNK_SIZE)
            for uncompressed_size, compressed_chunk in executor.map(
                compress_chunk, chunks
            ):
                f_out.write(struct.pack(">Q", uncompressed_size))
                if len(compressed_chunk) < uncompressed_size:
                    f_out.write(struct.pack(">Q", len(compressed_chunk)))
                    f_out.write(compressed_chunk)
                else:
                    # Considering how unlikely this is, we prefer to just decompress
                    # here than to keep the original uncompressed chunk around
                    f_out.write(struct.pack(">Q", uncompressed_size))
                    f_out.write(lzma.decompress(compressed_chunk))

        print(f"Compressed Payload file to {destination}")
        print(f"Compressed Payload size: {destination.stat().st_size // 1024}kb")


def create_bom(bom_path: Path, root_path: Path, mkbom_tool: Path):
    """
    Creates a Bill Of Materials file at <bom_path> based on <root_path>

    Args:
        bom_path: Path, destination Path for the BOM file
        root_path: Path, root directory Path
        mkbom_tool: Path, mkbom tool Path
    """
    print(f"Creating BOM file from {root_path} to {bom_path}")
    subprocess.check_call(
        [
            mkbom_tool,
            "-u",
            "0",
            "-g",
            "80",
            str(root_path),
            str(bom_path),
        ]
    )
    print(f"Created BOM File size: {bom_path.stat().st_size // 1024}kb")


def get_relative_glob_list(source: Path, glob: str) -> List[str]:
    """
    Given a source path, return a list of relative path based on glob

    Args:
        source: Path, source directory Path
        glob: str, unix style glob

    Returns:
        list[str], paths found in source directory
    """
    return [f"./{c.relative_to(source)}" for c in source.glob(glob)]


def xar_package_folder(source_path: Path, destination: Path, xar_tool: Path):
    """
    Create a pkg from <source_path> to <destination>
    The command is issued with <source_path> as cwd

    Args:
        source_path: Path, source absolute Path
        destination: Path, destination absolute Path
        xar_tool: Path, xar tool Path
    """
    if not source_path.is_absolute() or not destination.is_absolute():
        raise Exception("Source and destination should be absolute.")

    print(f"Creating pkg from {source_path} to {destination}")
    # Create a list of ./<file> - noting xar takes care of <file>/**
    file_list = get_relative_glob_list(source_path, "*")

    subprocess.check_call(
        [
            xar_tool,
            "--compression",
            "none",
            "-vcf",
            destination,
            *file_list,
        ],
        cwd=source_path,
    )
    print(f"Created PKG file to {destination}")
    print(f"Created PKG size: {destination.stat().st_size // 1024}kb")


def create_pkg(
    source_app: Path,
    output_pkg: Path,
    mkbom_tool: Path,
    xar_tool: Path,
    cpio_tool: Path,
):
    """
    Create a mac PKG installer from <source_app> to <output_pkg>

    Args:
        source_app: Path, source .app file/directory Path
        output_pkg: Path, destination .pkg file
        mkbom_tool: Path, mkbom tool Path
        xar_tool: Path, xar tool Path
        cpio: Path, cpio tool Path
    """

    app_name = source_app.name.rsplit(".", maxsplit=1)[0]

    with mozfile.TemporaryDirectory() as tmpdir:
        root_path = Path(tmpdir) / "darwin/root"
        flat_path = Path(tmpdir) / "darwin/flat"

        # Create required directories
        # TODO: Investigate Resources folder contents for other lproj?
        (flat_path / "Resources/en.lproj").mkdir(parents=True, exist_ok=True)
        (flat_path / f"{app_name}.pkg").mkdir(parents=True, exist_ok=True)
        root_path.mkdir(parents=True, exist_ok=True)

        # Copy files over
        subprocess.check_call(
            [
                "cp",
                "-R",
                str(source_app),
                str(root_path),
            ]
        )

        # Count all files (innards + itself)
        file_count = len(list(source_app.glob("**/*"))) + 1
        print(f"Calculated source files count: {file_count}")
        # Get package contents size
        package_size = sum(f.stat().st_size for f in source_app.glob("**/*")) // 1024
        print(f"Calculated source package size: {package_size}kb")

        app_info = get_app_info_plist(source_app)
        app_info["numberOfFiles"] = file_count
        app_info["installKBytes"] = package_size
        app_info["app_name"] = app_name
        app_info["app_name_url_encoded"] = quote(app_name)

        # This seems arbitrary, there might be another way of doing it,
        #   but Info.plist doesn't provide the simple version we need
        major_version = app_info["CFBundleShortVersionString"].split(".")[0]
        app_info["simple_version"] = f"{major_version}.0.0"

        pkg_info_tmpl = get_apple_template("PackageInfo.template")
        pkg_info = pkg_info_tmpl.substitute(app_info)
        save_text_file(pkg_info, flat_path / f"{app_name}.pkg/PackageInfo")

        distribution_tmp = get_apple_template("Distribution.template")
        distribution = distribution_tmp.substitute(app_info)
        save_text_file(distribution, flat_path / "Distribution")

        payload_path = flat_path / f"{app_name}.pkg/Payload"
        create_payload(payload_path, root_path, cpio_tool)

        bom_path = flat_path / f"{app_name}.pkg/Bom"
        create_bom(bom_path, root_path, mkbom_tool)

        xar_package_folder(flat_path, output_pkg, xar_tool)
