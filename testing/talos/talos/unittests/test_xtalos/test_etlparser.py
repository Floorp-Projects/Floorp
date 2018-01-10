from __future__ import absolute_import

import re

import mozunit

from talos.xtalos.etlparser import NAME_SUBSTITUTIONS


def test_NAME_SUBSTITUTIONS():
    filepaths_map = {
        # tp5n files
        r'{talos}\talos\tests\tp5n\alibaba.com\i03.i.aliimg.com\images\eng\style\css_images':
            r'{talos}\talos\tests\{tp5n_files}',
        r'{talos}\talos\tests\tp5n\cnet.com\i.i.com.com\cnwk.1d\i\tron\fd':
            r'{talos}\talos\tests\{tp5n_files}',
        r'{talos}\talos\tests\tp5n\tp5n.manifest':
            r'{talos}\talos\tests\{tp5n_files}',
        r'{talos}\talos\tests\tp5n\tp5n.manifest.develop':
            r'{talos}\talos\tests\{tp5n_files}',
        r'{talos}\talos\tests\tp5n\yelp.com\media1.ct.yelpcdn.com\photo':
            r'{talos}\talos\tests\{tp5n_files}',

        # cltbld for Windows 7 32bit
        r'c:\users\cltbld.t-w732-ix-015.000\appdata\locallow\mozilla':
            r'c:\users\{cltbld}\appdata\locallow\mozilla',
        r'c:\users\cltbld.t-w732-ix-035.000\appdata\locallow\mozilla':
            r'c:\users\{cltbld}\appdata\locallow\mozilla',
        r'c:\users\cltbld.t-w732-ix-058.000\appdata\locallow\mozilla':
            r'c:\users\{cltbld}\appdata\locallow\mozilla',
        r'c:\users\cltbld.t-w732-ix-112.001\appdata\local\temp':
            r'c:\users\{cltbld}\appdata\local\temp',

        # nvidia's 3D Vision
        r'c:\program files\nvidia corporation\3d vision\npnv3dv.dll':
            r'c:\program files\{nvidia_3d_vision}',
        r'c:\program files\nvidia corporation\3d vision\npnv3dvstreaming.dll':
            r'c:\program files\{nvidia_3d_vision}',
        r'c:\program files\nvidia corporation\3d vision\nvstereoapii.dll':
            r'c:\program files\{nvidia_3d_vision}',

        r'{firefox}\browser\extensions\{45b6d270-f6ec-4930-a6ad-14bac5ea2204}.xpi':
            r'{firefox}\browser\extensions\{uuid}.xpi',

        r'c:\slave\test\build\venv\lib\site-packages\pip\_vendor\html5lib\treebuilders':
            r'c:\slave\test\build\venv\lib\site-packages\{pip_vendor}',
        r'c:\slave\test\build\venv\lib\site-packages\pip\_vendor\colorama':
            r'c:\slave\test\build\venv\lib\site-packages\{pip_vendor}',
        r'c:\slave\test\build\venv\lib\site-packages\pip\_vendor\cachecontrol\caches':
            r'c:\slave\test\build\venv\lib\site-packages\{pip_vendor}',
        r'c:\slave\test\build\venv\lib\site-packages\pip\_vendor\requests\packages\urllib3'
        r'\packages\ssl_match_hostname':
            r'c:\slave\test\build\venv\lib\site-packages\{pip_vendor}',
    }

    for given_raw_path, exp_normal_path in filepaths_map.items():
        normal_path = given_raw_path
        for pattern, substitution in NAME_SUBSTITUTIONS:
            normal_path = re.sub(pattern, substitution, normal_path)
        assert exp_normal_path == normal_path


if __name__ == '__main__':
    mozunit.main()
