import datetime
from from_now import value_of

def json_time_from_now(input_str, now=None):
    '''
    :param str input_str: Input string (see value of)
    :param datetime now: Optionally set the definition of `now`
    :returns: JSON string representation of time in future.
    '''

    if now is None:
        now = datetime.date.fromordinal(1)

    time = now + value_of(input_str)

    # Sorta a big hack but the json schema validator for date does not like the
    # ISO dates until 'Z' (for timezone) is added...
    return time.isoformat() + 'Z'

def current_json_time():
    '''
    :returns: JSON string representation of the current time.
    '''

    return datetime.date.fromordinal(1).isoformat() + 'Z'

def slugid():
    return 'abcdef123456'
