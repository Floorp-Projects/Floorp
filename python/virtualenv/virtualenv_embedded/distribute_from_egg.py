# Called from virtualenv with parameters:
# [--always-unzip] [-v] egg_name
# So, the distribute egg is always the last argument.
import sys
eggname = sys.argv[-1]
sys.path.insert(0, eggname)
from setuptools.command.easy_install import main
main(sys.argv[1:])
