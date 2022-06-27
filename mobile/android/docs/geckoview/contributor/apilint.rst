apilint release process
~~~~~~~~~~~~~~~~~~~~~~~

To release a new version of `apilint <https://github.com/mozilla-mobile/gradle-apilint>`_, do the following:

- Create a commit titled "Branch X.Y" and modify the files ``apilint/build.gradle`` and ``apilint/Config.java`` accordingly. See for example `Branch 0.5 <https://github.com/mozilla-mobile/gradle-apilint/commit/93a79ffddb8587ad018be67a361eb2a6ae777c63>`_. Note that it's not necessary to modify ``apilint/Config.java`` if there aren't any ``apidoc`` changes.

- Create a git tag with the branch version

.. code:: bash

  $ git tag X.Y

- Run tests locally by running

.. code:: bash

  $ ./gradlew build


- Publish new version to local repository

.. code:: bash

  $ ./gradlew publishToMavenLocal

- Modify ``mozilla-central`` locally to test ``apilint`` with the new version, add ``mavenLocal()`` to every ``repositories {}`` block inside the root ``build.gradle``, e.g.


.. code:: diff

  diff --git a/build.gradle b/build.gradle
  index 813ba09aa3d4b..753fdb8d958a6 100644
  --- a/build.gradle
  +++ b/build.gradle
  @@ -60,6 +60,7 @@ allprojects {
       }

       repositories {
  +        mavenLocal()
           gradle.mozconfig.substs.GRADLE_MAVEN_REPOSITORIES.each { repository ->
               maven {
                   url repository
  @@ -100,6 +101,7 @@ buildDir "${topobjdir}/gradle/build"

   buildscript {
       repositories {
  +        mavenLocal()
           gradle.mozconfig.substs.GRADLE_MAVEN_REPOSITORIES.each { repository ->
               maven {
                   url repository
  @@ -113,7 +115,7 @@ buildscript {
       ext.kotlin_version = '1.5.31'

       dependencies {
  -        classpath 'org.mozilla.apilint:apilint:0.5.2'
  +        classpath 'org.mozilla.apilint:apilint:0.X.Y'
           classpath 'com.android.tools.build:gradle:7.0.3'
           classpath 'com.getkeepsafe.dexcount:dexcount-gradle-plugin:0.8.2'
           classpath 'org.apache.commons:commons-exec:1.3'

* Test integration running ``api-lint``, this should always pass with no ``api.txt`` modifications needed (there could be exceptions, but should be intentional).

.. code:: bash

  $ ./mach lint -l android-api-lint

- Push the tag to the remote repository (note, the branch commit is `not` pushed to the main branch).

.. code:: bash

    $ git push -u origin X.Y

- Wait until github automation finishes successfully.
- (optional, if there are any ``apidoc`` changes) ask the Releng team to publish a new `apidoc` version, the bundle will be present under the github artifacts, e.g. see ``maven.zip`` in `releases/tag/0.5 <https://github.com/mozilla-mobile/gradle-apilint/releases/tag/0.5>`_. See also `Bug 1727585 <https://bugzilla.mozilla.org/show_bug.cgi?id=1727585>`_.

- Add the ``plugins.gradle.org`` keys to your ``.gradle`` folder, see `publishing_gradle_plugins.html <https://docs.gradle.org/current/userguide/publishing_gradle_plugins.html>`_.

- Publish plugin by running

.. code:: bash

  $ ./gradlew apilint:publishPlugins

- Finally, update ``mozilla-central`` to use the new version, e.g. see `this patch <https://hg.mozilla.org/mozilla-central/rev/0f746422db0e9fc6b70488bdb7114f08973191a0>`_.
