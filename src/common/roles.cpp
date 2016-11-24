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

#include <mesos/roles.hpp>

#include <stout/foreach.hpp>
#include <stout/strings.hpp>

#include "common/protobuf_utils.hpp"

using mesos::internal::protobuf::frameworkHasCapability;

using std::initializer_list;
using std::set;
using std::string;
using std::vector;

namespace mesos {
namespace roles {

// TODO(haosdent): Remove this function after we stop supporting `--roles`
// flag in master.
Try<vector<string>> parse(const string& text)
{
  vector<string> roles = strings::tokenize(text, ",");

  Option<Error> error = validate(roles);
  if (error.isSome()) {
    return error.get();
  } else {
    return roles;
  }
}


// TODO(haosdent): Pull this out into `stout` and make it satisfy all
// OS/locale constraints.
// \x09 is horizontal tab (whitespace);
// \x0a is line feed (whitespace);
// \x0b is vertical tab (whitespace);
// \x0c is form feed (whitespace);
// \x0d is carriage return (whitespace);
// \x20 is space (whitespace);
// \x2f is slash ('/');
// \x7f is backspace (del);
static const string* INVALID_CHARACTERS =
  new string("\x09\x0a\x0b\x0c\x0d\x20\x2f\x7f");


Option<Error> validate(const string& role)
{
  // We check * explicitly first as a performance improvement.
  static const string* star = new string("*");
  if (role == *star) {
    return None();
  }

  if (role.empty()) {
    return Error("Empty role name is invalid");
  }

  static const string* dot = new string(".");
  static const string* dotdot = new string("..");
  if (role == *dot) {
    return Error("Role name '.' is invalid");
  } else if (role == *dotdot) {
    return Error("Role name '..' is invalid");
  } else if (strings::startsWith(role, '-')) {
    return Error("Role name '" + role + "' is invalid "
                 "because it starts with a dash");
  }

  if (role.find_first_of(*INVALID_CHARACTERS) != string::npos) {
    return Error("Role name '" + role + "' is invalid "
                 "because it contains slash, backspace or whitespace");
  }

  return None();
}


Option<Error> validate(const vector<string>& roles)
{
  foreach (const string& role, roles) {
    Option<Error> error = validate(role);
    if (error.isSome()) {
      return error.get();
    }
  }

  return None();
}


Option<Error> validate(const FrameworkInfo& frameworkInfo)
{
  // First we check if role, roles and MULTI_ROLE are set properly,
  // see comments in function declaration for more details.
  if (frameworkHasCapability(
          frameworkInfo,
          FrameworkInfo::Capability::MULTI_ROLE)) {
    if (frameworkInfo.has_role()) {
      return Error("FrameworkInfo.role should NOT be set when MULTI_ROLE "
                   "framework capability is provided.");
    }

  } else {
    if (frameworkInfo.roles_size()) {
      if (frameworkInfo.has_role()) {
        return Error("Only one of FrameworkInfo.role and FrameworkInfo.roles "
                     "must be set at a time.");
      } else {
        return Error("If FrameworkInfo.roles is set, then the MULTI_ROLE "
                     "framework capability must be provided.");
      }
    }
  }

  // Validate role if exists
  if (frameworkInfo.has_role()) {
    return validate(frameworkInfo.role());
  }

  // Validate each role in roles.
  set<string> roles;
  Option<Error> error;
  foreach (const string& role, frameworkInfo.roles()) {
    if (roles.count(role) > 0) {
      return Error("FrameworkInfo.roles must not contain duplicate entries.");
    }

    error = validate(role);
    if (error.isSome()) {
      return error.get();
    }

    roles.insert(role);
  }

  return None();
}

} // namespace roles {
} // namespace mesos {
