# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

XPCOM_GLUE_SRC_LCPPSRCS =        \
  AppData.cpp                    \
  nsArrayEnumerator.cpp          \
  nsArrayUtils.cpp               \
  nsCategoryCache.cpp            \
  nsCOMPtr.cpp                   \
  nsCOMArray.cpp                 \
  nsCRTGlue.cpp                  \
  nsClassInfoImpl.cpp            \
  nsComponentManagerUtils.cpp    \
  nsEnumeratorUtils.cpp          \
  nsID.cpp                       \
  nsIInterfaceRequestorUtils.cpp \
  nsINIParser.cpp                \
  nsISupportsImpl.cpp            \
  nsMemory.cpp                   \
  nsWeakReference.cpp            \
  nsVersionComparator.cpp        \
  nsTHashtable.cpp               \
  nsQuickSort.cpp                \
  nsVoidArray.cpp                \
  nsTArray.cpp                   \
  nsThreadUtils.cpp              \
  nsTObserverArray.cpp           \
  nsCycleCollectionParticipant.cpp \
  nsCycleCollectorUtils.cpp      \
  nsDeque.cpp \
  pldhash.cpp \
  FileUtils.cpp                  \
  $(NULL)

XPCOM_GLUE_SRC_CPPSRCS = $(addprefix $(topsrcdir)/xpcom/glue/, $(XPCOM_GLUE_SRC_LCPPSRCS))

XPCOM_GLUENS_SRC_LCPPSRCS =      \
  BlockingResourceBase.cpp       \
  DeadlockDetector.cpp           \
  SSE.cpp                        \
  unused.cpp                     \
  nsProxyRelease.cpp             \
  nsTextFormatter.cpp            \
  GenericFactory.cpp             \
  $(NULL)

ifneq (,$(filter arm%,$(TARGET_CPU)))
XPCOM_GLUENS_SRC_LCPPSRCS += arm.cpp
endif

XPCOM_GLUENS_SRC_CPPSRCS = $(addprefix $(topsrcdir)/xpcom/glue/,$(XPCOM_GLUENS_SRC_LCPPSRCS))
