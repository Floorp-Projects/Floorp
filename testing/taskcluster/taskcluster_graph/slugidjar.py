from slugid import nice as slugid

class SlugidJar():
    '''
    Container of seen slugid's used to implement the as_slugid functionality
    used in the task graph templates.
    '''
    def __init__(self):
        self._names = {}

    def __call__(self, name):
        '''
        So this object can easily be passed to mustache we allow it to be called
        directly...
        '''
        if name in self._names:
            return self._names[name];

        self._names[name] = slugid()
        return self._names[name]
