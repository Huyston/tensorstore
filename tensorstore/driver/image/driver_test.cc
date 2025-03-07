// Copyright 2021 The TensorStore Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <assert.h>

#include <string>
#include <type_traits>

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include "absl/strings/cord.h"
#include "absl/strings/string_view.h"
#include <nlohmann/json.hpp>
#include "tensorstore/context.h"
#include "tensorstore/index.h"
#include "tensorstore/index_space/dim_expression.h"
#include "tensorstore/internal/json_gtest.h"
#include "tensorstore/kvstore/kvstore.h"
#include "tensorstore/kvstore/operations.h"
#include "tensorstore/open.h"
#include "tensorstore/progress.h"
#include "tensorstore/strided_layout.h"
#include "tensorstore/tensorstore.h"
#include "tensorstore/util/future.h"
#include "tensorstore/util/result.h"
#include "tensorstore/util/status.h"
#include "tensorstore/util/status_testutil.h"

namespace {

using ::tensorstore::Context;
using ::tensorstore::CopyProgressFunction;
using ::tensorstore::DimensionIndex;
using ::tensorstore::Index;
using ::tensorstore::MatchesJson;
using ::tensorstore::MatchesStatus;
using ::tensorstore::ReadProgressFunction;

// hexdump -e \"\"\ 16/1\ \"\ 0x%02x,\"\ \"\\n\" image.png
static constexpr unsigned char kPng[] = {
    0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a, 0x00, 0x00, 0x00, 0x0d,
    0x49, 0x48, 0x44, 0x52, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01, 0x00,
    0x08, 0x02, 0x00, 0x00, 0x00, 0xd3, 0x10, 0x3f, 0x31, 0x00, 0x00, 0x00,
    0x09, 0x70, 0x48, 0x59, 0x73, 0x00, 0x00, 0x00, 0x48, 0x00, 0x00, 0x00,
    0x48, 0x00, 0x46, 0xc9, 0x6b, 0x3e, 0x00, 0x00, 0x02, 0xbb, 0x49, 0x44,
    0x41, 0x54, 0x78, 0xda, 0xed, 0xd3, 0x01, 0x09, 0x00, 0x30, 0x10, 0xc4,
    0xb0, 0x7b, 0x98, 0x7f, 0xcd, 0x13, 0xd2, 0x84, 0x5a, 0xe8, 0x6d, 0x3b,
    0xa9, 0xda, 0xdb, 0x0d, 0xb2, 0x0c, 0x40, 0x9a, 0x01, 0x48, 0x33, 0x00,
    0x69, 0x06, 0x20, 0xcd, 0x00, 0xa4, 0x19, 0x80, 0x34, 0x03, 0x90, 0x66,
    0x00, 0xd2, 0x0c, 0x40, 0x9a, 0x01, 0x48, 0x33, 0x00, 0x69, 0x06, 0x20,
    0xcd, 0x00, 0xa4, 0x19, 0x80, 0x34, 0x03, 0x90, 0x66, 0x00, 0xd2, 0x0c,
    0x40, 0x9a, 0x01, 0x48, 0x33, 0x00, 0x69, 0x06, 0x20, 0xcd, 0x00, 0xa4,
    0x19, 0x80, 0x34, 0x03, 0x90, 0x66, 0x00, 0xd2, 0x0c, 0x40, 0x9a, 0x01,
    0x48, 0x33, 0x00, 0x69, 0x06, 0x20, 0xcd, 0x00, 0xa4, 0x19, 0x80, 0x34,
    0x03, 0x90, 0x66, 0x00, 0xd2, 0x0c, 0x40, 0x9a, 0x01, 0x48, 0x33, 0x00,
    0x69, 0x06, 0x20, 0xcd, 0x00, 0xa4, 0x19, 0x80, 0x34, 0x03, 0x90, 0x66,
    0x00, 0xd2, 0x0c, 0x40, 0x9a, 0x01, 0x48, 0x33, 0x00, 0x69, 0x06, 0x20,
    0xcd, 0x00, 0xa4, 0x19, 0x80, 0x34, 0x03, 0x90, 0x66, 0x00, 0xd2, 0x0c,
    0x40, 0x9a, 0x01, 0x48, 0x33, 0x00, 0x69, 0x06, 0x20, 0xcd, 0x00, 0xa4,
    0x19, 0x80, 0x34, 0x03, 0x90, 0x66, 0x00, 0xd2, 0x0c, 0x40, 0x9a, 0x01,
    0x48, 0x33, 0x00, 0x69, 0x06, 0x20, 0xcd, 0x00, 0xa4, 0x19, 0x80, 0x34,
    0x03, 0x90, 0x66, 0x00, 0xd2, 0x0c, 0x40, 0x9a, 0x01, 0x48, 0x33, 0x00,
    0x69, 0x06, 0x20, 0xcd, 0x00, 0xa4, 0x19, 0x80, 0x34, 0x03, 0x90, 0x66,
    0x00, 0xd2, 0x0c, 0x40, 0x9a, 0x01, 0x48, 0x33, 0x00, 0x69, 0x06, 0x20,
    0xcd, 0x00, 0xa4, 0x19, 0x80, 0x34, 0x03, 0x90, 0x66, 0x00, 0xd2, 0x0c,
    0x40, 0x9a, 0x01, 0x48, 0x33, 0x00, 0x69, 0x06, 0x20, 0xcd, 0x00, 0xa4,
    0x19, 0x80, 0x34, 0x03, 0x90, 0x66, 0x00, 0xd2, 0x0c, 0x40, 0x9a, 0x01,
    0x48, 0x33, 0x00, 0x69, 0x06, 0x20, 0xcd, 0x00, 0xa4, 0x19, 0x80, 0x34,
    0x03, 0x90, 0x66, 0x00, 0xd2, 0x0c, 0x40, 0x9a, 0x01, 0x48, 0x33, 0x00,
    0x69, 0x06, 0x20, 0xcd, 0x00, 0xa4, 0x19, 0x80, 0x34, 0x03, 0x90, 0x66,
    0x00, 0xd2, 0x0c, 0x40, 0x9a, 0x01, 0x48, 0x33, 0x00, 0x69, 0x06, 0x20,
    0xcd, 0x00, 0xa4, 0x19, 0x80, 0x34, 0x03, 0x90, 0x66, 0x00, 0xd2, 0x0c,
    0x40, 0x9a, 0x01, 0x48, 0x33, 0x00, 0x69, 0x06, 0x20, 0xcd, 0x00, 0xa4,
    0x19, 0x80, 0x34, 0x03, 0x90, 0x66, 0x00, 0xd2, 0x0c, 0x40, 0x9a, 0x01,
    0x48, 0x33, 0x00, 0x69, 0x06, 0x20, 0xcd, 0x00, 0xa4, 0x19, 0x80, 0x34,
    0x03, 0x90, 0x66, 0x00, 0xd2, 0x0c, 0x40, 0x9a, 0x01, 0x48, 0x33, 0x00,
    0x69, 0x06, 0x20, 0xcd, 0x00, 0xa4, 0x19, 0x80, 0x34, 0x03, 0x90, 0x66,
    0x00, 0xd2, 0x0c, 0x40, 0x9a, 0x01, 0x48, 0x33, 0x00, 0x69, 0x06, 0x20,
    0xcd, 0x00, 0xa4, 0x19, 0x80, 0x34, 0x03, 0x90, 0x66, 0x00, 0xd2, 0x0c,
    0x40, 0x9a, 0x01, 0x48, 0x33, 0x00, 0x69, 0x06, 0x20, 0xcd, 0x00, 0xa4,
    0x19, 0x80, 0x34, 0x03, 0x90, 0x66, 0x00, 0xd2, 0x0c, 0x40, 0x9a, 0x01,
    0x48, 0x33, 0x00, 0x69, 0x06, 0x20, 0xcd, 0x00, 0xa4, 0x19, 0x80, 0x34,
    0x03, 0x90, 0x66, 0x00, 0xd2, 0x0c, 0x40, 0x9a, 0x01, 0x48, 0x33, 0x00,
    0x69, 0x06, 0x20, 0xcd, 0x00, 0xa4, 0x19, 0x80, 0x34, 0x03, 0x90, 0x66,
    0x00, 0xd2, 0x0c, 0x40, 0x9a, 0x01, 0x48, 0x33, 0x00, 0x69, 0x06, 0x20,
    0xcd, 0x00, 0xa4, 0x19, 0x80, 0x34, 0x03, 0x90, 0x66, 0x00, 0xd2, 0x0c,
    0x40, 0x9a, 0x01, 0x48, 0x33, 0x00, 0x69, 0x06, 0x20, 0xcd, 0x00, 0xa4,
    0x19, 0x80, 0x34, 0x03, 0x90, 0x66, 0x00, 0xd2, 0x0c, 0x40, 0x9a, 0x01,
    0x48, 0x33, 0x00, 0x69, 0x06, 0x20, 0xcd, 0x00, 0xa4, 0x19, 0x80, 0x34,
    0x03, 0x90, 0x66, 0x00, 0xd2, 0x0c, 0x40, 0x9a, 0x01, 0x48, 0x33, 0x00,
    0x69, 0x06, 0x20, 0xcd, 0x00, 0xa4, 0x19, 0x80, 0x34, 0x03, 0x90, 0x66,
    0x00, 0xd2, 0x0c, 0x40, 0x9a, 0x01, 0x48, 0x33, 0x00, 0x69, 0x06, 0x20,
    0xcd, 0x00, 0xa4, 0x19, 0x80, 0x34, 0x03, 0x90, 0x66, 0x00, 0xd2, 0x0c,
    0x40, 0x9a, 0x01, 0x48, 0x33, 0x00, 0x69, 0x06, 0x20, 0xcd, 0x00, 0xa4,
    0x19, 0x80, 0x34, 0x03, 0x90, 0x66, 0x00, 0xd2, 0x0c, 0x40, 0x9a, 0x01,
    0x48, 0x33, 0x00, 0x69, 0x06, 0x20, 0xcd, 0x00, 0xa4, 0x19, 0x80, 0x34,
    0x03, 0x90, 0x66, 0x00, 0xd2, 0x0c, 0x40, 0x9a, 0x01, 0x48, 0x33, 0x00,
    0x69, 0x06, 0x20, 0xcd, 0x00, 0xa4, 0x19, 0x80, 0x34, 0x03, 0x90, 0x66,
    0x00, 0xd2, 0x0c, 0x40, 0x9a, 0x01, 0x48, 0x33, 0x00, 0x69, 0x06, 0x20,
    0xcd, 0x00, 0xa4, 0x19, 0x80, 0x34, 0x03, 0x90, 0x66, 0x00, 0xd2, 0x0c,
    0x40, 0x9a, 0x01, 0x48, 0x33, 0x00, 0x69, 0x06, 0x20, 0xcd, 0x00, 0xa4,
    0x19, 0x80, 0x34, 0x03, 0x90, 0x66, 0x00, 0xd2, 0x0c, 0x40, 0x9a, 0x01,
    0x48, 0x33, 0x00, 0x69, 0x06, 0x20, 0xcd, 0x00, 0xa4, 0x19, 0x80, 0xb4,
    0x0f, 0x1a, 0x65, 0x05, 0xfc, 0x4f, 0xed, 0x72, 0x2f, 0x00, 0x00, 0x00,
    0x00, 0x49, 0x45, 0x4e, 0x44, 0xae, 0x42, 0x60, 0x82,
};

static constexpr unsigned char kJpeg[] = {
    0xff, 0xd8, 0xff, 0xe0, 0x00, 0x10, 0x4a, 0x46, 0x49, 0x46, 0x00, 0x01,
    0x01, 0x00, 0x00, 0x48, 0x00, 0x48, 0x00, 0x00, 0xff, 0xdb, 0x00, 0x84,
    0x00, 0x1c, 0x1c, 0x1c, 0x1c, 0x1c, 0x1c, 0x30, 0x1c, 0x1c, 0x30, 0x44,
    0x30, 0x30, 0x30, 0x44, 0x5c, 0x44, 0x44, 0x44, 0x44, 0x5c, 0x74, 0x5c,
    0x5c, 0x5c, 0x5c, 0x5c, 0x74, 0x8c, 0x74, 0x74, 0x74, 0x74, 0x74, 0x74,
    0x8c, 0x8c, 0x8c, 0x8c, 0x8c, 0x8c, 0x8c, 0x8c, 0xa8, 0xa8, 0xa8, 0xa8,
    0xa8, 0xa8, 0xc4, 0xc4, 0xc4, 0xc4, 0xc4, 0xdc, 0xdc, 0xdc, 0xdc, 0xdc,
    0xdc, 0xdc, 0xdc, 0xdc, 0xdc, 0x01, 0x22, 0x24, 0x24, 0x38, 0x34, 0x38,
    0x60, 0x34, 0x34, 0x60, 0xe6, 0x9c, 0x80, 0x9c, 0xe6, 0xe6, 0xe6, 0xe6,
    0xe6, 0xe6, 0xe6, 0xe6, 0xe6, 0xe6, 0xe6, 0xe6, 0xe6, 0xe6, 0xe6, 0xe6,
    0xe6, 0xe6, 0xe6, 0xe6, 0xe6, 0xe6, 0xe6, 0xe6, 0xe6, 0xe6, 0xe6, 0xe6,
    0xe6, 0xe6, 0xe6, 0xe6, 0xe6, 0xe6, 0xe6, 0xe6, 0xe6, 0xe6, 0xe6, 0xe6,
    0xe6, 0xe6, 0xe6, 0xe6, 0xe6, 0xe6, 0xe6, 0xe6, 0xe6, 0xe6, 0xff, 0xc2,
    0x00, 0x11, 0x08, 0x01, 0x00, 0x01, 0x00, 0x03, 0x01, 0x22, 0x00, 0x02,
    0x11, 0x01, 0x03, 0x11, 0x01, 0xff, 0xc4, 0x00, 0x17, 0x00, 0x01, 0x01,
    0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x01, 0x00, 0x05, 0x06, 0xff, 0xda, 0x00, 0x08, 0x01, 0x01,
    0x00, 0x00, 0x00, 0x00, 0xe6, 0xd6, 0x59, 0x59, 0x65, 0x96, 0x59, 0x65,
    0x95, 0xcb, 0x96, 0x56, 0x59, 0x65, 0x96, 0x59, 0x65, 0x67, 0x2d, 0x96,
    0x56, 0x59, 0x65, 0x96, 0x59, 0x65, 0x6c, 0xb5, 0x95, 0x96, 0x59, 0x65,
    0x96, 0x59, 0x59, 0x72, 0xe5, 0x95, 0x96, 0x59, 0x65, 0x96, 0x59, 0x59,
    0xcb, 0x65, 0x95, 0x96, 0x59, 0x65, 0x96, 0x59, 0x5b, 0x2d, 0x65, 0x65,
    0x96, 0x59, 0x65, 0x96, 0x56, 0x5c, 0xb9, 0x65, 0x65, 0x96, 0x59, 0x65,
    0x96, 0x56, 0x72, 0xd9, 0x59, 0x65, 0x96, 0x59, 0x65, 0x95, 0x96, 0xcb,
    0x59, 0x59, 0x65, 0x96, 0x59, 0x65, 0x95, 0x97, 0x2e, 0x56, 0x59, 0x65,
    0x96, 0x59, 0x65, 0x65, 0x9c, 0xb6, 0x56, 0x59, 0x65, 0x96, 0x59, 0x65,
    0x65, 0xb2, 0xd6, 0x56, 0x59, 0x65, 0x96, 0x59, 0x59, 0x65, 0xcb, 0x95,
    0x96, 0x59, 0x65, 0x96, 0x59, 0x59, 0x67, 0x2d, 0x95, 0x96, 0x59, 0x65,
    0x96, 0x56, 0x59, 0x6c, 0xb5, 0x65, 0x96, 0x59, 0x65, 0x96, 0x56, 0x59,
    0x72, 0xe5, 0x65, 0x96, 0x59, 0x65, 0x95, 0x96, 0x59, 0xcb, 0x59, 0x65,
    0x96, 0x59, 0x65, 0x95, 0x96, 0x5b, 0x2d, 0x59, 0x65, 0x96, 0x59, 0x65,
    0x95, 0x96, 0x5c, 0xb6, 0x59, 0x65, 0x96, 0x59, 0x65, 0x65, 0x96, 0x72,
    0xd6, 0x59, 0x65, 0x96, 0x59, 0x65, 0x65, 0x96, 0xcc, 0x96, 0x59, 0x65,
    0x96, 0x59, 0x59, 0x65, 0x97, 0x2d, 0x96, 0x59, 0x65, 0x96, 0x59, 0x59,
    0x65, 0x9c, 0xb5, 0x96, 0x59, 0x65, 0x96, 0x56, 0x59, 0x65, 0xb3, 0x25,
    0x96, 0x59, 0x65, 0x96, 0x56, 0x59, 0x65, 0xcb, 0x65, 0x96, 0x59, 0x65,
    0x95, 0x96, 0x59, 0x67, 0x2d, 0x65, 0x96, 0x59, 0x65, 0x95, 0x96, 0x59,
    0x6c, 0xc9, 0x65, 0x96, 0x59, 0x65, 0x65, 0x96, 0x59, 0x72, 0xd9, 0x65,
    0x96, 0x59, 0x65, 0x65, 0x96, 0x59, 0xcb, 0x59, 0x65, 0x96, 0x59, 0x65,
    0x65, 0x96, 0x5b, 0x32, 0x59, 0x65, 0x96, 0x59, 0x59, 0x65, 0x96, 0x5c,
    0xb6, 0x59, 0x65, 0x96, 0x59, 0x59, 0x65, 0x96, 0x7f, 0xff, 0xc4, 0x00,
    0x16, 0x01, 0x01, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x04, 0xff, 0xda, 0x00,
    0x08, 0x01, 0x02, 0x10, 0x00, 0x00, 0x00, 0x08, 0x82, 0x08, 0x82, 0xd0,
    0x41, 0x04, 0x41, 0x06, 0x90, 0x82, 0x20, 0x82, 0x34, 0x44, 0x10, 0x41,
    0x11, 0xa2, 0x08, 0x20, 0x88, 0x2d, 0x04, 0x10, 0x44, 0x10, 0x69, 0x08,
    0x82, 0x08, 0x23, 0x44, 0x41, 0x04, 0x11, 0x1a, 0x20, 0x82, 0x08, 0x82,
    0xd0, 0x41, 0x10, 0x41, 0x06, 0x90, 0x88, 0x20, 0x82, 0x34, 0x84, 0x10,
    0x44, 0x11, 0xa2, 0x08, 0x22, 0x08, 0x2d, 0x04, 0x11, 0x04, 0x10, 0x69,
    0x20, 0x82, 0x08, 0x83, 0x48, 0x41, 0x04, 0x41, 0x1f, 0xff, 0xc4, 0x00,
    0x17, 0x01, 0x01, 0x01, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x01, 0x05, 0x00, 0xff, 0xda,
    0x00, 0x08, 0x01, 0x03, 0x10, 0x00, 0x00, 0x00, 0xe4, 0x92, 0x55, 0x24,
    0x92, 0x55, 0x61, 0x54, 0x92, 0x49, 0x54, 0x92, 0x4b, 0x05, 0x24, 0xaa,
    0x49, 0x24, 0xaa, 0x58, 0x29, 0x24, 0x92, 0xa9, 0x24, 0x92, 0xc1, 0x4a,
    0xa4, 0x92, 0x4a, 0xa4, 0x96, 0x0a, 0x49, 0x25, 0x52, 0x49, 0x25, 0x70,
    0x95, 0x49, 0x24, 0x95, 0x49, 0x25, 0x82, 0x92, 0x4a, 0xa4, 0x92, 0x4a,
    0xac, 0x2a, 0x92, 0x49, 0x2a, 0x92, 0x49, 0x60, 0xa4, 0x95, 0x49, 0x24,
    0x95, 0x4b, 0x05, 0x24, 0x92, 0x55, 0x24, 0x92, 0x58, 0x29, 0x2a, 0x92,
    0x49, 0x2a, 0x92, 0xc1, 0x49, 0x24, 0xaa, 0x49, 0x24, 0xae, 0x12, 0xa9,
    0x24, 0x92, 0xa9, 0x24, 0xb0, 0x52, 0x49, 0x54, 0x92, 0x49, 0x55, 0x85,
    0x52, 0x49, 0x25, 0x52, 0x49, 0x2f, 0xff, 0xc4, 0x00, 0x14, 0x10, 0x01,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0xa0, 0xff, 0xda, 0x00, 0x08, 0x01, 0x01, 0x00, 0x01,
    0x3f, 0x00, 0x00, 0x1f, 0xff, 0xc4, 0x00, 0x14, 0x11, 0x01, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x80, 0xff, 0xda, 0x00, 0x08, 0x01, 0x02, 0x01, 0x01, 0x3f, 0x00,
    0x00, 0x7f, 0xff, 0xc4, 0x00, 0x15, 0x11, 0x01, 0x01, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x01, 0xff, 0xda, 0x00, 0x08, 0x01, 0x03, 0x01, 0x01, 0x3f, 0x00, 0x88,
    0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88,
    0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88,
    0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88,
    0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88,
    0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88,
    0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88,
    0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88,
    0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88,
    0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88,
    0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88,
    0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0xff, 0xd9,
};

static constexpr unsigned char kAvif[] = {
    0x00, 0x00, 0x00, 0x1c, 0x66, 0x74, 0x79, 0x70, 0x6d, 0x69, 0x66, 0x31,
    0x00, 0x00, 0x00, 0x00, 0x6d, 0x69, 0x66, 0x31, 0x61, 0x76, 0x69, 0x66,
    0x6d, 0x69, 0x61, 0x66, 0x00, 0x00, 0x00, 0xf3, 0x6d, 0x65, 0x74, 0x61,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x21, 0x68, 0x64, 0x6c, 0x72,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x70, 0x69, 0x63, 0x74,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x0e, 0x70, 0x69, 0x74, 0x6d, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x1e, 0x69, 0x6c, 0x6f, 0x63, 0x00,
    0x00, 0x00, 0x00, 0x04, 0x40, 0x00, 0x01, 0x00, 0x01, 0x00, 0x00, 0x00,
    0x00, 0x01, 0x17, 0x00, 0x01, 0x00, 0x00, 0x00, 0x7c, 0x00, 0x00, 0x00,
    0x28, 0x69, 0x69, 0x6e, 0x66, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00,
    0x00, 0x00, 0x1a, 0x69, 0x6e, 0x66, 0x65, 0x02, 0x00, 0x00, 0x00, 0x00,
    0x01, 0x00, 0x00, 0x61, 0x76, 0x30, 0x31, 0x49, 0x6d, 0x61, 0x67, 0x65,
    0x00, 0x00, 0x00, 0x00, 0x72, 0x69, 0x70, 0x72, 0x70, 0x00, 0x00, 0x00,
    0x53, 0x69, 0x70, 0x63, 0x6f, 0x00, 0x00, 0x00, 0x14, 0x69, 0x73, 0x70,
    0x65, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01,
    0x00, 0x00, 0x00, 0x00, 0x10, 0x70, 0x61, 0x73, 0x70, 0x00, 0x00, 0x00,
    0x01, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x17, 0x61, 0x76, 0x31,
    0x43, 0x81, 0x20, 0x00, 0x00, 0x0a, 0x09, 0x38, 0x1d, 0xff, 0xff, 0xda,
    0x40, 0x43, 0x40, 0x08, 0x00, 0x00, 0x00, 0x10, 0x70, 0x69, 0x78, 0x69,
    0x00, 0x00, 0x00, 0x00, 0x03, 0x08, 0x08, 0x08, 0x00, 0x00, 0x00, 0x17,
    0x69, 0x70, 0x6d, 0x61, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01,
    0x00, 0x01, 0x04, 0x01, 0x02, 0x83, 0x84, 0x00, 0x00, 0x00, 0x84, 0x6d,
    0x64, 0x61, 0x74, 0x0a, 0x09, 0x38, 0x1d, 0xff, 0xff, 0xda, 0x40, 0x43,
    0x40, 0x08, 0x32, 0x6f, 0x11, 0x90, 0x00, 0x00, 0x12, 0x5d, 0x60, 0x34,
    0x19, 0x59, 0x91, 0x94, 0xa0, 0x1e, 0xc3, 0xc8, 0xed, 0x0a, 0x11, 0xa0,
    0x57, 0x69, 0x46, 0xdf, 0x0a, 0xba, 0x02, 0x75, 0x9d, 0x7c, 0xa1, 0x38,
    0x79, 0x42, 0x29, 0x4a, 0xe2, 0x15, 0xd8, 0xdb, 0x9f, 0xf6, 0x57, 0xf3,
    0xa9, 0x38, 0xff, 0x39, 0x49, 0x78, 0x14, 0x47, 0xc7, 0xc5, 0x2d, 0x9c,
    0xd4, 0x7b, 0xde, 0xda, 0x53, 0x0c, 0x28, 0x31, 0x67, 0xe8, 0xa4, 0x0e,
    0xe7, 0x7a, 0xe9, 0xdd, 0xbd, 0x0f, 0x48, 0x83, 0x49, 0xc6, 0x1c, 0xb5,
    0xf7, 0x78, 0x65, 0xb1, 0x17, 0x2a, 0x67, 0x32, 0x54, 0xae, 0x0e, 0xa9,
    0x86, 0xc8, 0xea, 0xba, 0x38, 0xbe, 0x29, 0xb6, 0xc1, 0xc3, 0x93, 0x0a,
    0x06, 0x72, 0x4a, 0x09, 0x69, 0x41, 0x59,
};

static constexpr unsigned char kTiff[] = {
    0x49, 0x49, 0x2a, 0x00, 0xdc, 0x05, 0x00, 0x00, 0x78, 0xda, 0xed, 0xd2,
    0xc3, 0x16, 0x28, 0x86, 0x01, 0x45, 0xd1, 0x17, 0xdb, 0x49, 0xe3, 0xa4,
    0x31, 0xdb, 0xd8, 0x6a, 0x1a, 0xdb, 0x6e, 0x6c, 0xdb, 0x6e, 0x6c, 0xdb,
    0xb6, 0x6d, 0xdb, 0xb6, 0x6d, 0xdb, 0x7f, 0x71, 0x06, 0xd9, 0x6b, 0xed,
    0x3b, 0x3f, 0x83, 0x3b, 0x60, 0xc0, 0x80, 0x81, 0xe0, 0xef, 0xea, 0xaf,
    0xe5, 0x0d, 0x90, 0xfd, 0x7f, 0xe0, 0xbe, 0x01, 0xb2, 0xff, 0x0f, 0xd2,
    0x37, 0x40, 0xf6, 0xff, 0x41, 0xfb, 0x06, 0xc8, 0xfe, 0x3f, 0x58, 0xdf,
    0x00, 0xd9, 0xff, 0x07, 0xef, 0x1b, 0x20, 0xfb, 0xff, 0x10, 0x7d, 0x03,
    0x64, 0xff, 0x1f, 0xb2, 0x6f, 0x80, 0xec, 0xff, 0x43, 0xf5, 0x0d, 0x90,
    0xfd, 0x7f, 0xe8, 0xbe, 0x01, 0xb2, 0xff, 0x0f, 0xd3, 0x37, 0x40, 0xf6,
    0xff, 0x61, 0xfb, 0x06, 0xc8, 0xfe, 0x3f, 0x5c, 0xdf, 0x00, 0xd9, 0xff,
    0x87, 0xef, 0x1b, 0x20, 0xfb, 0xff, 0x08, 0x7d, 0x03, 0x64, 0xff, 0x1f,
    0xb1, 0x6f, 0x80, 0xec, 0xff, 0x23, 0xf5, 0x0d, 0x90, 0xfd, 0x7f, 0xe4,
    0xbe, 0x01, 0xb2, 0xff, 0x8f, 0xd2, 0x37, 0x40, 0xf6, 0xff, 0x51, 0xfb,
    0x06, 0xc8, 0xfe, 0x3f, 0x5a, 0xdf, 0x00, 0xd9, 0xff, 0xff, 0xd1, 0x37,
    0x40, 0xf6, 0xff, 0xd1, 0xfb, 0x06, 0xc8, 0xfe, 0x3f, 0x46, 0xdf, 0x00,
    0xd9, 0xff, 0xc7, 0xec, 0x1b, 0x20, 0xfb, 0xff, 0x58, 0x7d, 0x03, 0x64,
    0xff, 0x1f, 0xbb, 0x6f, 0x80, 0xec, 0xff, 0xe3, 0xf4, 0x0d, 0x90, 0xfd,
    0x7f, 0xdc, 0xbe, 0x01, 0xb2, 0xff, 0x8f, 0xd7, 0x37, 0x40, 0xf6, 0xff,
    0xf1, 0xfb, 0x06, 0xc8, 0xfe, 0xff, 0xcf, 0xbe, 0x01, 0xb2, 0xff, 0x4f,
    0xd0, 0x37, 0x40, 0xf6, 0xff, 0x09, 0xfb, 0x06, 0xc8, 0xfe, 0x3f, 0x51,
    0xdf, 0x00, 0xd9, 0xff, 0x27, 0xee, 0x1b, 0x20, 0xfb, 0xff, 0x24, 0x7d,
    0x03, 0x64, 0xff, 0x9f, 0xb4, 0x6f, 0x80, 0xec, 0xff, 0x93, 0xf5, 0x0d,
    0x90, 0xfd, 0x7f, 0xf2, 0xbe, 0x01, 0xb2, 0xff, 0x4f, 0xd1, 0x37, 0x40,
    0xf6, 0xff, 0x29, 0xfb, 0x06, 0xc8, 0xfe, 0x3f, 0x55, 0xdf, 0x00, 0xd9,
    0xff, 0xa7, 0xee, 0x1b, 0x20, 0xfb, 0xff, 0xbf, 0xfa, 0x06, 0xc8, 0xfe,
    0xff, 0xef, 0xbe, 0x01, 0xb2, 0xff, 0x4f, 0xd3, 0x37, 0x40, 0xf6, 0xff,
    0x69, 0xfb, 0x06, 0xc8, 0xfe, 0x3f, 0x5d, 0xdf, 0x00, 0xd9, 0xff, 0xa7,
    0xef, 0x1b, 0x20, 0xfb, 0xff, 0x0c, 0x7d, 0x03, 0x64, 0xff, 0x9f, 0xb1,
    0x6f, 0x80, 0xec, 0xff, 0x33, 0xf5, 0x0d, 0x90, 0xfd, 0x7f, 0xe6, 0xbe,
    0x01, 0xb2, 0xff, 0xcf, 0xd2, 0x37, 0x40, 0xf6, 0xff, 0x59, 0xfb, 0x06,
    0xc8, 0xfe, 0x3f, 0x5b, 0xdf, 0x00, 0xd9, 0xff, 0x67, 0xef, 0x1b, 0x20,
    0xfb, 0xff, 0x1c, 0x7d, 0x03, 0x64, 0xff, 0x9f, 0xb3, 0x6f, 0x80, 0xec,
    0xff, 0x73, 0xf5, 0x0d, 0x90, 0xfd, 0x7f, 0xee, 0xbe, 0x01, 0xb2, 0xff,
    0xcf, 0xd3, 0x37, 0x40, 0xf6, 0xff, 0xff, 0xf4, 0x0d, 0x90, 0xfd, 0x7f,
    0xde, 0xbe, 0x01, 0xb2, 0xff, 0xff, 0xb7, 0x6f, 0x80, 0xec, 0xff, 0xf3,
    0xf5, 0x0d, 0x90, 0xfd, 0x7f, 0xfe, 0xbe, 0x01, 0xb2, 0xff, 0x2f, 0xd0,
    0x37, 0x40, 0xf6, 0xff, 0x05, 0xfb, 0x06, 0xc8, 0xfe, 0xbf, 0x50, 0xdf,
    0x00, 0xd9, 0xff, 0x17, 0xee, 0x1b, 0x20, 0xfb, 0xff, 0x22, 0x7d, 0x03,
    0x64, 0xff, 0x5f, 0xb4, 0x6f, 0x80, 0xec, 0xff, 0x8b, 0xf5, 0x0d, 0x90,
    0xfd, 0x7f, 0xf1, 0xbe, 0x01, 0xb2, 0xff, 0x2f, 0xd1, 0x37, 0x40, 0xf6,
    0xff, 0x25, 0xfb, 0x06, 0xc8, 0xfe, 0xbf, 0x54, 0xdf, 0x00, 0xd9, 0xff,
    0x97, 0xee, 0x1b, 0x20, 0xfb, 0xff, 0x32, 0x7d, 0x03, 0x64, 0xff, 0x5f,
    0xb6, 0x6f, 0x80, 0xec, 0xff, 0xcb, 0xf5, 0x0d, 0x90, 0xfd, 0x7f, 0xf9,
    0xbe, 0x01, 0xb2, 0xff, 0xaf, 0xd0, 0x37, 0x40, 0xf6, 0xff, 0x15, 0xfb,
    0x06, 0xc8, 0xfe, 0xbf, 0x52, 0xdf, 0x00, 0xd9, 0xff, 0x57, 0xee, 0x1b,
    0x20, 0xfb, 0xff, 0x2a, 0x7d, 0x03, 0x64, 0xff, 0x5f, 0xb5, 0x6f, 0x80,
    0xec, 0xff, 0xab, 0xf5, 0x0d, 0x90, 0xfd, 0xff, 0x7f, 0x7d, 0x03, 0x64,
    0xff, 0x5f, 0xbd, 0x6f, 0x80, 0xec, 0xff, 0x6b, 0xf4, 0x0d, 0x90, 0xfd,
    0x7f, 0xcd, 0xbe, 0x01, 0xb2, 0xff, 0xaf, 0xd5, 0x37, 0x40, 0xf6, 0xff,
    0xb5, 0xfb, 0x06, 0xc8, 0xfe, 0xbf, 0x4e, 0xdf, 0x00, 0xd9, 0xff, 0xd7,
    0xed, 0x1b, 0x20, 0xfb, 0xff, 0x7a, 0x7d, 0x03, 0x64, 0xff, 0x5f, 0xbf,
    0x6f, 0x80, 0xec, 0xff, 0x1b, 0xf4, 0x0d, 0x90, 0xfd, 0x7f, 0xc3, 0xbe,
    0x01, 0xb2, 0xff, 0x6f, 0xd4, 0x37, 0x40, 0xf6, 0xff, 0x8d, 0xfb, 0x06,
    0xc8, 0xfe, 0xbf, 0x49, 0xdf, 0x00, 0xd9, 0xff, 0x37, 0xed, 0x1b, 0x20,
    0xfb, 0xff, 0x66, 0x7d, 0x03, 0x64, 0xff, 0xdf, 0xbc, 0x6f, 0x80, 0xec,
    0xff, 0x5b, 0xf4, 0x0d, 0x90, 0xfd, 0x7f, 0xcb, 0xbe, 0x01, 0xb2, 0xff,
    0x6f, 0xd5, 0x37, 0x40, 0xf6, 0xff, 0xad, 0xfb, 0x06, 0xc8, 0xfe, 0xbf,
    0x4d, 0xdf, 0x00, 0xd9, 0xff, 0xb7, 0xed, 0x1b, 0x20, 0xfb, 0xff, 0x76,
    0x7d, 0x03, 0x64, 0xff, 0xdf, 0xbe, 0x6f, 0x80, 0xec, 0xff, 0x3b, 0xf4,
    0x0d, 0x90, 0xfd, 0x7f, 0xc7, 0xbe, 0x01, 0xb2, 0xff, 0xef, 0xd4, 0x37,
    0x40, 0xf6, 0xff, 0x9d, 0xfb, 0x06, 0xc8, 0xfe, 0xbf, 0x4b, 0xdf, 0x00,
    0xd9, 0xff, 0x77, 0xed, 0x1b, 0x20, 0xfb, 0xff, 0x6e, 0x7d, 0x03, 0x64,
    0xff, 0xdf, 0xbd, 0x6f, 0x80, 0xec, 0xff, 0x7b, 0xf4, 0x0d, 0x90, 0xfd,
    0x7f, 0xcf, 0xbe, 0x01, 0xb2, 0xff, 0xef, 0xd5, 0x37, 0x40, 0xf6, 0xff,
    0xbd, 0xfb, 0x06, 0xc8, 0xfe, 0xff, 0xff, 0xbe, 0x01, 0xb2, 0xff, 0xef,
    0xd3, 0x37, 0x40, 0xf6, 0xff, 0x7d, 0xfb, 0x06, 0xc8, 0xfe, 0xbf, 0x5f,
    0xdf, 0x00, 0xd9, 0xff, 0xf7, 0xef, 0x1b, 0x20, 0xfb, 0xff, 0x01, 0x7d,
    0x03, 0x64, 0xff, 0x3f, 0xb0, 0x6f, 0x80, 0xec, 0xff, 0x07, 0xf5, 0x0d,
    0x90, 0xfd, 0xff, 0xe0, 0xbe, 0x01, 0xb2, 0xff, 0x1f, 0xd2, 0x37, 0x40,
    0xf6, 0xff, 0x43, 0xfb, 0x06, 0xc8, 0xfe, 0x7f, 0x58, 0xdf, 0x00, 0xd9,
    0xff, 0x0f, 0xef, 0x1b, 0x20, 0xfb, 0xff, 0x11, 0x7d, 0x03, 0x64, 0xff,
    0x3f, 0xb2, 0x6f, 0x80, 0xec, 0xff, 0x47, 0xf5, 0x0d, 0x90, 0xfd, 0xff,
    0xe8, 0xbe, 0x01, 0xb2, 0xff, 0x1f, 0xd3, 0x37, 0x40, 0xf6, 0xff, 0x63,
    0xfb, 0x06, 0xc8, 0xfe, 0x7f, 0x5c, 0xdf, 0x00, 0xd9, 0xff, 0x8f, 0xef,
    0x1b, 0x20, 0xfb, 0xff, 0x09, 0x7d, 0x03, 0x64, 0xff, 0x3f, 0xb1, 0x6f,
    0x80, 0xec, 0xff, 0x27, 0xf5, 0x0d, 0x90, 0xfd, 0xff, 0xe4, 0xbe, 0x01,
    0xb2, 0xff, 0x9f, 0xd2, 0x37, 0x40, 0xf6, 0xff, 0x53, 0xfb, 0x06, 0xc8,
    0xfe, 0x7f, 0x5a, 0xdf, 0x00, 0xd9, 0xff, 0x4f, 0xef, 0x1b, 0x20, 0xfb,
    0xff, 0x19, 0x7d, 0x03, 0x64, 0xff, 0x3f, 0xb3, 0x6f, 0x80, 0xec, 0xff,
    0x67, 0xf5, 0x0d, 0x90, 0xfd, 0xff, 0xec, 0xbe, 0x01, 0xb2, 0xff, 0x9f,
    0xd3, 0x37, 0x40, 0xf6, 0xff, 0x73, 0xfb, 0x06, 0xc8, 0xfe, 0x7f, 0x5e,
    0xdf, 0x00, 0xd9, 0xff, 0xcf, 0xef, 0x1b, 0x20, 0xfb, 0xff, 0x05, 0x7d,
    0x03, 0x64, 0xff, 0xbf, 0xb0, 0x6f, 0x80, 0xec, 0xff, 0x17, 0xf5, 0x0d,
    0x90, 0xfd, 0xff, 0xe2, 0xbe, 0x01, 0xb2, 0xff, 0x5f, 0xd2, 0x37, 0x40,
    0xf6, 0xff, 0x4b, 0xfb, 0x06, 0xc8, 0xfe, 0x7f, 0x59, 0xdf, 0x00, 0xd9,
    0xff, 0x2f, 0xef, 0x1b, 0x20, 0xfb, 0xff, 0x15, 0x7d, 0x03, 0x64, 0xff,
    0xbf, 0xb2, 0x6f, 0x80, 0xec, 0xff, 0x57, 0xf5, 0x0d, 0x90, 0xfd, 0xff,
    0xea, 0xbe, 0x01, 0xb2, 0xff, 0x5f, 0xd3, 0x37, 0x40, 0xf6, 0xff, 0x6b,
    0xfb, 0x06, 0xc8, 0xfe, 0x7f, 0x5d, 0xdf, 0x00, 0xd9, 0xff, 0xaf, 0xef,
    0x1b, 0x20, 0xfb, 0xff, 0x0d, 0x7d, 0x03, 0x64, 0xff, 0xbf, 0xb1, 0x6f,
    0x80, 0xec, 0xff, 0x37, 0xf5, 0x0d, 0x90, 0xfd, 0xff, 0xe6, 0xbe, 0x01,
    0xb2, 0xff, 0xdf, 0xd2, 0x37, 0x40, 0xf6, 0xff, 0x5b, 0xfb, 0x06, 0xc8,
    0xfe, 0x7f, 0x5b, 0xdf, 0x00, 0xd9, 0xff, 0x6f, 0xef, 0x1b, 0x20, 0xfb,
    0xff, 0x1d, 0x7d, 0x03, 0x64, 0xff, 0xbf, 0xb3, 0x6f, 0x80, 0xec, 0xff,
    0x77, 0xf5, 0x0d, 0x90, 0xfd, 0xff, 0xee, 0xbe, 0x01, 0xb2, 0xff, 0xdf,
    0xd3, 0x37, 0x40, 0xf6, 0xff, 0x7b, 0xfb, 0x06, 0xc8, 0xfe, 0x7f, 0x5f,
    0xdf, 0x00, 0xd9, 0xff, 0xef, 0xef, 0x1b, 0x20, 0xfb, 0xff, 0x03, 0x7d,
    0x03, 0x64, 0xff, 0x7f, 0xb0, 0x6f, 0x80, 0xec, 0xff, 0x0f, 0xf5, 0x0d,
    0x90, 0xfd, 0xff, 0xe1, 0xbe, 0x01, 0xb2, 0xff, 0x3f, 0xd2, 0x37, 0x40,
    0xf6, 0xff, 0x47, 0xfb, 0x06, 0xc8, 0xfe, 0xff, 0x58, 0xdf, 0x00, 0xd9,
    0xff, 0x1f, 0xef, 0x1b, 0x20, 0xfb, 0xff, 0x13, 0x7d, 0x03, 0x64, 0xff,
    0x7f, 0xb2, 0x6f, 0x80, 0xec, 0xff, 0x4f, 0xf5, 0x0d, 0x90, 0xfd, 0xff,
    0xe9, 0xbe, 0x01, 0xb2, 0xff, 0x3f, 0xd3, 0x37, 0x40, 0xf6, 0xff, 0x67,
    0xfb, 0x06, 0xc8, 0xfe, 0xff, 0x5c, 0xdf, 0x00, 0xd9, 0xff, 0x9f, 0xef,
    0x1b, 0x20, 0xfb, 0xff, 0x0b, 0x7d, 0x03, 0x64, 0xff, 0x7f, 0xb1, 0x6f,
    0x80, 0xec, 0xff, 0x2f, 0xf5, 0x0d, 0x90, 0xfd, 0xff, 0xe5, 0xbe, 0x01,
    0xb2, 0xff, 0xbf, 0xd2, 0x37, 0x40, 0xf6, 0xff, 0x57, 0xfb, 0x06, 0xc8,
    0xfe, 0xff, 0x5a, 0xdf, 0x00, 0xd9, 0xff, 0x5f, 0xef, 0x1b, 0x20, 0xfb,
    0xff, 0x1b, 0x7d, 0x03, 0x64, 0xff, 0x7f, 0xb3, 0x6f, 0x80, 0xec, 0xff,
    0x6f, 0xf5, 0x0d, 0x90, 0xfd, 0xff, 0xed, 0xbe, 0x01, 0xb2, 0xff, 0xbf,
    0xd3, 0x37, 0x40, 0xf6, 0xff, 0x77, 0xfb, 0x06, 0xc8, 0xfe, 0xff, 0x5e,
    0xdf, 0x00, 0xd9, 0xff, 0xdf, 0xef, 0x1b, 0x20, 0xfb, 0xff, 0x07, 0x7d,
    0x03, 0x64, 0xff, 0xff, 0xb0, 0x6f, 0x80, 0xec, 0xff, 0x1f, 0xf5, 0x0d,
    0x90, 0xfd, 0xff, 0xe3, 0xbe, 0x01, 0xb2, 0xff, 0x7f, 0xd2, 0x37, 0x40,
    0xf6, 0xff, 0x4f, 0xfb, 0x06, 0xc8, 0xfe, 0xff, 0x59, 0xdf, 0x00, 0xd9,
    0xff, 0x3f, 0xef, 0x1b, 0x20, 0xfb, 0xff, 0x17, 0x7d, 0x03, 0x64, 0xff,
    0xff, 0xb2, 0x6f, 0x80, 0xec, 0xff, 0x5f, 0xf5, 0x0d, 0x90, 0xfd, 0xff,
    0xeb, 0xbe, 0x01, 0xb2, 0xff, 0x7f, 0xd3, 0x37, 0x40, 0xf6, 0xff, 0x6f,
    0xfb, 0x06, 0xc8, 0xfe, 0xff, 0x5d, 0xdf, 0x00, 0xd9, 0xff, 0xbf, 0xef,
    0x1b, 0x20, 0xfb, 0xff, 0x0f, 0x7d, 0x03, 0x64, 0xff, 0xff, 0xb1, 0x6f,
    0x80, 0xec, 0xff, 0x3f, 0xf5, 0x0d, 0x90, 0xfd, 0xff, 0xe7, 0xbe, 0x01,
    0xb2, 0xff, 0xff, 0xd2, 0x37, 0x40, 0xf6, 0xff, 0x5f, 0xfb, 0x06, 0xc8,
    0xfe, 0xff, 0x5b, 0xdf, 0x00, 0xd9, 0xff, 0x7f, 0xef, 0x1b, 0x20, 0xfb,
    0xff, 0x1f, 0x7d, 0x03, 0x54, 0xfe, 0x04, 0xeb, 0xd1, 0x7e, 0x90, 0x00,
    0x13, 0x00, 0x00, 0x01, 0x03, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x01,
    0x00, 0x00, 0x01, 0x01, 0x03, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x01,
    0x00, 0x00, 0x02, 0x01, 0x03, 0x00, 0x03, 0x00, 0x00, 0x00, 0xd6, 0x06,
    0x00, 0x00, 0x03, 0x01, 0x03, 0x00, 0x01, 0x00, 0x00, 0x00, 0x08, 0x00,
    0x00, 0x00, 0x06, 0x01, 0x03, 0x00, 0x01, 0x00, 0x00, 0x00, 0x02, 0x00,
    0x00, 0x00, 0x0a, 0x01, 0x03, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01, 0x00,
    0x00, 0x00, 0x11, 0x01, 0x04, 0x00, 0x01, 0x00, 0x00, 0x00, 0x08, 0x00,
    0x00, 0x00, 0x12, 0x01, 0x03, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01, 0x00,
    0x00, 0x00, 0x15, 0x01, 0x03, 0x00, 0x01, 0x00, 0x00, 0x00, 0x03, 0x00,
    0x00, 0x00, 0x16, 0x01, 0x03, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x01,
    0x00, 0x00, 0x17, 0x01, 0x04, 0x00, 0x01, 0x00, 0x00, 0x00, 0xd3, 0x05,
    0x00, 0x00, 0x1a, 0x01, 0x05, 0x00, 0x01, 0x00, 0x00, 0x00, 0xc6, 0x06,
    0x00, 0x00, 0x1b, 0x01, 0x05, 0x00, 0x01, 0x00, 0x00, 0x00, 0xce, 0x06,
    0x00, 0x00, 0x1c, 0x01, 0x03, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01, 0x00,
    0x00, 0x00, 0x28, 0x01, 0x03, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01, 0x00,
    0x00, 0x00, 0x29, 0x01, 0x03, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x01, 0x00, 0x3d, 0x01, 0x03, 0x00, 0x01, 0x00, 0x00, 0x00, 0x02, 0x00,
    0x00, 0x00, 0x3e, 0x01, 0x05, 0x00, 0x02, 0x00, 0x00, 0x00, 0x0c, 0x07,
    0x00, 0x00, 0x3f, 0x01, 0x05, 0x00, 0x06, 0x00, 0x00, 0x00, 0xdc, 0x06,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x48, 0x00, 0x00, 0x00, 0x01, 0x00,
    0x00, 0x00, 0x48, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x08, 0x00,
    0x08, 0x00, 0x08, 0x00, 0x85, 0xeb, 0x51, 0x00, 0x00, 0x00, 0x80, 0x00,
    0xc3, 0xf5, 0xa8, 0x00, 0x00, 0x00, 0x00, 0x02, 0xcd, 0xcc, 0x4c, 0x00,
    0x00, 0x00, 0x00, 0x01, 0xcd, 0xcc, 0x4c, 0x00, 0x00, 0x00, 0x80, 0x00,
    0xcd, 0xcc, 0x4c, 0x00, 0x00, 0x00, 0x00, 0x02, 0x8f, 0xc2, 0xf5, 0x00,
    0x00, 0x00, 0x00, 0x10, 0x37, 0x1a, 0xa0, 0x00, 0x00, 0x00, 0x00, 0x02,
    0x2b, 0x87, 0x0a, 0x00, 0x00, 0x00, 0x20, 0x00,
};

::nlohmann::json GetSpec(std::string driver, std::string path) {
  return ::nlohmann::json{
      {"driver", driver},
      {"kvstore", {{"driver", "memory"}, {"path", path}}},
  };
}

struct P {
  std::string driver;
  std::string path;
  absl::Cord data;

