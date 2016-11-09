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

#include <errno.h>
#include <seccomp.h>
#include <string>
#include <vector>

#include <mesos/mesos.hpp>

using namespace process;

using std::cerr;
using std::cout;
using std::endl;
using std::string;
using std::vector;

namespace mesos {
namespace internal {
namespace seccomp {

#define MAX_ARGS_NUMBER 6

extern "C" {
scmp_filter_ctx seccomp_init(uint32_t def_action);

int seccomp_reset(scmp_filter_ctx ctx, uint32_t def_action);

int seccomp_rule_add(
    scmp_filter_ctx ctx,
    uint32_t action,
    int syscall,
    unsigned int arg_cnt);

int seccomp_load(scmp_filter_ctx);

int seccomp_syscall_resolve_name(const char *name);

void seccomp_release(scmp_filter_ctx ctx);

int seccomp_arch_add(scmp_filter_ctx ctx, uint32_t arch_token);

int seccomp_arch_exist(const scmp_filter_ctx ctx, uint32_t arch_token);

int seccomp_rule_add_array(
    scmp_filter_ctx ctx,
    uint32_t action,
    int syscall,
    unsigned int arg_cnt,
    const struct scmp_arg_cmp *arg_array);

int seccomp_attr_set(
    scmp_filter_ctx ctx,
    enum scmp_filter_attr attr,
    uint32_t value);
}


class ScmpFilter
{
public:
  // Create a Seccomp filter based on provided info.
  static Try<Owned<ScmpFilter>> create(const SeccompInfo& scmpInfo)
  {
    // Init a Seccomp context
    scmp_filter_ctx ctx = seccomp_init(
        resolveScmpAction(scmpInfo.default_action()));
    if (ctx == nullptr) {
      return Error("Failed to initialize Seccomp filter");
    }

    // Add arch
    int rc;
    foreach (const int& arch, scmpInfo.architectures()) {
      rc = -1;
      string archName = SeccompInfo_Architecture_Name(
          static_cast<SeccompInfo_Architecture>(arch));

      rc = seccomp_arch_add(ctx, resolveScmpArch(arch));
      if (rc != 0) {
        if (rc == -EEXIST) {
          // Libseccomp returns -EEXIST if requested arch is already persent.
          // However we simply ignore this since it's not fatal.
          cerr << "Architecture '" << archName
               << "' is already present, skip" << endl;
          continue;
        } else {
          seccomp_release(ctx);
          return Error("Failed to add Seccomp architecture '" + archName + "'");
        }
      }

      cout << "Added architecture '" << archName << "'" << endl;
    }

    // TODO(jay_guo) To install a filter, one of following must be met:
    // * Caller is privileged (CAP_SYS_ADMIN)
    // * Caller has to set no_new_privs process attribute (There is actually
    //   a filter attr to help setting this: SCMP_FLTATR_CTL_NNP)
    // In Docker, this attr is explicitly turned off, assuming
    // the Docker daemon has CAP_SYS_ADMIN (previleged Docker daemon).
    // However mesos-containerizer is run under non-root user, therefore not
    // meeting the first requirement. As a consequence, we need to set
    // no_new_privs here (most likely it's already set). But we need to think of
    // negative effects and corner cases.
    rc = -1;
    rc = seccomp_attr_set(ctx, SCMP_FLTATR_CTL_NNP, 1);
    if (rc < 0) {
      seccomp_release(ctx);
      return Error("Failed to set no_new_privs bit");
    }

    // Add rules
    foreach (const SeccompInfo_Syscall& syscall, scmpInfo.syscalls()) {
      rc = -1;
      string actionName = SeccompInfo_Syscall_Action_Name(syscall.action());
      cout << "Adding Seccomp rule for syscall '" << syscall.name()
           << "' with action '" << actionName << "'" << endl;

      int syscallNumber = seccomp_syscall_resolve_name(syscall.name().c_str());
      if (syscallNumber == __NR_SCMP_ERROR) {
        seccomp_release(ctx);
        return Error("Unrecognized syscall '" + syscall.name() + "'");
      }

      if (syscall.args_size() > 0) {
        // There are arguments associated with this syscall rule.
        if (syscall.args_size() > MAX_ARGS_NUMBER) {
          seccomp_release(ctx);
          return Error("Max number of arguments allowed is " + MAX_ARGS_NUMBER);
        }

        vector<scmp_arg_cmp> scmp_args;
        foreach (const SeccompInfo_Syscall_Arg& scmp_arg, syscall.args()) {
          scmp_arg_cmp arg = {
            scmp_arg.index(),
            resolveScmpOperator(scmp_arg.op()),
            scmp_arg.value(),
            scmp_arg.value_two()
          };
          scmp_args.push_back(arg);
        }

        rc = seccomp_rule_add_array(
            ctx,
            resolveScmpAction(syscall.action()),
            syscallNumber,
            syscall.args_size(),
            &scmp_args[0]);
      } else {
        // There is no argument associated to this syscall rule.
        rc = seccomp_rule_add(
            ctx,
            resolveScmpAction(syscall.action()),
            syscallNumber,
            0);
      }

      if (rc < 0) {
        seccomp_release(ctx);
        return Error("Cannot add rule for syscall '" + syscall.name() +
            " with action '" + actionName + "'");
      }
    }

    return new ScmpFilter(ctx);
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
    seccomp_release(scmpCtx);
  }

private:
  explicit ScmpFilter(const scmp_filter_ctx& _scmpCtx) : scmpCtx(_scmpCtx) {}

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

