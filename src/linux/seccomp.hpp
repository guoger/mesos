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

#include <process/future.hpp>

#include <seccomp.h>

#include <mesos/mesos.hpp>

using namespace process;

namespace mesos {
namespace internal {
namespace seccomp {

extern "C" {
void* seccomp_init(uint32_t def_action);
int seccomp_reset(void* ctx, uint32_t def_action);
int seccomp_rule_add(void* ctx, uint32_t action,
                     int syscall, unsigned int arg_cnt);
int seccomp_load(void*);
}

class SeccompProfile
{
public:
  SeccompProfile(const SeccompInfo& _seccompInfo) : seccompInfo(_seccompInfo) {}

  Try<int> set()
  {
    void* ctx = seccomp_init(SCMP_ACT_ALLOW);
    if (ctx == nullptr) {
      std::cout << "##### oh crap!" << std::endl;
      return Error("Couldn't create seccomp ctx!");
    }

    std::cout << "##### created ctx!" << std::endl;

    int rc = -1;
    rc = seccomp_rule_add(ctx, SCMP_ACT_KILL, SCMP_SYS(acct), 0);
    if (rc < 0) {
      std::cout << "Failed to add rule" << std::endl;
      return Error("Failed to add rule!");
    }
    std::cout << "rule added" << std::endl;

    rc = seccomp_load(ctx);
    if (rc < 0) {
      std::cout << "Failed to load rule" << std::endl;
      return Error("Failed to load rule!");
    }
    std::cout << "rule loaded" << std::endl;
    return 1;
  }

private:
  const SeccompInfo seccompInfo;
};

} // namespace seccomp {
} // namespace internal {
} // namespace mesos {

#endif // __LINUX_SECCOMP_HPP__
