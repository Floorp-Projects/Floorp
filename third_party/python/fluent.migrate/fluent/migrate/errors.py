class SkipTransform(RuntimeError):
    pass


class MigrationError(ValueError):
    pass


class EmptyLocalizationError(MigrationError):
    pass


class NotSupportedError(MigrationError):
    pass


class UnreadableReferenceError(MigrationError):
    pass


class InvalidTransformError(MigrationError):
    pass