      case SeccompInfo_Syscall_Action_SCMP_ACT_ERRNO:
        return SCMP_ACT_ERRNO(EPERM); // Use EPERM as return code

      case SeccompInfo_Syscall_Action_SCMP_ACT_TRACE:
        return SCMP_ACT_TRACE(EPERM); // Use EPERM as return code

      case SeccompInfo_Syscall_Action_SCMP_ACT_TRAP:
        return SCMP_ACT_TRAP;
    }

    UNREACHABLE();
  }

  static uint32_t resolveScmpArch(const int& arch)
  {
    switch (arch) {
      case SeccompInfo_Architecture_SCMP_ARCH_X86:
        return SCMP_ARCH_X86;

      case SeccompInfo_Architecture_SCMP_ARCH_X86_64:
        return SCMP_ARCH_X86_64;

      case SeccompInfo_Architecture_SCMP_ARCH_X32:
        return SCMP_ARCH_X32;

      case SeccompInfo_Architecture_SCMP_ARCH_ARM:
        return SCMP_ARCH_ARM;

      case SeccompInfo_Architecture_SCMP_ARCH_AARCH64:
        return SCMP_ARCH_AARCH64;

      case SeccompInfo_Architecture_SCMP_ARCH_MIPS:
        return SCMP_ARCH_MIPS;

      case SeccompInfo_Architecture_SCMP_ARCH_MIPSEL:
        return SCMP_ARCH_MIPSEL;

      case SeccompInfo_Architecture_SCMP_ARCH_MIPS64:
        return SCMP_ARCH_MIPS64;

      case SeccompInfo_Architecture_SCMP_ARCH_MIPSEL64:
        return SCMP_ARCH_MIPSEL64;

      case SeccompInfo_Architecture_SCMP_ARCH_MIPS64N32:
        return SCMP_ARCH_MIPS64N32;

      case SeccompInfo_Architecture_SCMP_ARCH_MIPSEL64N32:
        return SCMP_ARCH_MIPSEL64N32;

      case SeccompInfo_Architecture_SCMP_ARCH_PPC:
        return SCMP_ARCH_PPC;

      case SeccompInfo_Architecture_SCMP_ARCH_PPC64:
        return SCMP_ARCH_PPC64;

      case SeccompInfo_Architecture_SCMP_ARCH_PPC64LE:
        return SCMP_ARCH_PPC64LE;

      case SeccompInfo_Architecture_SCMP_ARCH_S390:
        return SCMP_ARCH_S390;

      case SeccompInfo_Architecture_SCMP_ARCH_S390X:
        return SCMP_ARCH_S390X;
    }

    UNREACHABLE();
  }

  static scmp_compare resolveScmpOperator(
      const SeccompInfo_Syscall_Arg_Operator& op)
  {
    switch (op) {
      case SeccompInfo_Syscall_Arg_Operator_SCMP_CMP_NE:
        return SCMP_CMP_NE;

      case SeccompInfo_Syscall_Arg_Operator_SCMP_CMP_LT:
        return SCMP_CMP_LT;

      case SeccompInfo_Syscall_Arg_Operator_SCMP_CMP_LE:
        return SCMP_CMP_LE;

      case SeccompInfo_Syscall_Arg_Operator_SCMP_CMP_EQ:
        return SCMP_CMP_EQ;

      case SeccompInfo_Syscall_Arg_Operator_SCMP_CMP_GE:
        return SCMP_CMP_GE;

      case SeccompInfo_Syscall_Arg_Operator_SCMP_CMP_GT:
        return SCMP_CMP_GT;

      case SeccompInfo_Syscall_Arg_Operator_SCMP_CMP_MASKED_EQ:
        return SCMP_CMP_MASKED_EQ;
    }

    UNREACHABLE();
  }

  scmp_filter_ctx scmpCtx;
};

} // namespace seccomp {
} // namespace internal {
} // namespace mesos {

#endif // __LINUX_SECCOMP_HPP__
