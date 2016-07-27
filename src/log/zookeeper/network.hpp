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

#ifndef __LOG_ZOOKEEPER_NETWORK_HPP__
#define __LOG_ZOOKEEPER_NETWORK_HPP__

#include <set>

#include <mesos/log/network.hpp>
#include <mesos/zookeeper/group.hpp>

namespace mesos {
namespace internal {
namespace log {

class ZooKeeperNetwork : public mesos::log::Network
{
public:
  ZooKeeperNetwork(
      const std::string& servers,
      const Duration& timeout,
      const std::string& znode,
      const Option<zookeeper::Authentication>& auth,
      const std::set<process::UPID>& base = std::set<process::UPID>());

private:
  typedef ZooKeeperNetwork This;

  // Not copyable, not assignable.
  ZooKeeperNetwork(const ZooKeeperNetwork&);
  ZooKeeperNetwork& operator=(const ZooKeeperNetwork&);

  // Helper that sets up a watch on the group.
  void watch(const std::set<zookeeper::Group::Membership>& expected);

  // Invoked when the group memberships have changed.
  void watched(const process::Future<std::set<zookeeper::Group::Membership> >&);

  // Invoked when group members data has been collected.
  void collected(
      const process::Future<std::list<Option<std::string> > >& datas);

  zookeeper::Group group;
  process::Future<std::set<zookeeper::Group::Membership> > memberships;

  // The set of PIDs that are always in the network.
  std::set<process::UPID> base;

  // NOTE: The declaration order here is important. We want to delete
  // the 'executor' before we delete the 'group' so that we don't get
  // spurious fatal errors when the 'group' is being deleted.
  process::Executor executor;
};

} // namespace log {
} // namespace internal {
} // namespace mesos {

#endif // __LOG_ZOOKEEPER_NETWORK_HPP__
