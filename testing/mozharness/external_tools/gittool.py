#!/usr/bin/env python
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
### Compressed module sources ###
module_sources = [
    (
        "util",
        "eJxlkMEKgzAQRO/5isWTQhFaSg8Ff6LnQknM2ixoItmov1+T2FLb3DY7mZkXGkbnAxjJpiclKI+K\nrOSWSAihsQM28sjBk32WXF0FrKe4YZi8hWAwrZMDuC5fJC1wkaQ+K7eIOqpXm1rTEzmU1ZahLuc/\ncwYlGS9nQNs6jfoACwUDQVIf/RdDAXmULYK0Gpo1aXAz6l3sG6VWJ/nIdjHdx45jWTR3W3xVSKTT\n8NuEE9a+DMzomZz9QOencdyDJ7LvH6zEC9SEeBQ=\n",
    ),
    (
        "util.file",
        "eJzNVk2P2zYQvftXTF0sVl64am20lwA+FNsCKVCkRZJbEHhpkbKYlUiBpNb2v+8MP0RZ3uTQU3SwJXLmcWbem5GWy+Vb0fbCQD2oykmtLDgNDVO8FVBL/NG4y/zOcrlcyK7XxkGrj0epjulR23Rnm8HJNj01zDatPKRHJ7qeMBe10R1YeS47/SJsWWlVy2PPjMVAou17dnr0y//65QWeCLt0bnkU7m+8FabY7xXrxH6/WiwWXNRQ6Q6BREHnbNY+he3qzQLwwvjjLibZCDRVTihnQdfgTtrb2jX0zFrBQSoQEs0MMOvd8cJaqFCVUCF0xe2qEtbKQypYz1xjS/hD4zbDLLseM44AiskXAdYZTCKGKq1Wa7AauAalHQwWa66gZeZItFBMVHjyljVIK5V1TFVjhgdmRQCMadLl97BeFHAyvDf3a/hoBrH6Ctj2G2DbKdj2BswfsR8LugsLpRGMF9liO7fYTi2McINRN1C7mWvkmUsjKqfNBVXiGOZRjCtYzaGu5TnTDu8DtsOAKXFi/4hMqAzj1UA4frM3+kVyVMFJtrxihnukALsmTiXyQ53y1Fqotf754cDMEySiGukwQ6Yuxae6FIrbEyqpiFGhPfJK+tK2bKV1GEMOfvV5pIfUgEiZCFR/KYzRplg+6qHl3qKWisPDnSXAO7uEOyhSnBn0qsKIOTZLf6HqFtZUaG7d2i91moudJ3es4CMuA1pRzt6uxy4S5oV0jAOik9gBNDywLcDJDqUv6xELhea1ksoThkR5c/qYeXLMqU9caGPmMrNATbuJRcjVNmxjh66oc1JJFUj4h7e//7Tx88pPg9l08H39VD+tqciNOBOFHXMj3Uh2HHUlHZMk349NQw1zuA/Lp4bQqB45vUOrq2dqij50xG+bLTzA5pftrznBqAhvmj29N/o8jytNOfScOVF4y2vmSwyeyyP2eDHWhdViP6hWqmff3DROc4nCBqQNkEeljSLWvRBt6iZfIY4jT907EGdUdSqOM5c30xxQ9DQhS2lJ97MTx5GDLWK0dl7DNoxxG1vmxNoc6RoF2XN9UlO9zpF8s3mI2/1MVIzri5aqCGfXq1fNryrW39ogkukoOULJ26K14vr8STV8yezXyhFR5yx5G3GuRO/gnw9/EiH4soLJKT/CX0SYgOU7jeOrauI73eTZsJySI2i+KE1Td3sdQlDQN5IxTFox1dSsvVFzWVZk0F58n49TBQ3w5UfSYv5LQRuGY1liF5pOcImKDtOlwbFtqAD0AUJ40TkJruYoiq73ct2N3xxl92zpnibtLlUd78ms8MGtXptN+vCl8C3sk/BNvCYqah4aG8+6P+HijXOeQTGWYEHa8OQVcTlWJhau1Yzvw+fQXAthFTOafRkVk6lJq2GAfEren1fwww7yYyYtzoR3WonpjAgoMR78zkrhhD98+Qn/nYhV6MM/2rGhTeTmOHAi7qNxEf9XnsHJfsAoZpirmyCjC4ZzYzuNPYZyE/weVfS9JECh/K8cDlq330sSFItgty6vJfIfMB3quQ==",
    ),
    (
        "util.commands",
        "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",
    ),
    (
        "util.retry",
        "eJytVk2P2zYQvetXDFwsLDuC4C2wORhxsUHQFgWKnHqXaYmyiUqkQ1LxGkX/e2dIivpy0h6qw1oa\nDh9nHt/MjmivSluwouVJrVULdSdLq1RjQPilm2ZX49dKJS1/s4049YvB0jLJzlwnwdqo81nIc4K/\ncOi/8jO3v+Mr12lRSNbyotgkSVLxGjS3+p6y0golM2DW8vZqzeElA9NwfqXgDu93GbTsrRgsL7AF\ntCYQH4dT8LeSPJQ0h/Tn/j3bZFA2nMnuevisJMdj9Bkd0Pznzb3+9fdm77BWq9Un1jRw9AGtgdHB\nou1aUDVaQ3hrR5qBTlrRgLBgurLkvDJDRJgb6xqLyYNV8JLDMUa/BmHAXjjIrj1xTciGI5uVIdcb\nEzainLi9cS4jL9kM9/0OmKygUt2pIRNn5cVT0W/J0C3CTbOZULrOAY5zEl2kDGx3bThuiTiRWsqD\nYfoX1TUVRgsl684Xm8NvNQwwoDBbTa4S/yjDI1AjjOUVCPnobKY5aCYMOjgJ9peSEXl3uAm8qNOA\nFVxF2/JKMMubuwvjGK7e5XLV6quo0ItYK/Gm2QkzwwsksBHrbm0KBqy2mASmELMnxD7hz4pU1bVc\nWhOBQohwZYZCwwsTnpu76nSvSV92BKf5l05o1NUSCUPEwzTKBCOSlIEjHnFckbp1ScH1WxtuTETO\nI86R9L526R+9+D3P/SU7NYnSkkBiFBQ4pQBY8YOY0HjsKVxj4bgFSpR6Q7CHwt6M16SyMXWlB9dg\n876inlY8fBj6wX6QjzrnFT9153Q19X6qwBHgJDc2r+AJ0lHbgOkxo66z8YFI7GLP7u12EUiQhA+H\nWI5DJKjd/QSWQhOyVunKCXsP1FeoRJ8MysJeXA/a41ffhPz7agISn1U4EX4IKfQN01id0u6Nf/VQ\n+CFD+LE4uO00qsNtS7fklcF2G/yjqy+/RTNdphZYj7lREQwVv4dVRl8FMXD4Q3d8Gg3ebrjt/SLf\nsJAuduBNPGL+m4T/Kr4S36QyidwSbWM1Ttih1jE/b5DNT7D7D+f9wlAfVVCQu+kq9vUTrxV1M/LE\nJYzl8T3TMyhw4UPW3K2n3/EaAj+M3rfw48JzluWkFJYZz7En7hNvGg2E7AZjLSTKf1YiEt5RbQ1z\ngHB9YOvV10vUfwWheoD1eg0f8T9hqTSz2EKQ2zBHbHLszqylTtYZHEu8/+sA7tmiA2ulRhrL8zyZ\n+8Zh5Hm3G48jz7sB5cR0utlPYEKESfQpImRRowIVxkmNebTt1Q1a3jqeIMZbyeWKA9S8dveP6tyz\nQXhh2PGbwrjjfxBjxPS39Ti7gmR21DLE5PFqyB3v+3U2OsY5EEsjBP3vIlhwFlEKYb/D0v/M0CN2\n7oLjNNTHkvwDPQB6iA==\n",
    ),
    (
        "util.git",
        "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",
    ),
]

