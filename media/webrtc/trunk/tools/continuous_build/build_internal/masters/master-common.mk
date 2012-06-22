# -*- makefile -*-

# This file is a copy of the Chromium master-common.mk located at
# /trunk/tools/build/masters of the Chromium tools. It is just modified to add
# the PRIVATESCRIPTS_DIR property.

# This should be included by a makefile which lives in a buildmaster/buildslave
# directory (next to the buildbot.tac file). That including makefile *must*
# define MASTERPATH.

# The 'start' and 'stop' targets start and stop the buildbot master.
# The 'reconfig' target will tell a buildmaster to reload its config file.

# Note that a relative PYTHONPATH entry is relative to the current directory.

# Confirm that MASTERPATH has been defined.
ifeq ($(MASTERPATH),)
  $(error MASTERPATH not defined.)
endif

# On the Mac, the buildbot is started via the launchd mechanism as a
# LaunchAgent to give the slave a proper Mac UI environment for tests.  In
# order for this to work, the plist must be present and loaded by launchd, and
# the user must be logged in to the UI.  The plist is loaded by launchd at user
# login (and the job may have been initially started at that time too).  Our
# Mac build slaves are all set up this way, and have auto-login enabled, so
# "make start" should just work and do the right thing.
#
# When using launchd to start the job, it also needs to be used to stop the
# job.  Otherwise, launchd might try to restart the job when stopped manually
# by SIGTERM.  Using SIGHUP for reconfig is safe with launchd.
#
# Because it's possible to have more than one slave on a machine (for testing),
# this tests to make sure that the slave is in the known slave location,
# /b/slave, which is what the LaunchAgent operates on.
USE_LAUNCHD := \
  $(shell [ -f ~/Library/LaunchAgents/org.chromium.buildbot.$(MASTERPATH).plist ] && \
          [ "$$(pwd -P)" = "/b/build/masters/$(MASTERPATH)" ] && \
          echo 1)

# Elements used to construct PYTHONPATH. These may be overridden by the
# including Makefile.
#
# For example: while we transition from buildbot 0.7.12 to buildbot 0.8.x ,
# some masters will override BUILDBOT_PATH in their local Makefiles.
TOPLEVEL_DIR ?= ../../../build
THIRDPARTY_DIR ?= $(TOPLEVEL_DIR)/third_party
SCRIPTS_DIR ?= $(TOPLEVEL_DIR)/scripts
PUBLICCONFIG_DIR ?= $(TOPLEVEL_DIR)/site_config
PRIVATECONFIG_DIR ?= $(TOPLEVEL_DIR)/../build_internal/site_config
PRIVATESCRIPTS_DIR ?= $(TOPLEVEL_DIR)/../build_internal/scripts

# Packages needed by buildbot7
BUILDBOT7_PATH = $(THIRDPARTY_DIR)/buildbot_7_12:$(THIRDPARTY_DIR)/twisted_8_1

# Packages needed by buildbot8
BUILDBOT8_DEPS :=               \
    buildbot_8_4p1              \
    twisted_10_2                \
    jinja2                      \
    sqlalchemy_0_7_1            \
    sqlalchemy_migrate_0_7_1    \
    tempita_0_5                 \
    decorator_3_3_1

nullstring :=
space := $(nullstring) #
BUILDBOT8_PATH = $(subst $(space),:,$(BUILDBOT8_DEPS:%=$(THIRDPARTY_DIR)/%))

# Default to buildbot7.  To override, put this in the master's Makefile:
#   BUILDBOT_PATH = $(BUILDBOT8_PATH)
BUILDBOT_PATH ?= $(BUILDBOT7_PATH)

PYTHONPATH := $(BUILDBOT_PATH):$(SCRIPTS_DIR):$(THIRDPARTY_DIR):$(PUBLICCONFIG_DIR):$(PRIVATECONFIG_DIR):$(PRIVATESCRIPTS_DIR):.

ifeq ($(BUILDBOT_PATH),$(BUILDBOT8_PATH))
start: upgrade
else
start:
endif
ifneq ($(USE_LAUNCHD),1)
	PYTHONPATH=$(PYTHONPATH) python $(SCRIPTS_DIR)/common/twistd --no_save -y buildbot.tac
else
	launchctl start org.chromium.buildbot.$(MASTERPATH)
endif

ifeq ($(BUILDBOT_PATH),$(BUILDBOT8_PATH))
start-prof: upgrade
else
start-prof:
endif
ifneq ($(USE_LAUNCHD),1)
	TWISTD_PROFILE=1 PYTHONPATH=$(PYTHONPATH) python $(SCRIPTS_DIR)/common/twistd --no_save -y buildbot.tac
else
	launchctl start org.chromium.buildbot.$(MASTERPATH)
endif

stop:
ifneq ($(USE_LAUNCHD),1)
	if `test -f twistd.pid`; then kill `cat twistd.pid`; fi;
else
	launchctl stop org.chromium.buildbot.$(MASTERPATH)
endif

reconfig:
	kill -HUP `cat twistd.pid`

log:
	tail -F twistd.log

wait:
	while `test -f twistd.pid`; do sleep 1; done;

restart: stop wait start log

restart-prof: stop wait start-prof log

# This target is only known to work on 0.8.x masters.
upgrade:
	@[ -e '.dbconfig' ] || [ -e 'state.sqlite' ] || PYTHONPATH=$(PYTHONPATH) python buildbot upgrade-master .

setup:
	@echo export PYTHONPATH=$(PYTHONPATH)
