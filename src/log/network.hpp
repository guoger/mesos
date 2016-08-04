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

#ifndef __NETWORK_HPP__
#define __NETWORK_HPP__

#include <list>
#include <set>
#include <string>

#include <mesos/zookeeper/group.hpp>

#include <process/collect.hpp>
#include <process/executor.hpp>
#include <process/future.hpp>
#include <process/pid.hpp>
#include <process/pid_group.hpp>

#include <stout/duration.hpp>
#include <stout/foreach.hpp>
#include <stout/lambda.hpp>
#include <stout/option.hpp>
#include <stout/set.hpp>

#include "logging/logging.hpp"

class ZooKeeperPIDGroup : public process::PIDGroup
{
public:
  ZooKeeperPIDGroup(
      const std::string& servers,
      const Duration& timeout,
      const std::string& znode,
      const Option<zookeeper::Authentication>& auth,
      const std::set<process::UPID>& base = std::set<process::UPID>());

private:
  typedef ZooKeeperPIDGroup This;

  // Not copyable, not assignable.
  ZooKeeperPIDGroup(const ZooKeeperPIDGroup&);
  ZooKeeperPIDGroup& operator=(const ZooKeeperPIDGroup&);

  // Helper that sets up a watch on the group.
  void watch(const std::set<zookeeper::Group::Membership>& expected);

  // Invoked when the group memberships have changed.
  void watched(const process::Future<std::set<zookeeper::Group::Membership>>&);

  // Invoked when group members data has been collected.
  void collected(
      const process::Future<std::list<Option<std::string>>>& datas);

  zookeeper::Group group;
  process::Future<std::set<zookeeper::Group::Membership>> memberships;

  // The set of PIDs that are always in the PID group.
  std::set<process::UPID> base;

  // NOTE: The declaration order here is important. We want to delete
  // the 'executor' before we delete the 'group' so that we don't get
  // spurious fatal errors when the 'group' is being deleted.
  process::Executor executor;
};


inline ZooKeeperPIDGroup::ZooKeeperPIDGroup(
    const std::string& servers,
    const Duration& timeout,
    const std::string& znode,
    const Option<zookeeper::Authentication>& auth,
    const std::set<process::UPID>& _base)
  : group(servers, timeout, znode, auth),
    base(_base)
{
  // PIDs from the base set are in the PID group from beginning.
  set(base);

  watch(std::set<zookeeper::Group::Membership>());
}


inline void ZooKeeperPIDGroup::watch(
    const std::set<zookeeper::Group::Membership>& expected)
{
  memberships = group.watch(expected);
  memberships
    .onAny(executor.defer(lambda::bind(&This::watched, this, lambda::_1)));
}


inline void ZooKeeperPIDGroup::watched(
    const process::Future<std::set<zookeeper::Group::Membership> >&)
{
  if (memberships.isFailed()) {
    // We can't do much here, we could try creating another Group but
    // that might just continue indefinitely, so we fail early
    // instead. Note that Group handles all retryable/recoverable
    // ZooKeeper errors internally.
    LOG(FATAL) << "Failed to watch ZooKeeper group: " << memberships.failure();
  }

  CHECK_READY(memberships);  // Not expecting Group to discard futures.

  LOG(INFO) << "ZooKeeper group memberships changed";

  // Get data for each membership in order to convert them to PIDs.
  std::list<process::Future<Option<std::string>>> futures;

  foreach (const zookeeper::Group::Membership& membership, memberships.get()) {
    futures.push_back(group.data(membership));
  }

  process::collect(futures)
    .after(Seconds(5),
           [](process::Future<std::list<Option<std::string>>> datas) {
             // Handling time outs when collecting membership
             // data. For now, a timeout is treated as a failure.
             datas.discard();
             return process::Failure("Timed out");
           })
    .onAny(executor.defer(lambda::bind(&This::collected, this, lambda::_1)));
}


inline void ZooKeeperPIDGroup::collected(
    const process::Future<std::list<Option<std::string> > >& datas)
{
  if (datas.isFailed()) {
    LOG(WARNING) << "Failed to get data for ZooKeeper group members: "
                 << datas.failure();

    // Try again later assuming empty group. Note that this does not
    // remove any of the current group members.
    watch(std::set<zookeeper::Group::Membership>());
    return;
  }

  CHECK_READY(datas);  // Not expecting collect to discard futures.

  std::set<process::UPID> pids;

  foreach (const Option<std::string>& data, datas.get()) {
    // Data could be None if the membership is gone before its
    // content can be read.
    if (data.isSome()) {
      process::UPID pid(data.get());
      CHECK(pid) << "Failed to parse '" << data.get() << "'";
      pids.insert(pid);
    }
  }

  LOG(INFO) << "ZooKeeper group PIDs: " << stringify(pids);

  // Update the PID group. We make sure that the PIDs from the base set
  // are always in the PID group.
  set(pids | base);

  watch(memberships.get());
}

#endif // __NETWORK_HPP__
