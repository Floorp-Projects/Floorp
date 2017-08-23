class CommandError(Exception):
    def __init__(self, args, ret, out, err):
        self.args = args
        self.ret = ret
        self.out = out
        self.err = err

    def __str__(self):
        return str((self.ret, self.out.rstrip(), self.err.rstrip()))

class ServerError(Exception):
    pass

class ResponseError(ServerError, ValueError):
    pass

class CapabilityError(ServerError):
    pass
