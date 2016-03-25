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

#include <paths.h>

#include <gmock/gmock.h>

#include <stout/foreach.hpp>
#include <stout/gtest.hpp>
#include <stout/none.hpp>
#include <stout/option.hpp>
#include <stout/os.hpp>
#include <stout/try.hpp>

#include <stout/tests/utils.hpp>

#include "linux/fs.hpp"

using std::string;

using mesos::internal::fs::MountTable;
using mesos::internal::fs::FileSystemTable;
using mesos::internal::fs::MountInfoTable;

namespace mesos {
namespace internal {
namespace tests {

class FsTest : public TemporaryDirectoryTest {};


TEST_F(FsTest, SupportedFS)
{
  EXPECT_SOME_TRUE(fs::supported("proc"));
  EXPECT_SOME_TRUE(fs::supported("sysfs"));

  EXPECT_SOME_FALSE(fs::supported("nonexistingfs"));
}


TEST_F(FsTest, MountTableRead)
{
  Try<MountTable> table = MountTable::read(_PATH_MOUNTED);

  ASSERT_SOME(table);

  Option<MountTable::Entry> root = None();
  Option<MountTable::Entry> proc = None();
  foreach (const MountTable::Entry& entry, table.get().entries) {
    if (entry.dir == "/") {
      root = entry;
    } else if (entry.dir == "/proc") {
      proc = entry;
    }
  }

  EXPECT_SOME(root);
  ASSERT_SOME(proc);
  EXPECT_EQ("proc", proc.get().type);
}


TEST_F(FsTest, MountTableHasOption)
{
  Try<MountTable> table = MountTable::read(_PATH_MOUNTED);

  ASSERT_SOME(table);

  Option<MountTable::Entry> proc = None();
  foreach (const MountTable::Entry& entry, table.get().entries) {
    if (entry.dir == "/proc") {
      proc = entry;
    }
  }

  ASSERT_SOME(proc);
  EXPECT_TRUE(proc.get().hasOption(MNTOPT_RW));
}


TEST_F(FsTest, FileSystemTableRead)
{
  Try<FileSystemTable> table = FileSystemTable::read();

  ASSERT_SOME(table);

  // NOTE: We do not check for /proc because, it is not always present in
  // /etc/fstab.
  Option<FileSystemTable::Entry> root = None();
  foreach (const FileSystemTable::Entry& entry, table.get().entries) {
    if (entry.file == "/") {
      root = entry;
    }
  }

  EXPECT_SOME(root);
}


TEST_F(FsTest, MountInfoTableParse)
{
  // Parse a private mount (no optional fields).
  const string privateMount =
    "19 1 8:1 / / rw,relatime - ext4 /dev/sda1 rw,seclabel,data=ordered";
  Try<MountInfoTable::Entry> entry = MountInfoTable::Entry::parse(privateMount);

  ASSERT_SOME(entry);
  EXPECT_EQ(19, entry.get().id);
  EXPECT_EQ(1, entry.get().parent);
  EXPECT_EQ(makedev(8, 1), entry.get().devno);
  EXPECT_EQ("/", entry.get().root);
  EXPECT_EQ("/", entry.get().target);
  EXPECT_EQ("rw,relatime", entry.get().vfsOptions);
  EXPECT_EQ("rw,seclabel,data=ordered", entry.get().fsOptions);
  EXPECT_EQ("", entry.get().optionalFields);
  EXPECT_EQ("ext4", entry.get().type);
  EXPECT_EQ("/dev/sda1", entry.get().source);

  // Parse a shared mount (includes one optional field).
  const string sharedMount =
    "19 1 8:1 / / rw,relatime shared:2 - ext4 /dev/sda1 rw,seclabel";
  entry = MountInfoTable::Entry::parse(sharedMount);

  ASSERT_SOME(entry);
  EXPECT_EQ(19, entry.get().id);
  EXPECT_EQ(1, entry.get().parent);
  EXPECT_EQ(makedev(8, 1), entry.get().devno);
  EXPECT_EQ("/", entry.get().root);
  EXPECT_EQ("/", entry.get().target);
  EXPECT_EQ("rw,relatime", entry.get().vfsOptions);
  EXPECT_EQ("rw,seclabel", entry.get().fsOptions);
  EXPECT_EQ("shared:2", entry.get().optionalFields);
  EXPECT_EQ("ext4", entry.get().type);
  EXPECT_EQ("/dev/sda1", entry.get().source);
}


TEST_F(FsTest, DISABLED_MountInfoTableRead)
{
  // Examine the calling process's mountinfo table.
  Try<MountInfoTable> table = MountInfoTable::read();
  ASSERT_SOME(table);

  // Every system should have at least a rootfs mounted.
  Option<MountInfoTable::Entry> root = None();
  foreach (const MountInfoTable::Entry& entry, table.get().entries) {
    if (entry.target == "/") {
      root = entry;
    }
  }

  EXPECT_SOME(root);

  // Repeat for pid 1.
  table = MountInfoTable::read(1);
  ASSERT_SOME(table);

  // Every system should have at least a rootfs mounted.
  root = None();
  foreach (const MountInfoTable::Entry& entry, table.get().entries) {
    if (entry.target == "/") {
      root = entry;
    }
  }

  EXPECT_SOME(root);
}


TEST_F(FsTest, ROOT_SharedMount)
{
  string directory = os::getcwd();

  // Do a self bind mount of the temporary directory.
  ASSERT_SOME(fs::mount(directory, directory, None(), MS_BIND, None()));

  // Mark the mount as a shared mount.
  ASSERT_SOME(fs::mount(None(), directory, None(), MS_SHARED, None()));

  // Find the above mount in the mount table.
  Try<MountInfoTable> table = MountInfoTable::read();
  ASSERT_SOME(table);

  Option<MountInfoTable::Entry> entry;
  foreach (const MountInfoTable::Entry& _entry, table.get().entries) {
    if (_entry.target == directory) {
      entry = _entry;
    }
  }

  ASSERT_SOME(entry);
  EXPECT_SOME(entry.get().shared());

  // Clean up the mount.
  EXPECT_SOME(fs::unmount(directory));
}


TEST_F(FsTest, ROOT_SlaveMount)
{
  string directory = os::getcwd();

  // Do a self bind mount of the temporary directory.
  ASSERT_SOME(fs::mount(directory, directory, None(), MS_BIND, None()));

  // Mark the mount as a shared mount of its own peer group.
  ASSERT_SOME(fs::mount(None(), directory, None(), MS_PRIVATE, None()));
  ASSERT_SOME(fs::mount(None(), directory, None(), MS_SHARED, None()));

  // Create a sub-mount under 'directory'.
  string source = path::join(directory, "source");
  string target = path::join(directory, "target");

  ASSERT_SOME(os::mkdir(source));
  ASSERT_SOME(os::mkdir(target));

  ASSERT_SOME(fs::mount(source, target, None(), MS_BIND, None()));

  // Mark the sub-mount as a slave mount.
  ASSERT_SOME(fs::mount(None(), target, None(), MS_SLAVE, None()));

  // Find the above sub-mount in the mount table, and check if it is a
  // slave mount as expected.
  Try<MountInfoTable> table = MountInfoTable::read();
  ASSERT_SOME(table);

  Option<MountInfoTable::Entry> parent;
  Option<MountInfoTable::Entry> child;
  foreach (const MountInfoTable::Entry& entry, table.get().entries) {
    if (entry.target == directory) {
      ASSERT_NONE(parent);
      parent = entry;
    } else if (entry.target == target) {
      ASSERT_NONE(child);
      child = entry;
    }
  }

  ASSERT_SOME(parent);
  ASSERT_SOME(child);

  EXPECT_SOME(parent.get().shared());
  EXPECT_SOME(child.get().master());
  EXPECT_EQ(child.get().master(), parent.get().shared());

  // Clean up the mount.
  EXPECT_SOME(fs::unmount(target));
  EXPECT_SOME(fs::unmount(directory));
}

} // namespace tests {
} // namespace internal {
} // namespace mesos {
