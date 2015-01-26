def parse_test_opts(input_str):
    '''
    Test argument parsing is surprisingly complicated with the "restrictions"
    logic this function is responsible for parsing this out into a easier to
    work with structure like { test: '..', platforms: ['..'] }
    '''

    # Final results which we will return.
    tests = []

    cur_test = {}
    token = ''
    in_platforms = False

    def add_test(value):
        cur_test['test'] = value.strip()
        tests.insert(0, cur_test)

    def add_platform(value):
        cur_test['platforms'].insert(0, value.strip())

    # This might be somewhat confusing but we parse the string _backwards_ so
    # there is no ambiguity over what state we are in.
    for char in reversed(input_str):

        # , indicates exiting a state
        if char == ',':

            # Exit a particular platform.
            if in_platforms:
                add_platform(token)

            # Exit a particular test.
            else:
                add_test(token)
                cur_test = {}

            # Token must always be reset after we exit a state
            token = ''
        elif char == '[':
            # Exiting platform state entering test state.
            add_platform(token)
            token = ''
            in_platforms = False
        elif char == ']':
            # Entering platform state.
            in_platforms = True
            cur_test['platforms'] = []
        else:
            # Accumulator.
            token = char + token

    # Handle any left over tokens.
    if token:
        add_test(token)

    return tests