### Load the compressed module sources ###
import sys, imp, base64, zlib

for name, source in module_sources:
    source = zlib.decompress(base64.b64decode(source))
    mod = imp.new_module(name)
    exec(source, mod.__dict__)
    sys.modules[name] = mod

### Original script follows ###
#!/usr/bin/python
"""%prog [-p|--props-file] [-r|--rev revision] [-b|--branch branch]
         [-s|--shared-dir shared_dir] repo [dest]

Tool to do safe operations with git.

revision/branch on commandline will override those in props-file"""

# Import snippet to find tools lib
import os
import site
import logging

site.addsitedir(
    os.path.join(os.path.dirname(os.path.realpath(__file__)), "../../lib/python")
)

try:
    import simplejson as json

    assert json
except ImportError:
    import json

from util.git import git


if __name__ == "__main__":
    from optparse import OptionParser

    parser = OptionParser(__doc__)
    parser.set_defaults(
        revision=os.environ.get("GIT_REV"),
        branch=os.environ.get("GIT_BRANCH", None),
        loglevel=logging.INFO,
        shared_dir=os.environ.get("GIT_SHARE_BASE_DIR"),
        mirrors=None,
        clean=False,
    )
    parser.add_option(
        "-r", "--rev", dest="revision", help="which revision to update to"
    )
    parser.add_option("-b", "--branch", dest="branch", help="which branch to update to")
    parser.add_option(
        "-p",
        "--props-file",
        dest="propsfile",
        help="build json file containing revision information",
    )
    parser.add_option(
        "-s", "--shared-dir", dest="shared_dir", help="clone to a shared directory"
    )
    parser.add_option(
        "--mirror",
        dest="mirrors",
        action="append",
        help="add a mirror to try cloning/pulling from before repo",
    )
    parser.add_option(
        "--clean",
        dest="clean",
        action="store_true",
        default=False,
        help="run 'git clean' after updating the local repository",
    )
    parser.add_option(
        "-v", "--verbose", dest="loglevel", action="store_const", const=logging.DEBUG
    )

    options, args = parser.parse_args()

    logging.basicConfig(level=options.loglevel, format="%(asctime)s %(message)s")

    if len(args) not in (1, 2):
        parser.error("Invalid number of arguments")

    repo = args[0]
    if len(args) == 2:
        dest = args[1]
    else:
        dest = os.path.basename(repo)

    # Parse propsfile
    if options.propsfile:
        js = json.load(open(options.propsfile))
        if options.revision is None:
            options.revision = js["sourcestamp"]["revision"]
        if options.branch is None:
            options.branch = js["sourcestamp"]["branch"]

    got_revision = git(
        repo,
        dest,
        options.branch,
        options.revision,
        shareBase=options.shared_dir,
        mirrors=options.mirrors,
        clean_dest=options.clean,
    )

    print("Got revision %s" % got_revision)
