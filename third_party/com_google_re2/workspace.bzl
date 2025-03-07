# Copyright 2021 The TensorStore Authors
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

load(
    "//third_party:repo.bzl",
    "third_party_http_archive",
)
load("@bazel_tools//tools/build_defs/repo:utils.bzl", "maybe")
load("//:cmake_helpers.bzl", "cmake_add_dep_mapping", "cmake_fetch_content_package")

# REPO_BRANCH = abseil

def repo():
    maybe(
        third_party_http_archive,
        name = "com_google_re2",
        strip_prefix = "re2-698890e31fade80eac8d4e7f160b74b3621211ee",
        urls = [
            "https://github.com/google/re2/archive/698890e31fade80eac8d4e7f160b74b3621211ee.tar.gz",  # abseil(2022-07-20)
        ],
        sha256 = "41758cb9ff49a51862d3c2e3ef45962eeda31bfbb54b4f834e811856f238c5fc",
    )

cmake_fetch_content_package(name = "com_google_re2")

cmake_add_dep_mapping(target_mapping = {
    "@com_google_re2//:re2": "re2",
})
