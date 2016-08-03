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

#ifndef __PROCESS_PID_GROUP_HPP__
#define __PROCESS_PID_GROUP_HPP__

#include <list>
#include <set>

#include <process/future.hpp>
#include <process/pid.hpp>
#include <process/protobuf.hpp>

#include <stout/nothing.hpp>

namespace process {

// Forward declaration.
class PIDGroupProcess;

/**
 * A "pid_group" is a collection of protobuf processes (may be local
 * and/or remote). A pid_group abstracts away the details of maintaining
 * which processes are waiting to receive messages and requests in the
 * presence of failures and dynamic reconfiguration.
 */
class PIDGroup
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

  PIDGroup();
  explicit PIDGroup(const std::set<process::UPID>& pids);
  virtual ~PIDGroup();

  // Adds a PID to this PID group.
  void add(const process::UPID& pid);

  // Removes a PID from this PID group.
  void remove(const process::UPID& pid);

  // Set the PIDs that are part of this PID group.
  void set(const std::set<process::UPID>& pids);

  // Returns a future which gets set when the PID group size satisfies
  // the constraint specified by 'size' and 'mode'. For example, if
  // 'size' is 2 and 'mode' is GREATER_THAN, then the returned future
  // will get set when the size of the PID group is greater than 2.
  process::Future<size_t> watch(
      size_t size,
      WatchMode mode = NOT_EQUAL_TO) const;

  // Sends a request to each member of the PID group and returns a set
  // of futures that represent their responses.
  template <typename Req, typename Res>
  process::Future<std::set<process::Future<Res> > > broadcast(
      const Protocol<Req, Res>& protocol,
      const Req& req,
      const std::set<process::UPID>& filter = std::set<process::UPID>()) const;

  // Sends a message to each member of the PID group. The returned
  // future is set when the message is broadcasted.
  template <typename M>
  process::Future<Nothing> broadcast(
      const M& m,
      const std::set<process::UPID>& filter = std::set<process::UPID>()) const;

private:
  // Not copyable, not assignable.
  PIDGroup(const PIDGroup&);
  PIDGroup& operator=(const PIDGroup&);

  PIDGroupProcess* process;
};


class PIDGroupProcess : public ProtobufProcess<PIDGroupProcess>
{
public:
  PIDGroupProcess();

  explicit PIDGroupProcess(const std::set<process::UPID>& pids);

  void add(const process::UPID& pid);

  void remove(const process::UPID& pid);

  void set(const std::set<process::UPID>& _pids);

  process::Future<size_t> watch(size_t size, PIDGroup::WatchMode mode);

  // Sends a request to each of the groups members and returns a set
  // of futures that represent their responses.
  template <typename Req, typename Res>
  std::set<process::Future<Res> > broadcast(
      const Protocol<Req, Res>& protocol,
      const Req& req,
      const std::set<process::UPID>& filter);

  template <typename M>
  Nothing broadcast(const M& m, const std::set<process::UPID>& filter);

protected:
  virtual void finalize();

private:
  struct Watch
  {
    Watch(size_t _size, PIDGroup::WatchMode _mode)
      : size(_size), mode(_mode) {}

    size_t size;
    PIDGroup::WatchMode mode;
    process::Promise<size_t> promise;
  };

  // Not copyable, not assignable.
  PIDGroupProcess(const PIDGroupProcess&);
  PIDGroupProcess& operator=(const PIDGroupProcess&);

  // Notifies the change of the PID group.
  void update();

  // Returns true if the current size of the PID group satisfies the
  // constraint specified by 'size' and 'mode'.
  bool satisfied(size_t size, PIDGroup::WatchMode mode);

  std::set<process::UPID> pids;
  std::list<Watch*> watches;
};


template <typename Req, typename Res>
process::Future<std::set<process::Future<Res>>> PIDGroup::broadcast(
    const Protocol<Req, Res>& protocol,
    const Req& req,
    const std::set<process::UPID>& filter) const
{
  return process::dispatch(process, &PIDGroupProcess::broadcast<Req, Res>,
                           protocol, req, filter);
}


template <typename M>
process::Future<Nothing> PIDGroup::broadcast(
    const M& m,
    const std::set<process::UPID>& filter) const
{
  // Need to disambiguate overloaded function.
  Nothing (PIDGroupProcess::*broadcast)(
      const M&, const std::set<process::UPID>&)
    = &PIDGroupProcess::broadcast<M>;

  return process::dispatch(process, broadcast, m, filter);
}


template <typename Req, typename Res>
std::set<process::Future<Res>> PIDGroupProcess::broadcast(
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
Nothing PIDGroupProcess::broadcast(
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

} // namespace process {

#endif // __PROCESS_PID_GROUP_HPP__
