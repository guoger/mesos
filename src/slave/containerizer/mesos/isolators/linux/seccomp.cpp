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

#include <stout/os.hpp>

#include "linux/fs.hpp"

#include "slave/containerizer/mesos/isolators/linux/seccomp.hpp"

using std::set;
using std::string;

using process::Failure;
using process::Future;
using process::Owned;

using mesos::slave::ContainerConfig;
using mesos::slave::ContainerLaunchInfo;
using mesos::slave::Isolator;

namespace mesos {
namespace internal {
namespace slave {

Try<Isolator*> LinuxSeccompIsolatorProcess::create(const Flags& flags)
{
  // TODO(jay_guo) Do permission check at later point if user namespace is
  // enabled.
  if (geteuid() != 0) {
    return Error("Linux seccomp isolator requires root permissions");
  }

  // TODO(jay_guo) Check kernel support and seccomp version

  return new MesosIsolator(
      Owned<MesosIsolatorProcess>(new LinuxSeccompIsolatorProcess(flags)));
}


Future<Option<ContainerLaunchInfo>> LinuxSeccompIsolatorProcess::prepare(
    const ContainerID& containerId,
    const ContainerConfig& containerConfig)
{
  // TODO(jay_guo) Do check in slave main()
  CHECK_SOME(flags.seccomp_profile);

  ContainerLaunchInfo launchInfo;
  launchInfo.mutable_seccomp_profile()->CopyFrom(flags.seccomp_profile.get());

  return launchInfo;
}

} // namespace slave {
} // namespace internal {
} // namespace mesos {
