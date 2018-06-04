from hglib.util import b

class handlers(object):
    """
    These can be used as the cb argument to hgclient.merge() to control the
    behaviour when Mercurial prompts what to do with regard to a specific file,
    e.g. when one parent modified a file and the other removed it.
    """

    @staticmethod
    def abort(size, output):
        """
        Abort the merge if a prompt appears.
        """
        return b('')

    """
    This corresponds to Mercurial's -y/--noninteractive global option, which
    picks the first choice on all prompts.
    """
    noninteractive = 'yes'
