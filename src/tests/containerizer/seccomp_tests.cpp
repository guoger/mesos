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

#include <vector>

#include <process/subprocess.hpp>

#include "tests/mesos.hpp"
#include "tests/utils.hpp"

#include "tests/containerizer/seccomp_test_helper.hpp"

using std::vector;

using process::Subprocess;

namespace mesos {
namespace internal {
namespace tests {

class SeccompTest : public ::testing::Test
{
public:
  // Launch 'ping' using the given capabilities and user.
  Try<Subprocess> sleep()
  {
    vector<string> argv = {
      "test-helper",
      SeccompTestHelper::NAME
    };

    return subprocess(
        getTestHelperPath("test-helper"),
        argv,
        Subprocess::FD(STDIN_FILENO),
        Subprocess::FD(STDOUT_FILENO),
        Subprocess::FD(STDERR_FILENO));
  }
};


TEST_F(SeccompTest, FooTest)
{
  Try<Subprocess> s = sleep();
  ASSERT_SOME(s);

  AWAIT_EXPECT_WEXITSTATUS_NE(0, s->status());
}

} // namespace tests {
} // namespace internal {
} // namespace mesos {
