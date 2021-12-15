import random
import string


def main(request, response):
    r = "".join(random.choice(string.ascii_letters) for _ in range(2 * 1024 * 1024))
    return r
