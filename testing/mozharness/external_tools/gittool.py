#!/usr/bin/env python3
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
        "eJzdWW1v2zgS/u5fwXPQs9x1laDFvSBA9pDdJnfBtkkucS9XtIUgS+OYG4n0kVQc76+/GZKSKPkl2T3slzOQF4nkcF6eeWZID4fD80pkhkuh2VwqpiohuLhnmSzLVOR6OBwOeLmUyjBdzZZKZqB1/UY2/xleQv3/skgNiirr50Le36PIRgx/GuArdlIPxPdgPuC/oKIkEWkJSTIeDIxaHw8YfvyyFRfv3s55ARsv0yW3764/311cvnuLgqeqggE8ZbA07MLOPFNKquPetPO00DAYDHKYky5JVuYR/kzY69cPq1Td67FbccCyVc64ZnoJGU8LxgUzixS3B5YWq3St2SoVhnG0XXFhAEXAIwjG5/hupJmQxguCp2XBM26KNcsWUoOw791uqJH7J87kch2NnaFzNsLdRySD9nUznF7t0i92zjeUIDW5E5/8erQr5mIuo6GP6DG7nZ7eTIc7h1/pIXsVBDsuuDZv0S8FF0D+GbulhJYHWE/YY1pUQLphZGNuQOFPqaOOC3fuZfebOEHaqMgKwx1cVEpQ95CAeIzwx4sSsKI3zlb8hyspQo/58bha5qkBu9C+V2AqJephvwHCfWfYEfo3lWA4xKKUkReYnDOcUJUgjB7HjN2kXEPoqx/TooD82j1Z0GEErTSzgDqpEAXcIGa4WWBYxZtfQEkUdjHvTHL6IuiqjITNq6JYT0JLjmJKTu/ZLVZ4yN0Bc65A6YjhdI4RqlOPzSBLKzQBkZxLMTIOyGgEmUracLGwAc1rAKPnH1OlWVnlhIh7FG4nysosK9NgFueN9uGVxgmvbYQ7I075hgPsA84mmonpl4/1Dlg5XY6H7STvsiBS2QKyhyRDU7e5zZPH3shuQfaoUeHs5ubqZjQhQQkNnRAnBfoQcFwacYGSAzuhSJca8q617A0z+yw+u3zPolfx27mu14+/Csyr+iGA+38qDmYn6HN4FIg0yq4liAhTzL+ZsNFq1MmkPc7UJscYnDRL8RmUap9bZ7d6KSilgVYxDeoR1IQhPBVVhZNLKQBf68VPsG4fTkb4azRpnBN8eqah1GSpYM6f0LovuOpbjdZ6izYI7dwYngyIPPoyelOMWm2+NfTsFHpmKcel6Mtlahb4dokxI0GRWzv2woKV6XJJK50Pmq245kKbVGTgXDRLNSBpYhaOO/uTffjnWxitgOlqL3zHesjvcGKyxMSXOc+SpazjivOoO0ioyCmk6pN3R0cTRuPtq6P47Z+2hsN/ahmEllmaPfhQ/kryJW5JAwgyokwLQ0bciomxLIDaGiutNoUy7cW0vUnZVtZvoe2WsgfbXMDmvgdD4Zh7M5RmFXJ7kfQ5uliuiG4bF3gNHXLc5thqwBNklRVl0gfQyBUCGyuaKDai12IGQaVMQlTTUAaGFd1Rx+L/usw8W0HIEygkAMy1Zcctjmg9uaVcFalG5rUgPQlmDhwoFtjc2ta1NUPRtrRnbHNwPGhG0Hgc5La3ZJRBx52UI5NymFX30dCj2kMQQWUbLpWNOwucvJMTdnS8kbu7KlJgQX/JbylRfRl1znQGAFv2TQ3V8wkdqWxCdBL4UMjVjp4CvUGDb8KIfb8nf+hzgAprMOESI1n6KHmOB4e0LO2xymd8N1ghLHDfnsG1851GO5yOGvdZZdNNASrIQaTQTiqixjx6pcd4GNhD5lF//SRQcLwZ005HF376cqKdlLQp1DdrZ/YPqb51BzIeLBKs8dZYi5sca4eXgYsnjN8LSTU1juPhvkLmt97W3tmhrUglLbytqEdTXGrSxoRG3qbANMSdiuFeJYZ9WGKW5eNn1kzqPbdA4JmltxQYcpohiDTBwQfKbTo7Rn2NnvPi858ADf+zrJpwAiawHKALgGXUaWbqlgiP1IkrCY7zuciKKofE97X2ImFCpc0kGOD6+de2NG37EJQ612F0N3TlD3sMMHVv3XQOfmbuLilqKdh94oNaEU1y00zWuDGyVHtAJdpU2EZ4odhcQlrGzlF3Lqru6mMtK6YXsipyKudU223ZbxoZpL4sXaItgG1agUbi31JWgmy20pxeLPoex+ewwuWy0uSB2dqAHk8aNXNI80JmD81pl/fdEfYxVu1Oqb6dvr/6NHWnuk5KNpOpfA6e63B+n0Ot98IJG7ZZt+ugu7cL8SeucPDi+qw5ebk/z+dO1+Z6V2QsbgJNGq3toNs6RqTk3RJqRx2gM5kD+8NGd/GSut2T4ot4sA11QHXqbXZBz14NdCa64Y17g6DrfMnNAEuxx2lVOWAfsRfHJQpc8nj/uQxm0iVqU77am4C4cXSgQOu33+UCYf/9AZ7RHyGhY2xEv1pmm1L2U9+Pc5ZFmgFxmr0j1AssM0WsSuQSb/9KqgeN/GPojFAJhKY/WYlcrnTMplj0cAJyyA+QojeNddDV7em5leiOWzm4w1T0g0Sfr9DguZIlm1W8yGfSIJ2njxDX99jj4BBT90A3ZA0xmu2IrT2DOtcTrhOvT7T1DvOTppWJM6ueyqhnaNHTG3Uu62Gr2ZFg3F4RYHXQoYsdjqyX0Tl5gUe7mZIPeKrDNu5Bh/lQC+GahvpC6IMzXCT3a7Rf0EuEHLB/4bFuvrYhy7mCzEi1JpwQWxyu6Mh36DoRaENbKUXgoRuSgd8pW5QytxtN2JH8y9GRjxStoesYYmXprqpxm76qdAi33yy4S2Nr1M+SCy+QRsaBlzEh71zEJravrg+tCwQUszrThULJtbZHdiqbNOy8EcixB3c81B22lgOnMoy0LJmv80QHohc+p+oJGwkz6kauxUlqrxeixjR7v3SXXP003mw6D9gFlWfx4Iq0PTWuUN8ccw8P4hNMGoNTAiuxI76cbpFTphnKAUpNrNKkurXlYlSye2mbA8kKIAncfl1hqWCLnDfUIvrw9pDpQh3YdST/3MR7A5kU7WZuz/SQrdo5AU/2G/QD9t66AEu0bWBIk15+dYMQpscuLXabZWHcm+ZTqqcuDZQNsJGMD0KCso1jn4no+kfgPEuKpfxlQQ7X2vH4FnbTa22gpJruv76L3Zv2eyg/A6MaDX1+IG0OP/JMSS3nZhgY7uum/SKuszy2nbymiy08/3z++93F5TPrduZDONebtY1vmxJVJ/Ub+zXenGcsrEk1q9IahLC4NwsdHsLYx9N/J9en03/EjQLX0h6ErIezQs5moEDFy3XcuTVLNfKY6brc3SrRTi0hISMWbWF17EYo8NNGX/Hzt69fR+w7u3JP2WiWbTg2XOS/Bz0O86oL6y1MGvi9vlAe5VCww3N2+E82xEI6wgNgtyBs5llfgsU2O7zdLiNsnzo1hKg4nflvhJvvh+NbMOf499QYxZHYQN9FfedNgunnFx/OktPp9Obih0/Ts+Ty6ubj6Yd651s8aYCjeDyKOLxgM2XvZVNL7sR/JVCTpbeFxHmxv/8m4lt93kMBBsiCzWVt0ZvP0RGUi/VX4PE5gotW6Y1l+AdfvY5fj8KaiFOTFH2E2LLCvhx9a2/GXK107//6rQOROhfjEauLr32Me6Uqw5aci6pl/xdV4XCrVsU/7o7X+4ubsx+nVzefu7v3qeBF/P9iDAU0/iIgbW6wI8o9Ndv5tleF9zX8t0Djvwh1IB4=",
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
