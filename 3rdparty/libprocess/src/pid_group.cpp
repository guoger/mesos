// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License

#include <process/pid_group.hpp>

#include <list>
#include <set>
#include <string>

#include <process/dispatch.hpp>
#include <process/future.hpp>
#include <process/id.hpp>
#include <process/pid.hpp>
#include <process/process.hpp>
#include <process/protobuf.hpp>

#include <stout/foreach.hpp>
#include <stout/nothing.hpp>
#include <stout/unreachable.hpp>

using std::set;
using std::string;

namespace process {

PIDGroupProcess::PIDGroupProcess()
  : ProcessBase(process::ID::generate("pid-group")) {}

PIDGroupProcess::PIDGroupProcess(const std::set<process::UPID>& pids)
  : ProcessBase(process::ID::generate("pid-group"))
{
  set(pids);
}


void PIDGroupProcess::add(const process::UPID& pid)
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
  // of the PID group member to create a new connection.
  //
  // See MESOS-5576 for a scenario where reconnecting helps avoid
  // dropped messages.
  link(pid, RemoteConnection::RECONNECT);

  pids.insert(pid);

  // Update any pending watches.
  update();
}


void PIDGroupProcess::remove(const process::UPID& pid)
{
  // TODO(benh): unlink(pid);
  pids.erase(pid);

  // Update any pending watches.
  update();
}


void PIDGroupProcess::set(const std::set<process::UPID>& _pids)
{
  pids.clear();
  foreach (const process::UPID& pid, _pids) {
    add(pid); // Also does a link.
  }

  // Update any pending watches.
  update();
}


process::Future<size_t> PIDGroupProcess::watch(
    size_t size,
    PIDGroup::WatchMode mode)
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


void PIDGroupProcess::finalize()
{
  foreach (Watch* watch, watches) {
    watch->promise.fail("PID group is being terminated");
    delete watch;
  }
  watches.clear();
}

void PIDGroupProcess::update()
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


bool PIDGroupProcess::satisfied(size_t size, PIDGroup::WatchMode mode)
{
  switch (mode) {
    case PIDGroup::EQUAL_TO:
      return pids.size() == size;
    case PIDGroup::NOT_EQUAL_TO:
      return pids.size() != size;
    case PIDGroup::LESS_THAN:
      return pids.size() < size;
    case PIDGroup::LESS_THAN_OR_EQUAL_TO:
      return pids.size() <= size;
    case PIDGroup::GREATER_THAN:
      return pids.size() > size;
    case PIDGroup::GREATER_THAN_OR_EQUAL_TO:
      return pids.size() >= size;
    default:
      LOG(FATAL) << "Invalid watch mode";
      UNREACHABLE();
  }
}


PIDGroup::PIDGroup()
{
  process = new PIDGroupProcess();
  process::spawn(process);
}


PIDGroup::PIDGroup(const std::set<process::UPID>& pids)
  : base(pids)
{
  process = new PIDGroupProcess(pids);
  process::spawn(process);
}


PIDGroup::~PIDGroup()
{
  process::terminate(process);
  process::wait(process);
  delete process;
}


void PIDGroup::initialize(const process::UPID& _base)
{
  // No-op.
}


void PIDGroup::add(const process::UPID& pid)
{
  process::dispatch(process, &PIDGroupProcess::add, pid);
}


void PIDGroup::remove(const process::UPID& pid)
{
  process::dispatch(process, &PIDGroupProcess::remove, pid);
}


void PIDGroup::set(const std::set<process::UPID>& pids)
{
  process::dispatch(process, &PIDGroupProcess::set, pids);
}


process::Future<size_t> PIDGroup::watch(
    size_t size, PIDGroup::WatchMode mode) const
{
  return process::dispatch(process, &PIDGroupProcess::watch, size, mode);
}

} // namespace process {
