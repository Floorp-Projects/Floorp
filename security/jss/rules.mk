.PHONY: buildJava
.PHONY: cleanJava
.PHONY: releaseJava

clean:: cleanJava

PERL_VARIABLES="SOURCE_PREFIX=$(SOURCE_PREFIX)"

buildJava:
	perl build_java.pl $(PERL_VARIABLES) build

cleanJava:
	perl build_java.pl $(PERL_VARIABLES) clean
