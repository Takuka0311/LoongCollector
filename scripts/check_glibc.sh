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

# initialize variables
OUT_DIR=${1:-output}
ROOTDIR=$(cd $(dirname "${BASH_SOURCE[0]}") && cd .. && pwd)

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

BIN="${ROOTDIR}/${OUT_DIR}/${BINARY_NAME}"
ADAPTER="${ROOTDIR}/${OUT_DIR}/${PLUGIN_ADAPTER_SO}"
PLUGIN="${ROOTDIR}/${OUT_DIR}/${PLUGIN_BASE_SO}"
EBPF_DRIVER="${ROOTDIR}/${OUT_DIR}/libeBPFDriver.so"

# check if the symbols in loongcollector are compatible with GLIBC_2.5
awk_script=$(cat <<- EOF
BEGIN {
  delete bad_syms[0]
}
NF>0 && \$(NF-1)~"GLIBC_" {
  sec=\$(NF-1);
  split(sec, a, "_");
  split(a[2], a, ".");
  ver=a[1]*1000+a[2];
  if (ver > 2012) {
    bad_syms[length(bad_syms)]=\$0
  }
}
END {
  if (length(bad_syms) == 0) {
    exit 0
  }
  print "\033[0;31mError: The following symbols are not compatible with GLIBC_2.12"
  for (i in bad_syms) {
    print bad_syms[i]
    printf("\033[0m")
  }
  exit 1
}
EOF
)
all=("$BIN" "$ADAPTER" "$PLUGIN" "$EBPF_DRIVER")
failed=0
for obj in "${all[@]}"; do
    echo "Checking symbols in $obj ..."
    objdump -T "$obj" | awk "$awk_script" || failed+=1
done
[[ $failed -gt 0 ]] && exit 1 || {
    echo -e "\033[0;32mAll the checks passed\033[0m"
}
