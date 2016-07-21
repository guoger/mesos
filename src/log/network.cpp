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

Network::Network()
{
  process = new NetworkProcess();
  process::spawn(process);
}


Network::Network(const std::set<process::UPID>& pids)
{
  process = new NetworkProcess(pids);
  process::spawn(process);
}


Network::~Network()
{
  process::terminate(process);
  process::wait(process);
  delete process;
}


void Network::add(const process::UPID& pid)
{
  process::dispatch(process, &NetworkProcess::add, pid);
}


void Network::remove(const process::UPID& pid)
{
  process::dispatch(process, &NetworkProcess::remove, pid);
}


void Network::set(const std::set<process::UPID>& pids)
{
  process::dispatch(process, &NetworkProcess::set, pids);
}


process::Future<size_t> Network::watch(
    size_t size, Network::WatchMode mode) const
{
  return process::dispatch(process, &NetworkProcess::watch, size, mode);
}


NetworkProcess::NetworkProcess()
  : ProcessBase(process::ID::generate("log-network"))
{
}


NetworkProcess::NetworkProcess(const std::set<process::UPID>& pids)
  : ProcessBase(process::ID::generate("log-network"))
{
  set(pids);
}


void NetworkProcess::add(const process::UPID& pid)
{
  // Link in order to keep a socket open (more efficient).
  //
  // We force a reconnect to avoid sending on a "stale" socket. In
  // general when linking to a remote process, the underlying TCP
  // connection may become "stale". RFC 793 refers to this as a
  // "half-open" connection: the RST is not sent upon the death
  // of the peer and a RST will only be received once further
  // data is sent on the socket.
  //
  // "Half-open" (aka "stale") connections are typically addressed
  // via keep-alives (see RFC 1122 4.2.3.6) to periodically probe
  // the connection. In this case, we can rely on the (re-)addition
  // of the network member to create a new connection.
  //
  // See MESOS-5576 for a scenario where reconnecting helps avoid
  // dropped messages.
  link(pid, RemoteConnection::RECONNECT);

  pids.insert(pid);

  // Update any pending watches.
  update();
}


void NetworkProcess::remove(const process::UPID& pid)
{
  // TODO(benh): unlink(pid);
  pids.erase(pid);

  // Update any pending watches.
  update();
}


void NetworkProcess::set(const std::set<process::UPID>& _pids)
{
  pids.clear();
  foreach (const process::UPID& pid, _pids) {
    add(pid); // Also does a link.
  }

  // Update any pending watches.
  update();
}


process::Future<size_t> NetworkProcess::watch(
    size_t size, Network::WatchMode mode)
{
  if (satisfied(size, mode)) {
    return pids.size();
  }

  Watch* watch = new Watch(size, mode);
  watches.push_back(watch);

  // TODO(jieyu): Consider deleting 'watch' if the returned future
  // is discarded by the user.
  return watch->promise.future();
}


void NetworkProcess::finalize()
{
  foreach (Watch* watch, watches) {
    watch->promise.fail("Network is being terminated");
    delete watch;
  }
  watches.clear();
}


void NetworkProcess::update()
{
  const size_t size = watches.size();
  for (size_t i = 0; i < size; i++) {
    Watch* watch = watches.front();
    watches.pop_front();

    if (satisfied(watch->size, watch->mode)) {
      watch->promise.set(pids.size());
      delete watch;
    } else {
      watches.push_back(watch);
    }
  }
}


bool NetworkProcess::satisfied(size_t size, Network::WatchMode mode)
{
  switch (mode) {
    case Network::EQUAL_TO:
      return pids.size() == size;
    case Network::NOT_EQUAL_TO:
      return pids.size() != size;
    case Network::LESS_THAN:
      return pids.size() < size;
    case Network::LESS_THAN_OR_EQUAL_TO:
      return pids.size() <= size;
    case Network::GREATER_THAN:
      return pids.size() > size;
    case Network::GREATER_THAN_OR_EQUAL_TO:
      return pids.size() >= size;
    default:
      LOG(FATAL) << "Invalid watch mode";
      UNREACHABLE();
  }
}
