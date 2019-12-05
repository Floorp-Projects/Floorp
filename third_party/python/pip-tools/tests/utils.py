import subprocess


def invoke(command):
    """Invoke sub-process."""
    try:
        output = subprocess.check_output(command, stderr=subprocess.STDOUT)
        status = 0
    except subprocess.CalledProcessError as error:  # pragma: no cover
        output = error.output
        status = error.returncode

    return status, output
