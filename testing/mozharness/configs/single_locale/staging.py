config = {
    "upload_environment": "stage",
    'post_upload_extra': ['--bucket-prefix', 'net-mozaws-stage-delivery',
                          '--url-prefix', 'http://ftp.stage.mozaws.net/',
                          ],
}
