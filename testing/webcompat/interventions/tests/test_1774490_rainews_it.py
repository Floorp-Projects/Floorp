import pytest
from helpers import Css, await_element

URL = (
    "https://www.rainews.it/photogallery/2022/06/fotorassegna-stampa-le-prime"
    "-pagine-dei-quotidiani-di-sabato-04-giugno--cd5414b3-65b9-429b-85d5-e53fadd59f4c.html"
)

PICTURE_CSS = ".swiper-slide picture"
GALLERY_CSS = Css(".swiper-wrapper")


def get_picture_width(session):
    return session.execute_script(
        f"""
        const p = document.querySelector("{PICTURE_CSS}");
        return window.getComputedStyle(p).width;
    """
    )


@pytest.mark.with_interventions
def test_enabled(session):
    session.get(URL)
    await_element(session, GALLERY_CSS)
    assert "0px" != get_picture_width(session)


@pytest.mark.without_interventions
def test_disabled(session):
    session.get(URL)
    await_element(session, GALLERY_CSS)
    assert "0px" == get_picture_width(session)
