/*
 * seccomp.hpp
 *
 *  Created on: Oct 25, 2016
 *      Author: guoger
 */

#ifndef __LINUX_SECCOMP_ISOLATOR_HPP__
#define __LINUX_SECCOMP_ISOLATOR_HPP__

#include <stout/try.hpp>

#include "slave/flags.hpp"

#include "slave/containerizer/mesos/isolator.hpp"

namespace mesos {
namespace internal {
namespace slave {

class LinuxSeccompIsolatorProcess : public MesosIsolatorProcess
{
public:
  static Try<mesos::slave::Isolator*> create(const Flags& flags);

  virtual process::Future<Option<mesos::slave::ContainerLaunchInfo>> prepare(
      const ContainerID& containerId,
      const mesos::slave::ContainerConfig& containerConfig);

  virtual process::Future<Nothing> isolate(
      const ContainerID& containerId,
      pid_t pid);

private:
  LinuxSeccompIsolatorProcess(const Flags& _flags)
    : flags(_flags) {}

  Try<mesos::SeccompInfo> parseSeccompInfo(const std::string& s);

  const Flags flags;
};

} // namespace slave {
} // namespace internal {
} // namespace mesos {

#endif // __LINUX_SECCOMP_ISOLATOR_HPP__
