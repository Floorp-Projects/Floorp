import json
import logging
import requests
from redo import retry
from requests import exceptions

logger = logging.getLogger(__name__)
headers = {
    'User-Agent': 'TaskCluster'
}

# It's a list of project name which SETA is useful on
SETA_PROJECTS = ['mozilla-inbound', 'autoland']
SETA_ENDPOINT = "http://seta.herokuapp.com/data/setadetails/?branch=%s"


class SETA(object):
    """
    Interface to the SETA service, which defines low-value tasks that can be optimized out
    of the taskgraph.
    """
    def __init__(self):
        # cached low value tasks, by project
        self.low_value_tasks = {}

    def query_low_value_tasks(self, project):
        # Request the set of low value tasks from the SETA service.  Low value tasks will be
        # optimized out of the task graph.
        if project not in SETA_PROJECTS:
            logger.debug("SETA is not enabled for project `{}`".format(project))
            return []

        logger.debug("Querying SETA service for low-value tasks on {}".format(project))
        low_value_tasks = []

        url = SETA_ENDPOINT % project
        # Try to fetch the SETA data twice, falling back to an empty list of low value tasks.
        # There are 10 seconds between each try.
        try:
            logger.debug("Retrieving low-value jobs list from SETA")
            response = retry(requests.get, attempts=2, sleeptime=10,
                             args=(url, ),
                             kwargs={'timeout': 5, 'headers': headers})
            task_list = json.loads(response.content).get('jobtypes', '')
            if len(task_list) > 0:
                low_value_tasks = task_list.values()[0]

        # In the event of request times out, requests will raise a TimeoutError.
        except exceptions.Timeout:
            logger.warning("SETA server is timeout, we will treat all test tasks as high value.")

        # In the event of a network problem (e.g. DNS failure, refused connection, etc),
        # requests will raise a ConnectionError.
        except exceptions.ConnectionError:
            logger.warning("SETA server is timeout, we will treat all test tasks as high value.")

        # In the event of the rare invalid HTTP response(e.g 404, 401),
        # requests will raise an HTTPError exception
        except exceptions.HTTPError:
            logger.warning("We got bad Http response from ouija,"
                           " we will treat all test tasks as high value.")

        # We just print the error out as a debug message if we failed to catch the exception above
        except exceptions.RequestException as error:
            logger.warning(error)

        # When we get invalid JSON (i.e. 500 error), it results in a ValueError (bug 1313426)
        except ValueError as error:
            logger.warning("Invalid JSON, possible server error: {}".format(error))

        return low_value_tasks

    def is_low_value_task(self, label, project):
        # cache the low value tasks per project to avoid repeated SETA server queries
        if project not in self.low_value_tasks:
            self.low_value_tasks[project] = self.query_low_value_tasks(project)
        return label in self.low_value_tasks[project]

# create a single instance of this class, and expose its `is_low_value_task`
# bound method as a module-level function
is_low_value_task = SETA().is_low_value_task
