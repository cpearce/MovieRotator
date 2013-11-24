// Copyright 2013  Chris Pearce
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//    http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#pragma once

#include "stdafx.h"

// Creates and registers a class factory that creates the H.264 encoder.
// This is so that we can intercept the creation of the H.264 encoder and
// crank up it's quality, as we can't do this once the sink writer has had
// it's media type set.
class AutoRegisterH264ClassFactory {
public:
  AutoRegisterH264ClassFactory();
  ~AutoRegisterH264ClassFactory();
private:
  IClassFactoryPtr mClassFactory;
};
