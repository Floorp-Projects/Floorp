config = {
    "upload_environment": "prod",
    "upload_env": {
        "UPLOAD_USER": "ffxbld",
        # ssh_key_dir is defined per platform: it is "~/.ssh" for every platform
        # except when mock is in use, in this case, ssh_key_dir is
        # /home/mock_mozilla/.ssh
        "UPLOAD_SSH_KEY": "%(ssh_key_dir)s/ffxbld_rsa",
        "UPLOAD_HOST": "upload.ffxbld.productdelivery.prod.mozaws.net",
        "POST_UPLOAD_CMD": "post_upload.py -b %(branch)s-l10n -p %(stage_product)s -i %(buildid)s --release-to-latest --release-to-dated",
        "UPLOAD_TO_TEMP": "1"
    },
    'taskcluster_index': 'index',
}
