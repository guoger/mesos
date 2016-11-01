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
#include <string>

#include <mesos/mesos.hpp>

using namespace process;

namespace mesos {
namespace internal {
namespace seccomp {

extern "C" {
scmp_filter_ctx seccomp_init(uint32_t def_action);
int seccomp_reset(scmp_filter_ctx ctx, uint32_t def_action);
int seccomp_rule_add(scmp_filter_ctx ctx, uint32_t action,
                     int syscall, unsigned int arg_cnt);
int seccomp_load(scmp_filter_ctx);
uint32_t seccomp_arch_resolve_name(const char *arch_name);
int seccomp_syscall_resolve_name(const char *name);
void seccomp_release(scmp_filter_ctx ctx);
}

class ScmpFilter
{
public:
  // Create a Seccomp filter based on provided info.
  static Try<ScmpFilter> create(const SeccompInfo& scmpInfo)
  {
    // TODO(guoger) Check linux capabilities

    // Init a Seccomp context
    scmp_filter_ctx ctx = seccomp_init(
        resolveScmpAction(scmpInfo.default_action()));
    if (ctx == nullptr) {
      return Error("Failed to initialize Seccomp filter.");
    }

    // TODO(guoger) add arch

    // Add rules
    int syscallNumber, rc = -1;
    foreach (const SeccompInfo_Syscall& syscall, scmpInfo.syscalls()) {
      std::cout << "Adding Seccomp rule for syscall: '" << syscall.name()
                << "' with action '" << syscall.action() << std::endl;

      syscallNumber = seccomp_syscall_resolve_name(syscall.name().c_str());
      if (syscallNumber == __NR_SCMP_ERROR) {
        return Error("Unrecognized syscall: '" + syscall.name() + "'.");
      }

      rc = seccomp_rule_add(
          ctx,
          resolveScmpAction(syscall.action()),
          syscallNumber,
          0);
      if (rc < 0) {
        return Error("Cannot add rule for syscall: '"
            + syscall.name() + "' with action: '" + SeccompInfo_Syscall_Action_Name(syscall.action()) + "'.");
      }
    }

    return ScmpFilter(ctx);
  }


  Try<Nothing> load()
  {
    int rc = -1;
    rc = seccomp_load(scmpCtx);
    if (rc < 0) {
      return Error("Failed to load Seccomp filter.");
    }

    return Nothing();
  }


  ~ScmpFilter()
  {
    // Should release scmp ctx here but get double free problem...
//    seccomp_release(scmpCtx);
  }
private:
  explicit ScmpFilter(const scmp_filter_ctx& _scmpCtx)
    : scmpCtx(_scmpCtx)
  {
    std::cout << "In constructor" << std::endl;
  }

  // We should have valid action enum at this point,
  // so we don't do error check here.
  static uint32_t resolveScmpAction(
      const SeccompInfo_Syscall_Action& action)
  {
    switch (action) {
      case SeccompInfo_Syscall_Action_SCMP_ACT_ALLOW:
        return SCMP_ACT_ALLOW;

      case SeccompInfo_Syscall_Action_SCMP_ACT_KILL:
        return SCMP_ACT_KILL;
    }

    UNREACHABLE();
  }

  scmp_filter_ctx scmpCtx;
};

} // namespace seccomp {
} // namespace internal {
} // namespace mesos {

#endif // __LINUX_SECCOMP_HPP__
