.PHONY: buildJava
.PHONY: cleanJava
.PHONY: releaseJava

clean:: cleanJava
release_classes:: releaseJava

# always do a private_export
export:: private_export

PERL_VARIABLES=     \
    "SOURCE_PREFIX=$(SOURCE_PREFIX)" \
    "SOURCE_RELEASE_PREFIX=$(SOURCE_RELEASE_PREFIX)" \
    "SOURCE_RELEASE_CLASSES_DBG_DIR=$(SOURCE_RELEASE_CLASSES_DBG_DIR)" \
    "SOURCE_RELEASE_CLASSES_DIR=$(SOURCE_RELEASE_CLASSES_DIR)"

buildJava:
	perl build_java.pl $(PERL_VARIABLES) build

cleanJava:
	perl build_java.pl $(PERL_VARIABLES) clean

releaseJava:
	perl build_java.pl $(PERL_VARIABLES) release
