# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

from fluent.migrate import COPY_PATTERN
from fluent.migrate.helpers import transforms_from


def migrate(ctx):
    """Bug 1844783 - Use new moz-message-bar in shopping components, part {index}."""
    shopping_ftl = "browser/browser/shopping.ftl"
    ctx.add_transforms(
        shopping_ftl,
        shopping_ftl,
        transforms_from(
            """
shopping-message-bar-generic-error =
    .heading = {COPY_PATTERN(from_path, "shopping-message-bar-generic-error-title2")}
    .message = {COPY_PATTERN(from_path, "shopping-message-bar-generic-error-message")}

shopping-message-bar-warning-not-enough-reviews =
    .heading = {COPY_PATTERN(from_path, "shopping-message-bar-warning-not-enough-reviews-title")}
    .message = {COPY_PATTERN(from_path, "shopping-message-bar-warning-not-enough-reviews-message2")}

shopping-message-bar-warning-product-not-available =
    .heading = {COPY_PATTERN(from_path, "shopping-message-bar-warning-product-not-available-title")}
    .message = {COPY_PATTERN(from_path, "shopping-message-bar-warning-product-not-available-message2")}

shopping-message-bar-thanks-for-reporting =
    .heading = {COPY_PATTERN(from_path, "shopping-message-bar-thanks-for-reporting-title")}
    .message = {COPY_PATTERN(from_path, "shopping-message-bar-thanks-for-reporting-message2")}

shopping-message-bar-warning-product-not-available-reported =
    .heading = {COPY_PATTERN(from_path, "shopping-message-bar-warning-product-not-available-reported-title2")}
    .message = {COPY_PATTERN(from_path, "shopping-message-bar-warning-product-not-available-reported-message2")}

shopping-message-bar-page-not-supported =
    .heading = {COPY_PATTERN(from_path, "shopping-message-bar-page-not-supported-title")}
    .message = {COPY_PATTERN(from_path, "shopping-message-bar-page-not-supported-message")}

shopping-survey-thanks =
    .heading = {COPY_PATTERN(from_path, "shopping-survey-thanks-message")}
""",
            from_path=shopping_ftl,
        ),
    )
