import mozunit

from mozperftest.metrics.notebook.constant import Constant
from mozperftest.metrics.notebook.transforms.single_json import SingleJsonRetriever


def test_predefined_transformers():
    tfms = Constant().predefined_transformers
    assert SingleJsonRetriever.__name__ in tfms


if __name__ == "__main__":
    mozunit.main()
