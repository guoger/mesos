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

#ifndef __PID_GROUP_ZOOKEEPER_HPP__
#define __PID_GROUP_ZOOKEEPER_HPP__

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

#include "log/pid_group/zookeeper.hpp"

#include "logging/logging.hpp"

namespace mesos {
namespace internal {
namespace log {

void ZooKeeperPIDGroup::initialize(const process::UPID& _base)
{
  base = {_base};

  // PIDs from the base set are in the network from beginning.
  set(base);

  observe(std::set<zookeeper::Group::Membership>());

  membership = renewer.join(_base)
    .onFailed(executor.defer(
        lambda::bind(&ZooKeeperPIDGroup::failed, this, lambda::_1)))
    .onDiscarded(executor.defer(
        lambda::bind(&ZooKeeperPIDGroup::discarded, this)));

  renewer.watch()
    .onReady(executor.defer(
        lambda::bind(&ZooKeeperPIDGroup::renew, this, _base, lambda::_1)))
    .onFailed(executor.defer(
        lambda::bind(&ZooKeeperPIDGroup::failed, this, lambda::_1)))
    .onDiscarded(executor.defer(
        lambda::bind(&ZooKeeperPIDGroup::discarded, this)));
}


void ZooKeeperPIDGroup::renew(
    const process::UPID& pid,
    const std::set<zookeeper::Group::Membership>& _memberships)
{
  if (membership.isReady() && _memberships.count(membership.get()) == 0) {
    // Our replica's membership must have expired, join back up.
    LOG(INFO) << "Renewing replica group membership";

    membership = renewer.join(pid)
      .onFailed(executor.defer(
          lambda::bind(&ZooKeeperPIDGroup::failed, this, lambda::_1)))
      .onDiscarded(executor.defer(
          lambda::bind(&ZooKeeperPIDGroup::discarded, this)));
  }

  renewer.watch(_memberships)
    .onReady(executor.defer(
        lambda::bind(&ZooKeeperPIDGroup::renew, this, pid, lambda::_1)))
    .onFailed(executor.defer(
        lambda::bind(&ZooKeeperPIDGroup::failed, this, lambda::_1)))
    .onDiscarded(executor.defer(
        lambda::bind(&ZooKeeperPIDGroup::discarded, this)));
}


void ZooKeeperPIDGroup::observe(
    const std::set<zookeeper::Group::Membership>& expected)
{
  // We can't do much here upon failure, we could try creating another Group but
  // that might just continue indefinitely, so we fail early
  // instead. Note that Group handles all retryable/recoverable
  // ZooKeeper errors internally.
  memberships = observer.watch(expected)
    .onReady(executor.defer(
        lambda::bind(&ZooKeeperPIDGroup::observed, this, lambda::_1)))
    .onFailed(executor.defer(
        lambda::bind(&ZooKeeperPIDGroup::failed, this, lambda::_1)))
    .onDiscarded(executor.defer(
        lambda::bind(&ZooKeeperPIDGroup::discarded, this)));
}


void ZooKeeperPIDGroup::observed(
    const process::Future<std::set<zookeeper::Group::Membership>>&)
{
  CHECK_READY(memberships);  // Not expecting Group to discard futures.

  LOG(INFO) << "ZooKeeper group memberships changed";

  // Get data for each membership in order to convert them to PIDs.
  std::list<process::Future<Option<std::string>>> futures;

  foreach (const zookeeper::Group::Membership& membership, memberships.get()) {
    futures.push_back(observer.data(membership));
  }

  process::collect(futures)
    .after(Seconds(5),
           [](process::Future<std::list<Option<std::string>>> datas) {
             // Handling time outs when collecting membership
             // data. For now, a timeout is treated as a failure.
             datas.discard();
             return process::Failure("Timed out");
           })
    .onAny(executor.defer(
        lambda::bind(&ZooKeeperPIDGroup::collected, this, lambda::_1)));
}


void ZooKeeperPIDGroup::collected(
    const process::Future<std::list<Option<std::string>>>& datas)
{
  if (datas.isFailed()) {
    LOG(WARNING) << "Failed to get data for ZooKeeper group members: "
                 << datas.failure();

    // Try again later assuming empty group. Note that this does not
    // remove any of the current group members.
    observe(std::set<zookeeper::Group::Membership>());
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

  observe(memberships.get());
}

void ZooKeeperPIDGroup::failed(const std::string& message)
{
  LOG(FATAL) << "Failed to watch/join the ZooKeeper group: " << message;
}


void ZooKeeperPIDGroup::discarded()
{
  LOG(FATAL) << "Not expecting future to get discarded!";
}

} // namespace log {
} // namespace internal {
} // namespace mesos {


#endif // __PID_GROUP_ZOOKEEPER_HPP__
