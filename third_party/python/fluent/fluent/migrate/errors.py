class MigrationError(ValueError):
    pass


class NotSupportedError(MigrationError):
    pass


class UnreadableReferenceError(MigrationError):
    pass
