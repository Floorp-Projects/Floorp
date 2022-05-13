#!/usr/bin/env python3
# -*- coding: utf-8 -*-

from conans import ConanFile, tools

def get_version():
    git = tools.Git()
    try:
        return git.run("describe --tags --abbrev=0")
    except:
        return None

class Function2Conan(ConanFile):
    name = "function2"
    version = get_version()
    license = "boost"
    url = "https://github.com/Naios/function2"
    author = "Denis Blank (denis.blank@outlook.com)"
    description = "Improved and configurable drop-in replacement to std::function"
    homepage = "http://naios.github.io/function2"
    no_copy_source = True
    scm = {
        "type": "git",
        "url": "auto",
        "revision": "auto"
    }

    def package(self):
        self.copy("LICENSE.txt", "licenses")
        self.copy("include/function2/function2.hpp")

    def package_id(self):
        self.info.header_only()
