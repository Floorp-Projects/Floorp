from __future__ import absolute_import, print_function

import base64
import hashlib
import re
import time

from mitmproxy import ctx


class AddDeterministic():

    def get_csp_directives(self, headers):
        csp = headers.get("Content-Security-Policy", "")
        return [d.strip() for d in csp.split(";")]

    def get_csp_script_sources(self, headers):
        sources = []
        for directive in self.get_csp_directives(headers):
            if directive.startswith("script-src "):
                sources = directive.split()[1:]
        return sources

    def get_nonce_from_headers(self, headers):
        """
        get_nonce_from_headers returns the nonce token from a
        Content-Security-Policy (CSP) header's script source directive.

        Note:
        For more background information on CSP and nonce, please refer to
        https://developer.mozilla.org/en-US/docs/Web/HTTP/Headers/
        Content-Security-Policy/script-src
        https://developers.google.com/web/fundamentals/security/csp/
        """

        for source in self.get_csp_script_sources(headers) or []:
            if source.startswith("'nonce-"):
                return source.partition("'nonce-")[-1][:-1]

    def get_script_with_nonce(self, script, nonce=None):
        """
        Given a nonce, get_script_with_nonce returns the injected script text with the nonce.

        If nonce None, get_script_with_nonce returns the script block
        without attaching a nonce attribute.

        Note:
        Some responses may specify a nonce inside their Content-Security-Policy,
        script-src directive.
        The script injector needs to set the injected script's nonce attribute to
        open execute permission for the injected script.
        """

        if nonce:
            return '<script nonce="{}">{}</script>'.format(nonce, script)
        return '<script>{}</script>'.format(script)

    def update_csp_script_src(self, headers, sha256):
        """
        Update the CSP script directives with appropriate information

        Without this permissions a page with a
        restrictive CSP will not execute injected scripts.

        Note:
        For more background information on CSP, please refer to
        https://developer.mozilla.org/en-US/docs/Web/HTTP/Headers/
        Content-Security-Policy/script-src
        https://developers.google.com/web/fundamentals/security/csp/
        """

        sources = self.get_csp_script_sources(headers)
        add_unsafe = True

        for token in sources:
            if token == "'unsafe-inline'":
                add_unsafe = False
                ctx.log.info("Contains unsafe-inline")
            elif token.startswith("'sha"):
                sources.append("'sha256-{}'".format(sha256))
                add_unsafe = False
                ctx.log.info("Add sha hash directive")
                break

        if add_unsafe:
            ctx.log.info("Add unsafe")
            sources.append("'unsafe-inline'")

        return "script-src {}".format(" ".join(sources))

    def get_new_csp_header(self, headers, updated_csp_script):
        """
        get_new_csp_header generates a new header object containing
        the updated elements from new_csp_script_directives
        """

        if updated_csp_script:
            directives = self.get_csp_directives(headers)
            for index, directive in enumerate(directives):
                if directive.startswith("script-src "):
                    directives[index] = updated_csp_script

            ctx.log.info("Original Header %s \n" % headers["Content-Security-Policy"])
            headers["Content-Security-Policy"] = "; ".join(directives)
            ctx.log.info("Updated  Header %s \n" % headers["Content-Security-Policy"])

        return headers

    def response(self, flow):

        millis = int(round(time.time() * 1000))

        if "content-type" in flow.response.headers:
            if 'text/html' in flow.response.headers["content-type"]:
                ctx.log.info("Working on {}".format(flow.response.headers["content-type"]))

                flow.response.decode()
                html = flow.response.text

                with open("scripts/catapult/deterministic.js", "r") as jsfile:
                    js = jsfile.read().replace("REPLACE_LOAD_TIMESTAMP", str(millis))

                    if js not in html:
                        script_index = re.search('(?i).*?<head.*?>', html)
                        if script_index is None:
                            script_index = re.search('(?i).*?<html.*?>', html)
                        if script_index is None:
                            script_index = re.search('(?i).*?<!doctype html>', html)
                        if script_index is None:
                            ctx.log.info("No start tags found in request {}. Skip injecting".
                                         format(flow.request.url))
                            return
                        script_index = script_index.end()

                        nonce = None

                        if flow.response.headers.get("Content-Security-Policy", False):
                            nonce = self.get_nonce_from_headers(flow.response.headers)
                            ctx.log.info("nonce : %s" % nonce)

                            if self.get_csp_script_sources(flow.response.headers) and not nonce:
                                # generate sha256 for the script
                                hash_object = hashlib.sha256(js.encode('utf-8'))
                                script_sha256 = base64.b64encode(hash_object.digest()). \
                                    decode("utf-8")

                                # generate the new response headers
                                updated_script_sources = self.update_csp_script_src(
                                    flow.response.headers,
                                    script_sha256)
                                flow.response.headers = self.get_new_csp_header(
                                    flow.response.headers,
                                    updated_script_sources)

                        # generate new html file
                        new_html = html[:script_index] + \
                            self.get_script_with_nonce(js, nonce) + \
                            html[script_index:]
                        flow.response.text = new_html

                        ctx.log.info("In request {} injected deterministic JS".
                                     format(flow.request.url))
                    else:
                        ctx.log.info("Script already injected in request {}".
                                     format(flow.request.url))


def start():
    ctx.log.info("Load Deterministic JS")
    return AddDeterministic()
