

class LibicalError(Exception):
    "Libical Error"

    def __init__(self,str):
        Exception.__init__(self,str)

    def __str__(self):
        return Exception.__str__(self)+"\nLibical errno: "+icalerror_perror()
