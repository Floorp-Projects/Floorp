/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/sound/automaticallychosensoundsystem.h"
#include "webrtc/sound/nullsoundsystem.h"
#include "webrtc/base/gunit.h"

namespace rtc {

class NeverFailsToFailSoundSystem : public NullSoundSystem {
 public:
  // Overrides superclass.
  virtual bool Init() {
    return false;
  }

  static SoundSystemInterface *Create() {
    return new NeverFailsToFailSoundSystem();
  }
};

class InitCheckingSoundSystem1 : public NullSoundSystem {
 public:
  // Overrides superclass.
  virtual bool Init() {
    created_ = true;
    return true;
  }

  static SoundSystemInterface *Create() {
    return new InitCheckingSoundSystem1();
  }

  static bool created_;
};

bool InitCheckingSoundSystem1::created_ = false;

class InitCheckingSoundSystem2 : public NullSoundSystem {
 public:
  // Overrides superclass.
  virtual bool Init() {
    created_ = true;
    return true;
  }

  static SoundSystemInterface *Create() {
    return new InitCheckingSoundSystem2();
  }

  static bool created_;
};

bool InitCheckingSoundSystem2::created_ = false;

class DeletionCheckingSoundSystem1 : public NeverFailsToFailSoundSystem {
 public:
  virtual ~DeletionCheckingSoundSystem1() {
    deleted_ = true;
  }

  static SoundSystemInterface *Create() {
    return new DeletionCheckingSoundSystem1();
  }

  static bool deleted_;
};

bool DeletionCheckingSoundSystem1::deleted_ = false;

class DeletionCheckingSoundSystem2 : public NeverFailsToFailSoundSystem {
 public:
  virtual ~DeletionCheckingSoundSystem2() {
    deleted_ = true;
  }

  static SoundSystemInterface *Create() {
    return new DeletionCheckingSoundSystem2();
  }

  static bool deleted_;
};

bool DeletionCheckingSoundSystem2::deleted_ = false;

class DeletionCheckingSoundSystem3 : public NullSoundSystem {
 public:
  virtual ~DeletionCheckingSoundSystem3() {
    deleted_ = true;
  }

  static SoundSystemInterface *Create() {
    return new DeletionCheckingSoundSystem3();
  }

  static bool deleted_;
};

bool DeletionCheckingSoundSystem3::deleted_ = false;

extern const SoundSystemCreator kSingleSystemFailingCreators[] = {
  &NeverFailsToFailSoundSystem::Create,
};

TEST(AutomaticallyChosenSoundSystem, SingleSystemFailing) {
  AutomaticallyChosenSoundSystem<
      kSingleSystemFailingCreators,
      ARRAY_SIZE(kSingleSystemFailingCreators)> sound_system;
  EXPECT_FALSE(sound_system.Init());
}

extern const SoundSystemCreator kSingleSystemSucceedingCreators[] = {
  &NullSoundSystem::Create,
};

TEST(AutomaticallyChosenSoundSystem, SingleSystemSucceeding) {
  AutomaticallyChosenSoundSystem<
      kSingleSystemSucceedingCreators,
      ARRAY_SIZE(kSingleSystemSucceedingCreators)> sound_system;
  EXPECT_TRUE(sound_system.Init());
}

extern const SoundSystemCreator
    kFailedFirstSystemResultsInUsingSecondCreators[] = {
  &NeverFailsToFailSoundSystem::Create,
  &NullSoundSystem::Create,
};

TEST(AutomaticallyChosenSoundSystem, FailedFirstSystemResultsInUsingSecond) {
  AutomaticallyChosenSoundSystem<
      kFailedFirstSystemResultsInUsingSecondCreators,
      ARRAY_SIZE(kFailedFirstSystemResultsInUsingSecondCreators)> sound_system;
  EXPECT_TRUE(sound_system.Init());
}

extern const SoundSystemCreator kEarlierEntriesHavePriorityCreators[] = {
  &InitCheckingSoundSystem1::Create,
  &InitCheckingSoundSystem2::Create,
};

TEST(AutomaticallyChosenSoundSystem, EarlierEntriesHavePriority) {
  AutomaticallyChosenSoundSystem<
      kEarlierEntriesHavePriorityCreators,
      ARRAY_SIZE(kEarlierEntriesHavePriorityCreators)> sound_system;
  InitCheckingSoundSystem1::created_ = false;
  InitCheckingSoundSystem2::created_ = false;
  EXPECT_TRUE(sound_system.Init());
  EXPECT_TRUE(InitCheckingSoundSystem1::created_);
  EXPECT_FALSE(InitCheckingSoundSystem2::created_);
}

extern const SoundSystemCreator kManySoundSystemsCreators[] = {
  &NullSoundSystem::Create,
  &NullSoundSystem::Create,
  &NullSoundSystem::Create,
  &NullSoundSystem::Create,
  &NullSoundSystem::Create,
  &NullSoundSystem::Create,
  &NullSoundSystem::Create,
};

TEST(AutomaticallyChosenSoundSystem, ManySoundSystems) {
  AutomaticallyChosenSoundSystem<
      kManySoundSystemsCreators,
      ARRAY_SIZE(kManySoundSystemsCreators)> sound_system;
  EXPECT_TRUE(sound_system.Init());
}

extern const SoundSystemCreator kDeletesAllCreatedSoundSystemsCreators[] = {
  &DeletionCheckingSoundSystem1::Create,
  &DeletionCheckingSoundSystem2::Create,
  &DeletionCheckingSoundSystem3::Create,
};

TEST(AutomaticallyChosenSoundSystem, DeletesAllCreatedSoundSystems) {
  typedef AutomaticallyChosenSoundSystem<
      kDeletesAllCreatedSoundSystemsCreators,
      ARRAY_SIZE(kDeletesAllCreatedSoundSystemsCreators)> TestSoundSystem;
  TestSoundSystem *sound_system = new TestSoundSystem();
  DeletionCheckingSoundSystem1::deleted_ = false;
  DeletionCheckingSoundSystem2::deleted_ = false;
  DeletionCheckingSoundSystem3::deleted_ = false;
  EXPECT_TRUE(sound_system->Init());
  delete sound_system;
  EXPECT_TRUE(DeletionCheckingSoundSystem1::deleted_);
  EXPECT_TRUE(DeletionCheckingSoundSystem2::deleted_);
  EXPECT_TRUE(DeletionCheckingSoundSystem3::deleted_);
}

}  // namespace rtc
