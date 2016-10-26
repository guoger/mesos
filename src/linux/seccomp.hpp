// Licensed to the Apache Software Foundation (ASF) under one
// or more contributor license agreements.  See the NOTICE file
// distributed with this work for additional information
// regarding copyright ownership.  The ASF licenses this file
// to you under the Apache License, Version 2.0 (the
// "License"); you may not use this file except in compliance
// with the License.  You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef __LINUX_SECCOMP_HPP__
#define __LINUX_SECCOMP_HPP__

#include <seccomp.h>

namespace mesos {
namespace internal {
namespace seccomp {

extern "C" {
extern void* seccomp_init(uint32_t def_action);
extern int seccomp_reset(void* ctx, uint32_t def_action);
}

class SeccompProfile
{
public:
  SeccompProfile() {}

  void set()
  {
    void* ctx = seccomp_init(SCMP_ACT_KILL);
    if (ctx == nullptr) {
      std::cout << "##### oh crap!" << std::endl;
    }

    std::cout << "##### created ctx!" << std::endl;

    int rc = -1;
    rc = seccomp_reset(ctx, SCMP_ACT_KILL);
    if (rc < 0) {
      std::cout << "##### oh crapppp!" << std::endl;
    }

    std::cout << "##### deleted ctx!" << std::endl;
  }
};

} // namespace seccomp {
} // namespace internal {
} // namespace mesos {

#endif // __LINUX_SECCOMP_HPP__
