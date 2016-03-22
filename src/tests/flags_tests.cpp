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

#include <stdlib.h>

#include "master/master.hpp"

#include <stout/duration.hpp>
#include <stout/foreach.hpp>

#include "tests/mesos.hpp"

using namespace mesos::internal;
using namespace std;

namespace mesos {
namespace internal {
namespace tests {

class FlagsTest : public MesosTest {};


// This checks that master loads agent-related flags into Master::Flags
// and also propagate them to slave-related members, see MESOS-3781
// TODO: Change this test case when slave-related flags are removed
TEST(FlagsTest, AgentFlags)
{
  setenv("MESOS_AGENT_REREGISTER_TIMEOUT", "15mins", 1);
  setenv("MESOS_RECOVERY_AGENT_REMOVAL_LIMIT", "fake-removal-limit", 1);
  setenv("MESOS_AGENT_REMOVAL_RATE_LIMIT", "fake-rate-limit", 1);
  setenv("MESOS_AUTHENTICATE_AGENTS", "true", 1);
  setenv("MESOS_AGENT_PING_TIMEOUT", "10mins", 1);
  setenv("MESOS_MAX_AGENT_PING_TIMEOUTS", "100mins", 1);
  setenv("MESOS_MAX_EXECUTORS_PER_AGENT", "10", 1);

  master::Flags flags;

  Try<Nothing> load = flags.load("MESOS_");
  EXPECT_EQ(load.isError(), false);

  cout << flags.agent_reregister_timeout << endl;

  EXPECT_EQ(flags.slave_reregister_timeout, Minutes(15));

  unsetenv("MESOS_AGENT_REREGISTER_TIMEOUT");
  unsetenv("MESOS_RECOVERY_AGENT_REMOVAL_LIMIT");
  unsetenv("MESOS_AGENT_REMOVAL_RATE_LIMIT");
  unsetenv("MESOS_AUTHENTICATE_AGENTS");
  unsetenv("MESOS_AGENT_PING_TIMEOUT");
  unsetenv("MESOS_MAX_AGENT_PING_TIMEOUTS");
  unsetenv("MESOS_MAX_EXECUTORS_PER_AGENT");
}

} // namespace tests {
} // namespace internal {
} // namespace mesos {