  /// Values at predefined points in the image.
  /// image drivers return c-order: y, x, c
  uint8_t a[3] = {100, 50, 0};   // (50,100)
  uint8_t b[3] = {200, 100, 0};  // (100,200)
};

// Implements ::testing::PrintToStringParamName().
static std::string PrintToString(const P& p) { return p.driver; }

P ParamPng() {
  return {
      "png", "a.png",
      absl::MakeCordFromExternal(
          absl::string_view(reinterpret_cast<const char*>(kPng), sizeof(kPng)),
          [] {})};
}

P ParamJpeg() {
  return {
      "jpeg",
      "b.jpg",
      absl::MakeCordFromExternal(
          absl::string_view(reinterpret_cast<const char*>(kJpeg),
                            sizeof(kJpeg)),
          [] {}),
      {98, 55, 2},   // (50,100)
      {200, 104, 1}  // (100,200)
  };
}

P ParamAvif() {
  return {"avif", "c.avif",
          absl::MakeCordFromExternal(
              absl::string_view(reinterpret_cast<const char*>(kAvif),
                                sizeof(kAvif)),
              [] {})};
}

P ParamTiff() {
  return {"tiff", "d.tiff",
          absl::MakeCordFromExternal(
              absl::string_view(reinterpret_cast<const char*>(kTiff),
                                sizeof(kTiff)),
              [] {})};
}

class ImageDriverReadTest : public ::testing::TestWithParam<P> {
 public:
  tensorstore::Result<tensorstore::Context> PrepareTest(
      ::nlohmann::json& spec) {
    auto context = tensorstore::Context::Default();
    TENSORSTORE_ASSIGN_OR_RETURN(
        auto kvs,
        tensorstore::kvstore::Open(spec["kvstore"], context).result());
    TENSORSTORE_RETURN_IF_ERROR(
        tensorstore::kvstore::Write(kvs, {}, GetParam().data));
    return context;
  }
};

INSTANTIATE_TEST_SUITE_P(ReadTests, ImageDriverReadTest,
                         testing::Values(ParamPng(), ParamJpeg(), ParamAvif(),
                                         ParamTiff()),
                         testing::PrintToStringParamName());

TEST_P(ImageDriverReadTest, Handle_OpenResolveBounds) {
  auto json_spec = GetSpec(GetParam().driver, GetParam().path);
  TENSORSTORE_ASSERT_OK_AND_ASSIGN(auto context, PrepareTest(json_spec));

  TENSORSTORE_ASSERT_OK_AND_ASSIGN(auto spec,
                                   tensorstore::Spec::FromJson(json_spec));

  tensorstore::TransactionalOpenOptions options;
  TENSORSTORE_ASSERT_OK(options.Set(context));
  TENSORSTORE_ASSERT_OK_AND_ASSIGN(
      auto handle,
      tensorstore::internal::OpenDriver(
          std::move(tensorstore::internal_spec::SpecAccess::impl(spec)),
          std::move(options))
          .result());

  auto transform_result =
      handle.driver
          ->ResolveBounds(
              {}, tensorstore::IdentityTransform(handle.driver->rank()), {})
          .result();

  EXPECT_THAT(transform_result->input_origin(),
              ::testing::ElementsAre(0, 0, 0));
  EXPECT_THAT(transform_result->input_shape(),
              ::testing::ElementsAre(256, 256, 3));
}

TEST_P(ImageDriverReadTest, OpenAndResolveBounds) {
  auto spec = GetSpec(GetParam().driver, GetParam().path);
  TENSORSTORE_ASSERT_OK_AND_ASSIGN(auto context, PrepareTest(spec));

  TENSORSTORE_ASSERT_OK_AND_ASSIGN(auto store,
                                   tensorstore::Open(spec, context).result());

  {
    TENSORSTORE_ASSERT_OK_AND_ASSIGN(auto spec, store.spec());
    EXPECT_THAT(
        spec.ToJson(),
        ::testing::Optional(MatchesJson({
            {"driver", GetParam().driver},
            {"dtype", "uint8"},
            {"kvstore", {{"driver", "memory"}, {"path", GetParam().path}}},
            {"transform",
             {
                 {"input_exclusive_max", {256, 256, 3}},
                 {"input_inclusive_min", {0, 0, 0}},
             }},
        })));
  }

  TENSORSTORE_ASSERT_OK_AND_ASSIGN(auto resolved,
                                   ResolveBounds(store).result());

  // Bounds are effectively resolved at open.
  {
    TENSORSTORE_ASSERT_OK_AND_ASSIGN(auto spec, resolved.spec());
    EXPECT_THAT(
        spec.ToJson(),
        ::testing::Optional(MatchesJson({
            {"driver", GetParam().driver},
            {"dtype", "uint8"},
            {"kvstore", {{"driver", "memory"}, {"path", GetParam().path}}},
            {"transform",
             {
                 {"input_exclusive_max", {256, 256, 3}},
                 {"input_inclusive_min", {0, 0, 0}},
             }},
        })));
  }
}

TEST_P(ImageDriverReadTest, Read) {
  auto spec = GetSpec(GetParam().driver, GetParam().path);
  TENSORSTORE_ASSERT_OK_AND_ASSIGN(auto context, PrepareTest(spec));

  // Path is embedded in kvstore, so we don't write it.
  TENSORSTORE_ASSERT_OK_AND_ASSIGN(auto store,
                                   tensorstore::Open(spec, context).result());

  TENSORSTORE_ASSERT_OK_AND_ASSIGN(auto array,
                                   tensorstore::Read(store).result());

  EXPECT_THAT(array.shape(), ::testing::ElementsAre(256, 256, 3));
  EXPECT_THAT(array[50][100], tensorstore::MakeArray<uint8_t>(GetParam().a));
  EXPECT_THAT(array[100][200], tensorstore::MakeArray<uint8_t>(GetParam().b));
}

TEST_P(ImageDriverReadTest, ReadWithTransform) {
  auto spec = GetSpec(GetParam().driver, GetParam().path);
  TENSORSTORE_ASSERT_OK_AND_ASSIGN(auto context, PrepareTest(spec));

  TENSORSTORE_ASSERT_OK_AND_ASSIGN(auto store,
                                   tensorstore::Open(spec, context).result());

  auto transformed =
      store | tensorstore::Dims(0, 1).SizedInterval({100, 200}, {1, 1});
  TENSORSTORE_ASSERT_OK_AND_ASSIGN(
      auto array,
      tensorstore::Read<tensorstore::zero_origin>(transformed).result());

  EXPECT_THAT(array.shape(), ::testing::ElementsAre(1, 1, 3));
  EXPECT_THAT(array[0][0], tensorstore::MakeArray<uint8_t>(GetParam().b));
}

TEST_P(ImageDriverReadTest, ReadTransactionError) {
  auto spec = GetSpec(GetParam().driver, GetParam().path);
  TENSORSTORE_ASSERT_OK_AND_ASSIGN(auto context, PrepareTest(spec));

  // Open with transaction succeeds
  tensorstore::Transaction transaction(tensorstore::TransactionMode::isolated);
  TENSORSTORE_ASSERT_OK_AND_ASSIGN(
      auto store, tensorstore::Open(spec, context, transaction).result());

  // But read/write/resolve are unsupported.
  EXPECT_THAT(
      tensorstore::Read(store).result(),
      MatchesStatus(absl::StatusCode::kUnimplemented, ".*transaction.*"));
}

TEST_P(ImageDriverReadTest, MissingPath_Open) {
  auto context = tensorstore::Context::Default();
  auto spec = GetSpec(GetParam().driver, GetParam().path);
  EXPECT_THAT(tensorstore::Open(spec).result(),
              MatchesStatus(absl::StatusCode::kNotFound));
}

TEST(ImageDriverErrors, NoKvStore) {
  EXPECT_THAT(
      tensorstore::Open({
                            {"driver", "png"},
                        })
          .result(),
      MatchesStatus(absl::StatusCode::kInvalidArgument, ".*\"kvstore\".*"));
}

TEST(ImageDriverErrors, Mode) {
  for (auto mode : {tensorstore::ReadWriteMode::write,
                    tensorstore::ReadWriteMode::read_write}) {
    SCOPED_TRACE(tensorstore::StrCat("mode=", mode));
    EXPECT_THAT(tensorstore::Open(
                    {
                        {"driver", "png"},
                        {"dtype", "uint8"},
                        {"kvstore", {{"driver", "memory"}, {"path", "a.png"}}},
                    },
                    mode)
                    .result(),
                MatchesStatus(absl::StatusCode::kInvalidArgument,
                              ".*: only reading is supported"));
  }
}

TEST(ImageDriverErrors, RankMismatch) {
  EXPECT_THAT(tensorstore::Open(
                  {
                      {"driver", "png"},
                      {"kvstore", {{"driver", "memory"}, {"path", "a.png"}}},
                      {"schema",
                       {
                           {"domain", {{"rank", 2}}},
                       }},
                  })
                  .result(),
              MatchesStatus(absl::StatusCode::kInvalidArgument, ".*rank.*"));
}

TEST(ImageDriverErrors, DomainOrigin) {
  EXPECT_THAT(tensorstore::Open(
                  {
                      {"driver", "png"},
                      {"kvstore", {{"driver", "memory"}, {"path", "a.png"}}},
                      {"schema",
                       {
                           {"domain", {{"inclusive_min", {0, 0, 1}}}},
                       }},
                  })
                  .result(),
              MatchesStatus(absl::StatusCode::kInvalidArgument, ".*origin.*"));
}

TEST(ImageDriverErrors, DimensionUnits) {
  EXPECT_THAT(tensorstore::Open(
                  {
                      {"driver", "png"},
                      {"kvstore", {{"driver", "memory"}, {"path", "a.png"}}},
                      {"schema",
                       {
                           {"dimension_units", {"1ft", "2ft"}},
                       }},
                  })
                  .result(),
              // dimension_units sets schema.rank
              MatchesStatus(absl::StatusCode::kInvalidArgument, ".*rank.*"));
}

// TODO: schema.fill_value

}  // namespace
