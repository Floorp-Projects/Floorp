"""Proxxy module. Defines a Proxxy element that fetches files using local
   proxxy instances (if available). The goal of Proxxy is to lower the traffic
   from the cloud to internal servers.
"""
import urlparse
import socket
from mozharness.base.log import INFO, ERROR, LogMixin
from mozharness.base.script import ScriptMixin


# Proxxy {{{1
class Proxxy(ScriptMixin, LogMixin):
    """
    Support downloading files from HTTP caching proxies

    Current supports 'proxxy' instances, in which the caching proxy at
    proxxy.domain.com will cache requests for ftp.mozilla.org when passed requests to
    http://ftp.mozilla.org.proxxy.domain.com/...

    self.config['proxxy']['urls'] defines the list of backend hosts we are currently caching, and
    the hostname prefix to use for proxxy

    self.config['proxxy']['instances'] lists current hostnames for proxxy instances. wildcard DNS
    is set up so that *.proxxy.domain.com is a CNAME to the proxxy instance
    """

    # Default configuration. Can be overridden via self.config
    PROXXY_CONFIG = {
        "urls": [
            ('http://ftp.mozilla.org', 'ftp.mozilla.org'),
            ('https://ftp.mozilla.org', 'ftp.mozilla.org'),
            ('https://ftp-ssl.mozilla.org', 'ftp.mozilla.org'),
            ('http://pvtbuilds.pvt.build.mozilla.org', 'pvtbuilds.mozilla.org'),
            # pypi
            ('http://pypi.pvt.build.mozilla.org', 'pypi.pvt.build.mozilla.org'),
            ('http://pypi.pub.build.mozilla.org', 'pypi.pub.build.mozilla.org'),
            # taskcluster stuff
            ('https://queue.taskcluster.net', 'queue.taskcluster.net'),
        ],
        "instances": [
            'proxxy1.srv.releng.use1.mozilla.com',
            'proxxy1.srv.releng.usw2.mozilla.com',
            'proxxy1.srv.releng.scl3.mozilla.com',
        ],
        "regions": [".use1.", ".usw2.", ".scl3"],
    }

    def __init__(self, config, log_obj):
        # proxxy does not need the need the full configuration,
        # just the 'proxxy' element
        # if configuration has no 'proxxy' section use the default
        # configuration instead
        self.config = config.get('proxxy', self.PROXXY_CONFIG)
        self.log_obj = log_obj

    def get_proxies_for_url(self, url):
        """Maps url to its proxxy urls

           Args:
               url (str): url to be proxxied
           Returns:
               list: of proxy URLs to try, in sorted order.
               please note that url is NOT included in this list.
        """
        config = self.config
        urls = []

        self.info("proxxy config: %s" % config)

        proxxy_urls = config.get('urls', [])
        proxxy_instances = config.get('instances', [])

        url_parts = urlparse.urlsplit(url)
        url_path = url_parts.path
        if url_parts.query:
            url_path += "?" + url_parts.query
        if url_parts.fragment:
            url_path += "#" + url_parts.fragment

        for prefix, target in proxxy_urls:
            if url.startswith(prefix):
                self.info("%s matches %s" % (url, prefix))
                for instance in proxxy_instances:
                    if not self.query_is_proxxy_local(instance):
                        continue
                    new_url = "http://%s.%s%s" % (target, instance, url_path)
                    urls.append(new_url)

        for url in urls:
            self.info("URL Candidate: %s" % url)
        return urls

    def get_proxies_and_urls(self, urls):
        """Gets a list of urls and returns a list of proxied urls, the list
           of input urls is appended at the end of the return values

            Args:
                urls (list, tuple): urls to be mapped to proxxy urls

            Returns:
                list: proxxied urls and urls. urls are appended to the proxxied
                    urls list and they are the last elements of the list.
           """
        proxxy_list = []
        for url in urls:
            # get_proxies_for_url returns always a list...
            proxxy_list.extend(self.get_proxies_for_url(url))
        proxxy_list.extend(urls)
        return proxxy_list

    def query_is_proxxy_local(self, url):
        """Checks is url is 'proxxable' for the local instance

            Args:
                url (string): url to check

            Returns:
                bool: True if url maps to a usable proxxy,
                    False in any other case
        """
        fqdn = socket.getfqdn()
        config = self.config
        regions = config.get('regions', [])

        return any(r in fqdn and r in url for r in regions)

    def download_proxied_file(self, url, file_name, parent_dir=None,
                              create_parent_dir=True, error_level=ERROR,
                              exit_code=3):
        """
        Wrapper around BaseScript.download_file that understands proxies
        retry dict is set to 3 attempts, sleeping time 30 seconds.

            Args:
                url (string): url to fetch
                file_name (string, optional): output filename, defaults to None
                    if file_name is not defined, the output name is taken from
                    the url.
                parent_dir (string, optional): name of the parent directory
                create_parent_dir (bool, optional): if True, creates the parent
                    directory. Defaults to True
                error_level (mozharness log level, optional): log error level
                    defaults to ERROR
                exit_code (int, optional): return code to log if file_name
                    is not defined and it cannot be determined from the url
            Returns:
                string: file_name if the download has succeded, None in case of
                    error. In case of error, if error_level is set to FATAL,
                    this method interrupts the execution of the script

        """
        urls = self.get_proxies_and_urls([url])

        for url in urls:
            self.info("trying %s" % url)
            retval = self.download_file(
                url, file_name=file_name, parent_dir=parent_dir,
                create_parent_dir=create_parent_dir, error_level=ERROR,
                exit_code=exit_code,
                retry_config=dict(
                    attempts=3,
                    sleeptime=30,
                    error_level=INFO,
                ))
            if retval:
                return retval

        self.log("Failed to download from all available URLs, aborting",
                 level=error_level, exit_code=exit_code)
        return retval
