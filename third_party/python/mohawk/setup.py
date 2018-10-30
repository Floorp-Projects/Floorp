from setuptools import setup, find_packages


setup(name='mohawk',
      version='0.3.4',
      description="Library for Hawk HTTP authorization",
      long_description='',
      author='Kumar McMillan, Austin King',
      author_email='kumar.mcmillan@gmail.com',
      license='MPL 2.0 (Mozilla Public License)',
      url='https://github.com/kumar303/mohawk',
      include_package_data=True,
      classifiers=[
          'Intended Audience :: Developers',
          'Natural Language :: English',
          'Operating System :: OS Independent',
          'Programming Language :: Python :: 2',
          'Programming Language :: Python :: 3',
          'Programming Language :: Python :: 2.6',
          'Programming Language :: Python :: 2.7',
          'Programming Language :: Python :: 3.3',
          'Topic :: Internet :: WWW/HTTP',
      ],
      packages=find_packages(exclude=['tests']),
      install_requires=['six'])
