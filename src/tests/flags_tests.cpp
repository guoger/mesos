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
// from environment variables and also propagate them to slave-related members,
// see MESOS-3781.
// TODO(guoger): Change this test case when flags with keyword slave
// are removed
TEST(FlagsTest, AgentFlagsFromEnv)
{
  setenv("MESOS_AGENT_REREGISTER_TIMEOUT", "15mins", 1);
  setenv("MESOS_RECOVERY_AGENT_REMOVAL_LIMIT", "50%", 1);
  setenv("MESOS_AGENT_REMOVAL_RATE_LIMIT", "1/20mins", 1);
  setenv("MESOS_AUTHENTICATE_AGENTS", "true", 1);
  setenv("MESOS_AGENT_PING_TIMEOUT", "10secs", 1);
  setenv("MESOS_MAX_AGENT_PING_TIMEOUTS", "10", 1);
  setenv("MESOS_MAX_EXECUTORS_PER_AGENT", "10", 1);

  master::Flags flags;

  Try<Nothing> load = flags.load("MESOS_");
  CHECK_SOME(load);

  EXPECT_EQ(flags.slave_reregister_timeout, Minutes(15));
  EXPECT_EQ(flags.recovery_slave_removal_limit, "50%");
  CHECK_SOME(flags.slave_removal_rate_limit);
  EXPECT_EQ(flags.slave_removal_rate_limit, Option<std::string>("1/20mins"));
  EXPECT_EQ(flags.authenticate_slaves, true);
  EXPECT_EQ(flags.slave_ping_timeout, Seconds(10));
  EXPECT_EQ(flags.max_slave_ping_timeouts, 10);
#ifdef WITH_NETWORK_ISOLATOR
  CHECK_SOME(flags.max_executors_per_slave);
  EXPECT_EQ(flags.max_executors_per_slave, Option<size_t>(10));
#endif // WITH_NETWORK_ISOLATOR

  unsetenv("MESOS_AGENT_REREGISTER_TIMEOUT");
  unsetenv("MESOS_RECOVERY_AGENT_REMOVAL_LIMIT");
  unsetenv("MESOS_AGENT_REMOVAL_RATE_LIMIT");
  unsetenv("MESOS_AUTHENTICATE_AGENTS");
  unsetenv("MESOS_AGENT_PING_TIMEOUT");
  unsetenv("MESOS_MAX_AGENT_PING_TIMEOUTS");
  unsetenv("MESOS_MAX_EXECUTORS_PER_AGENT");
}


// This checks that master loads agent-related flags into Master::Flags
// from command line arguments and also propagate them to slave-related
// members, see MESOS-3781.
// TODO(guoger): Change this test case when flags with keyword slave
// are removed
TEST(FlagsTest, AgentFlagsFromCmdArgs)
{
#ifdef WITH_NETWORK_ISOLATOR
  int argc = 8;
  const char* const argv[] = {
      "mesos-executable",
      "--agent_reregister_timeout=15mins",
      "--recovery_agent_removal_limit=50%",
      "--agent_removal_rate_limit=1/20mins",
      "--authenticate_agents=true",
      "--agent_ping_timeout=10secs",
      "--max_agent_ping_timeouts=10",
      "--max_executors_per_agent=10"
  };
#else
  int argc = 7;
  const char* const argv[] = {
      "mesos-executable",
      "--agent_reregister_timeout=15mins",
      "--recovery_agent_removal_limit=50%",
      "--agent_removal_rate_limit=1/20mins",
      "--authenticate_agents=true",
      "--agent_ping_timeout=10secs",
      "--max_agent_ping_timeouts=10"
  };
#endif

  master::Flags flags;

  Try<Nothing> load = flags.load("MESOS_", argc, argv);
  CHECK_SOME(load);

  EXPECT_EQ(flags.slave_reregister_timeout, Minutes(15));
  EXPECT_EQ(flags.recovery_slave_removal_limit, "50%");
  CHECK_SOME(flags.slave_removal_rate_limit);
  EXPECT_EQ(flags.slave_removal_rate_limit, Option<std::string>("1/20mins"));
  EXPECT_EQ(flags.authenticate_slaves, true);
  EXPECT_EQ(flags.slave_ping_timeout, Seconds(10));
  EXPECT_EQ(flags.max_slave_ping_timeouts, 10);

#ifdef WITH_NETWORK_ISOLATOR
  CHECK_SOME(flags.max_executors_per_slave);
  EXPECT_EQ(flags.max_executors_per_slave, Option<size_t>(10));
#endif // WITH_NETWORK_ISOLATOR
}

} // namespace tests {
} // namespace internal {
} // namespace mesos {
