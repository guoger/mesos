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

#include "network.hpp"

#include <process/collect.hpp>

#include <stout/set.hpp>

namespace mesos {
namespace internal {
namespace log {

ZooKeeperNetwork::ZooKeeperNetwork(
    const std::string& servers,
    const Duration& timeout,
    const std::string& znode,
    const Option<zookeeper::Authentication>& auth,
    const std::set<process::UPID>& _base)
  : group(servers, timeout, znode, auth),
    base(_base)
{
  // PIDs from the base set are in the network from beginning.
  set(base);

  watch(std::set<zookeeper::Group::Membership>());
}


void ZooKeeperNetwork::watch(
    const std::set<zookeeper::Group::Membership>& expected)
{
  memberships = group.watch(expected);
  memberships
    .onAny(executor.defer(lambda::bind(&This::watched, this, lambda::_1)));
}


void ZooKeeperNetwork::watched(
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
  std::list<process::Future<Option<std::string> > > futures;

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


void ZooKeeperNetwork::collected(
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

  // Update the network. We make sure that the PIDs from the base set
  // are always in the network.
  set(pids | base);

  watch(memberships.get());
}

} // namespace log {
} // namespace internal {
} // namespace mesos {
