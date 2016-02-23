import os


def pop_aws_auth_from_env():
    """
    retrieves aws creds and deletes them from os.environ if present.
    """
    aws_key_id = os.environ.pop("AWS_ACCESS_KEY_ID", None)
    aws_secret_key = os.environ.pop("AWS_SECRET_ACCESS_KEY", None)

    return aws_key_id, aws_secret_key
