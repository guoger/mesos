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

// TODO(benh): Eventually move and associate this code with the
// libprocess protobuf code rather than keep it here.

#include <list>
#include <set>
#include <string>

#include <mesos/zookeeper/group.hpp>

#include <process/collect.hpp>
#include <process/executor.hpp>
#include <process/id.hpp>
#include <process/protobuf.hpp>

#include <stout/duration.hpp>
#include <stout/foreach.hpp>
#include <stout/lambda.hpp>
#include <stout/nothing.hpp>
#include <stout/set.hpp>
#include <stout/unreachable.hpp>

#include "logging/logging.hpp"

// Forward declaration.
class NetworkProcess;

// A "network" is a collection of protobuf processes (may be local
// and/or remote). A network abstracts away the details of maintaining
// which processes are waiting to receive messages and requests in the
// presence of failures and dynamic reconfiguration.
class Network
{
public:
  enum WatchMode
  {
    EQUAL_TO,
    NOT_EQUAL_TO,
    LESS_THAN,
    LESS_THAN_OR_EQUAL_TO,
    GREATER_THAN,
    GREATER_THAN_OR_EQUAL_TO
  };

  Network();
  explicit Network(const std::set<process::UPID>& pids);
  virtual ~Network();

  // Adds a PID to this network.
  void add(const process::UPID& pid);

  // Removes a PID from this network.
  void remove(const process::UPID& pid);

  // Set the PIDs that are part of this network.
  void set(const std::set<process::UPID>& pids);

  // Returns a future which gets set when the network size satisfies
  // the constraint specified by 'size' and 'mode'. For example, if
  // 'size' is 2 and 'mode' is GREATER_THAN, then the returned future
  // will get set when the size of the network is greater than 2.
  process::Future<size_t> watch(
      size_t size,
      WatchMode mode = NOT_EQUAL_TO) const;

  // Sends a request to each member of the network and returns a set
  // of futures that represent their responses.
  template <typename Req, typename Res>
  process::Future<std::set<process::Future<Res> > > broadcast(
      const Protocol<Req, Res>& protocol,
      const Req& req,
      const std::set<process::UPID>& filter = std::set<process::UPID>()) const;

  // Sends a message to each member of the network. The returned
  // future is set when the message is broadcasted.
  template <typename M>
  process::Future<Nothing> broadcast(
      const M& m,
      const std::set<process::UPID>& filter = std::set<process::UPID>()) const;

private:
  // Not copyable, not assignable.
  Network(const Network&);
  Network& operator=(const Network&);

  NetworkProcess* process;
};


class NetworkProcess : public ProtobufProcess<NetworkProcess>
{
public:
  NetworkProcess();

  explicit NetworkProcess(const std::set<process::UPID>& pids);

  void add(const process::UPID& pid);

  void remove(const process::UPID& pid);

  void set(const std::set<process::UPID>& _pids);

  process::Future<size_t> watch(size_t size, Network::WatchMode mode);

  // Sends a request to each of the groups members and returns a set
  // of futures that represent their responses.
  template <typename Req, typename Res>
  std::set<process::Future<Res> > broadcast(
      const Protocol<Req, Res>& protocol,
      const Req& req,
      const std::set<process::UPID>& filter)
  {
    std::set<process::Future<Res> > futures;
    typename std::set<process::UPID>::const_iterator iterator;
    for (iterator = pids.begin(); iterator != pids.end(); ++iterator) {
      const process::UPID& pid = *iterator;
      if (filter.count(pid) == 0) {
        futures.insert(protocol(pid, req));
      }
    }
    return futures;
  }

  template <typename M>
  Nothing broadcast(
      const M& m,
      const std::set<process::UPID>& filter)
  {
    std::set<process::UPID>::const_iterator iterator;
    for (iterator = pids.begin(); iterator != pids.end(); ++iterator) {
      const process::UPID& pid = *iterator;
      if (filter.count(pid) == 0) {
        process::post(pid, m);
      }
    }
    return Nothing();
  }

protected:
  virtual void finalize();

private:
  struct Watch
  {
    Watch(size_t _size, Network::WatchMode _mode)
      : size(_size), mode(_mode) {}

    size_t size;
    Network::WatchMode mode;
    process::Promise<size_t> promise;
  };

  // Not copyable, not assignable.
  NetworkProcess(const NetworkProcess&);
  NetworkProcess& operator=(const NetworkProcess&);

  // Notifies the change of the network.
  void update();

  // Returns true if the current size of the network satisfies the
  // constraint specified by 'size' and 'mode'.
  bool satisfied(size_t size, Network::WatchMode mode);

  std::set<process::UPID> pids;
  std::list<Watch*> watches;
};


template <typename Req, typename Res>
process::Future<std::set<process::Future<Res> > > Network::broadcast(
    const Protocol<Req, Res>& protocol,
    const Req& req,
    const std::set<process::UPID>& filter) const
{
  return process::dispatch(process, &NetworkProcess::broadcast<Req, Res>,
                           protocol, req, filter);
}


template <typename M>
process::Future<Nothing> Network::broadcast(
    const M& m,
    const std::set<process::UPID>& filter) const
{
  // Need to disambiguate overloaded function.
  Nothing (NetworkProcess::*broadcast)(const M&, const std::set<process::UPID>&)
    = &NetworkProcess::broadcast<M>;

  return process::dispatch(process, broadcast, m, filter);
}

#endif // __NETWORK_HPP__
