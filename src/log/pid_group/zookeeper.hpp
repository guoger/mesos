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

#ifndef __LOG_PID_GROUP_ZOOKEEPER_HPP__
#define __LOG_PID_GROUP_ZOOKEEPER_HPP__

#include <list>
#include <set>
#include <string>

#include <mesos/zookeeper/group.hpp>

#include <process/executor.hpp>
#include <process/future.hpp>
#include <process/pid.hpp>
#include <process/pid_group.hpp>

#include <stout/duration.hpp>
#include <stout/option.hpp>

namespace mesos {
namespace internal {
namespace log {

class ZooKeeperPIDGroup : public process::PIDGroup
{
public:
  ZooKeeperPIDGroup(
      const std::string& servers,
      const Duration& timeout,
      const std::string& znode,
      const Option<zookeeper::Authentication>& auth)
  : observer(servers, timeout, znode, auth),
    renewer(servers, timeout, znode, auth) {}

  void initialize(const process::UPID& _base);

private:
  // Not copyable, not assignable.
  ZooKeeperPIDGroup(const ZooKeeperPIDGroup&);
  ZooKeeperPIDGroup& operator=(const ZooKeeperPIDGroup&);

  void renew(
      const process::UPID& pid,
      const std::set<zookeeper::Group::Membership>& _memberships);

  void discarded();

  void failed(const std::string& message);

  // Helper that sets up a watch on the group.
  void observe(const std::set<zookeeper::Group::Membership>& expected);

  // Invoked when the group memberships have changed.
  void observed(const process::Future<std::set<zookeeper::Group::Membership>>&);

  // Invoked when group members data has been collected.
  void collected(
      const process::Future<std::list<Option<std::string>>>& datas);

  zookeeper::Group observer;
  process::Future<std::set<zookeeper::Group::Membership>> memberships;

  // For renewing membership. We store a Group instance in order to
  // continually renew the replicas membership (when using ZooKeeper).
  zookeeper::Group renewer;
  process::Future<zookeeper::Group::Membership> membership;

  // NOTE: The declaration order here is important. We want to delete
  // the 'executor' before we delete the 'group' so that we don't get
  // spurious fatal errors when the 'group' is being deleted.
  process::Executor executor;
};

} // namespace log {
} // namespace internal {
} // namespace mesos {

#endif // __LOG_PID_GROUP_ZOOKEEPER_HPP__
