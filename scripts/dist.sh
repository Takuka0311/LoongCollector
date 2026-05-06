#!/usr/bin/env bash

# Copyright 2021 iLogtail Authors
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

set -ue
set -o pipefail

function arch() {
  if uname -m | grep x86_64 &>/dev/null; then
    echo amd64
  elif uname -m | grep -E "aarch64|arm64" &>/dev/null; then
    echo arm64
  else
    echo sw64
  fi
}

# initialize variables
OUT_DIR=${1:-output}
DIST_DIR=${2:-dist}
PACKAGE_DIR=${3:-loongcollector-0.0.1}
ROOTDIR=$(cd $(dirname "${BASH_SOURCE[0]}") && cd .. && pwd)
ARCH=$(arch)

# Determine file names based on ENABLE_CORP_FEATURE
if [ "${ENABLE_CORP_FEATURE:-}" = "ON" ] || [ "${ENABLE_CORP_FEATURE:-}" = "1" ] || [ "${ENABLE_CORP_FEATURE:-}" = "true" ]; then
  PLUGIN_BASE_SO="libPluginBase.so"
  PLUGIN_ADAPTER_SO="libPluginAdapter.so"
  BINARY_NAME="ilogtail"
else
  PLUGIN_BASE_SO="libGoPluginBase.so"
  PLUGIN_ADAPTER_SO="libGoPluginAdapter.so"
  BINARY_NAME="loongcollector"
fi

# prepare dist dir
mkdir -p "${ROOTDIR}/${DIST_DIR}/${PACKAGE_DIR}"
cp LICENSE README.md "${ROOTDIR}/${DIST_DIR}/${PACKAGE_DIR}"
cp "${ROOTDIR}/${OUT_DIR}/${BINARY_NAME}" "${ROOTDIR}/${DIST_DIR}/${PACKAGE_DIR}"
cp "${ROOTDIR}/${OUT_DIR}/${PLUGIN_ADAPTER_SO}" "${ROOTDIR}/${DIST_DIR}/${PACKAGE_DIR}"
cp "${ROOTDIR}/${OUT_DIR}/${PLUGIN_BASE_SO}" "${ROOTDIR}/${DIST_DIR}/${PACKAGE_DIR}"
cp "${ROOTDIR}/${OUT_DIR}/libeBPFDriver.so" "${ROOTDIR}/${DIST_DIR}/${PACKAGE_DIR}"
mkdir -p "${ROOTDIR}/${DIST_DIR}/${PACKAGE_DIR}/conf/instance_config/local/"
mkdir -p "${ROOTDIR}/${DIST_DIR}/${PACKAGE_DIR}/conf/continuous_pipeline_config/local/"
cp "${ROOTDIR}/${OUT_DIR}/conf/instance_config/local/loongcollector_config.json" "${ROOTDIR}/${DIST_DIR}/${PACKAGE_DIR}/conf/instance_config/local/"
cp -a "${ROOTDIR}/${OUT_DIR}/conf/continuous_pipeline_config/local" "${ROOTDIR}/${DIST_DIR}/${PACKAGE_DIR}/conf/continuous_pipeline_config"
if file "${ROOTDIR}/${DIST_DIR}/${PACKAGE_DIR}/${BINARY_NAME}" | grep x86-64; then ./scripts/download_ebpflib.sh "${ROOTDIR}/${DIST_DIR}/${PACKAGE_DIR}"; fi

# Splitting debug info at build time with -gsplit-dwarf does not work with current gcc version
# Strip binary to reduce size here
strip "${ROOTDIR}/${DIST_DIR}/${PACKAGE_DIR}/${BINARY_NAME}"
strip "${ROOTDIR}/${DIST_DIR}/${PACKAGE_DIR}/${PLUGIN_ADAPTER_SO}"
strip "${ROOTDIR}/${DIST_DIR}/${PACKAGE_DIR}/${PLUGIN_BASE_SO}"
strip "${ROOTDIR}/${DIST_DIR}/${PACKAGE_DIR}/libeBPFDriver.so"

# pack dist dir
cd "${ROOTDIR}/${DIST_DIR}"
tar -cvzf "${PACKAGE_DIR}.linux-${ARCH}.tar.gz" "${PACKAGE_DIR}"
rm -rf "${PACKAGE_DIR}"
cd "${ROOTDIR}"
