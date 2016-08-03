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

#include <gtest/gtest.h>

#include <gmock/gmock.h>

#include <process/future.hpp>
#include <process/gtest.hpp>
#include <process/pid.hpp>
#include <process/pid_group.hpp>
#include <process/process.hpp>

using process::Future;
using process::PIDGroup;
using process::ProcessBase;
using process::UPID;

TEST(PIDGroupTest, Watch)
{
  UPID pid1 = ProcessBase().self();
  UPID pid2 = ProcessBase().self();

  PIDGroup pidGroup;

  // Test the default parameter.
  Future<size_t> future = pidGroup.watch(1u);
  AWAIT_READY(future);
  EXPECT_EQ(0u, future.get());

  future = pidGroup.watch(2u, PIDGroup::NOT_EQUAL_TO);
  AWAIT_READY(future);
  EXPECT_EQ(0u, future.get());

  future = pidGroup.watch(0u, PIDGroup::GREATER_THAN_OR_EQUAL_TO);
  AWAIT_READY(future);
  EXPECT_EQ(0u, future.get());

  future = pidGroup.watch(1u, PIDGroup::LESS_THAN);
  AWAIT_READY(future);
  EXPECT_EQ(0u, future.get());

  pidGroup.add(pid1);

  future = pidGroup.watch(1u, PIDGroup::EQUAL_TO);
  AWAIT_READY(future);
  EXPECT_EQ(1u, future.get());

  future = pidGroup.watch(1u, PIDGroup::GREATER_THAN);
  ASSERT_TRUE(future.isPending());

  pidGroup.add(pid2);

  AWAIT_READY(future);
  EXPECT_EQ(2u, future.get());

  future = pidGroup.watch(1u, PIDGroup::LESS_THAN_OR_EQUAL_TO);
  ASSERT_TRUE(future.isPending());

  pidGroup.remove(pid2);

  AWAIT_READY(future);
  EXPECT_EQ(1u, future.get());
}
