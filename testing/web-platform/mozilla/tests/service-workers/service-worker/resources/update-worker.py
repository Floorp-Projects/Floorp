import time

def main(request, response):
    # Set mode to 'init' for initial fetch.
    mode = 'init'
    if 'mode' in request.cookies:
        mode = request.cookies['mode'].value

    # no-cache itself to ensure the user agent finds a new version for each update.
    headers = [('Cache-Control', 'no-cache, must-revalidate'),
               ('Pragma', 'no-cache')]

    content_type = ''

    if mode == 'init':
        # Set a normal mimetype.
        # Set cookie value to 'normal' so the next fetch will work in 'normal' mode.
        content_type = 'application/javascript'
        response.set_cookie('mode', 'normal')
    elif mode == 'normal':
        # Set a normal mimetype.
        # Set cookie value to 'error' so the next fetch will work in 'error' mode.
        content_type = 'application/javascript'
        response.set_cookie('mode', 'error');
    elif mode == 'error':
        # Set a disallowed mimetype.
        # Unset and delete cookie to clean up the test setting.
        content_type = 'text/html'
        response.delete_cookie('mode')

    headers.append(('Content-Type', content_type))
    # Return a different script for each access.
    return headers, '// %s' % (time.time())

