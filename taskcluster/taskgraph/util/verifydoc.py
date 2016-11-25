# -*- coding: utf-8 -*-
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import re
import os

base_path = os.path.join(os.getcwd(), "taskcluster/docs/")


def verify_docs(filename, identifiers, appearing_as):
    with open(os.path.join(base_path, filename)) as fileObject:
        doctext = "".join(fileObject.readlines())
        if appearing_as == "inline-literal":
            expression_list = ["``" + identifier + "``" for identifier in identifiers]
        elif appearing_as == "heading":
            expression_list = [identifier + "\n[-+\n*]+|[.+\n*]+" for identifier in identifiers]
        else:
            raise Exception("appearing_as = {} not defined".format(appearing_as))

        for expression, identifier in zip(expression_list, identifiers):
            match_group = re.search(expression, doctext)
            if not match_group:
                raise Exception(
                    "{}: {} missing from doc file: {}".format(appearing_as, identifier, filename)
                )
