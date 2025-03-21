# 概览

## 输入

### 原生插件

| 名称                                                                                           | 提供方   | 简介                                  |
| ---------------------------------------------------------------------------------------------- | -------- | ------------------------------------- |
| `input_file`<br>[文本日志](input/native/input-file.md)                                         | SLS 官方 | 文本采集。                            |
| `input_container_stdio`<br>[容器标准输出](input/native/input-container-stdio.md)               | SLS 官方 | 从容器标准输出/标准错误流中采集日志。 |
| `input_ebpf_file_security`<br>[eBPF 文件安全数据](input/native/input-file-security.md)         | SLS 官方 | eBPF 文件安全数据采集。               |
| `input_ebpf_network_observer`<br>[eBPF 网络可观测数据](input/native/input-network-observer.md) | SLS 官方 | eBPF 网络可观测数据采集。             |
| `input_ebpf_network_security`<br>[eBPF 网络安全数据](input/native/input-network-security.md)   | SLS 官方 | eBPF 网络安全数据采集。               |
| `input_ebpf_process_security`<br>[eBPF 进程安全数据](input/native/input-process-security.md)   | SLS 官方 | eBPF 进程安全数据采集。               |
| `input_internal_metrics`<br>[自监控指标数据](input/native/input-internal-metrics.md)           | SLS 官方 | 导出自监控指标数据。                  |
| `input_internal_alarms`<br>[自监控告警数据](input/native/input-internal-alarms.md)             | SLS 官方 | 导出自监控告警数据。                  |

### 扩展插件

| 名称                                                                                        | 提供方                                                | 简介                                                                             |
| ------------------------------------------------------------------------------------------- | ----------------------------------------------------- | -------------------------------------------------------------------------------- |
| `input_command`<br>[脚本执行数据](input/extended/input-command.md)                          | 社区<br>[didachuxing](https://github.com/didachuxing) | 采集脚本执行数据。                                                               |
| `input_docker_stdout`<br>[容器标准输出](input/extended/service-docker-stdout.md)            | SLS 官方                                              | 从容器标准输出/标准错误流中采集日志。                                            |
| `metric_debug_file`<br>[文本日志（debug）](input/extended/metric-debug-file.md)             | SLS 官方                                              | 用于调试的读取文件内容的插件。                                                   |
| `metric_input_example`<br>[MetricInput 示例插件](input/extended/metric-input-example.md)    | SLS 官方                                              | MetricInput 示例插件。                                                           |
| `metric_meta_host`<br>[主机 Meta 数据](input/extended/metric-meta-host.md)                  | SLS 官方                                              | 主机 Meta 数据。                                                                 |
| `metric_mock`<br>[Mock 数据-Metric](input/extended/metric-mock.md)                          | SLS 官方                                              | 生成 metric 模拟数据的插件。                                                     |
| `metric_system_v2`<br>[主机监控数据](input/extended/metric-system.md)                       | SLS 官方                                              | 主机监控数据。                                                                   |
| `service_canal`<br>[MySQL Binlog](input/extended/service-canal.md)                          | SLS 官方                                              | 将 MySQL Binlog 输入到 iLogtail。                                                |
| `service_go_profile`<br>[GO Profile](input/extended/service-goprofile.md)                   | SLS 官方                                              | 采集 Golang pprof 性能数据。                                                     |
| `service_gpu_metric`<br>[GPU 数据](input/extended/service-gpu.md)                           | SLS 官方                                              | 支持收集英伟达 GPU 指标。                                                        |
| `service_http_server`<br>[HTTP 数据](input/extended/service-http-server.md)                 | SLS 官方                                              | 接收来自 unix socket、http/https、tcp 的请求，并支持 sls 协议、otlp 等多种协议。 |
| `service_input_example`<br>[ServiceInput 示例插件](input/extended/service-input-example.md) | SLS 官方                                              | ServiceInput 示例插件。                                                          |
| `service_journal`<br>[Journal 数据](input/extended/service-journal.md)                      | SLS 官方                                              | 从原始的二进制文件中采集 Linux 系统的 Journal（systemd）日志。                   |
| `service_kafka`<br>[Kafka](input/extended/service-kafka.md)                                 | SLS 官方                                              | 将 Kafka 数据输入到 iLogtail。                                                   |
| `service_mock`<br>[Mock 数据-Service](input/extended/service-mock.md)                       | SLS 官方                                              | 生成 service 模拟数据的插件。                                                    |
| `service_mssql`<br>[SqlServer 查询数据](input/extended/service-mssql.md)                    | SLS 官方                                              | 将 Sql Server 数据输入到 iLogtail。                                              |
| `service_otlp`<br>[OTLP 数据](input/extended/service-otlp.md)                               | 社区<br>[Zhu Shunjia](https://github.com/shunjiazhu)  | 通过 http/grpc 协议，接收 OTLP 数据。                                            |
| `service_pgsql`<br>[PostgreSQL 查询数据](input/extended/service-pgsql.md)                   | SLS 官方                                              | 将 PostgresSQL 数据输入到 iLogtail。                                             |
| `service_snmp`<br>[收集 SNMP 协议机器信息](input/extended/service-snmp.md)                  | SLS 官方                                              | 收集 SNMP 协议机器信息.                                                          |
| `service_syslog`<br>[Syslog 数据](input/extended/service-syslog.md)                         | SLS 官方                                              | 采集 syslog 数据。                                                               |

## 处理

### SPL 处理

| 名称                                                                 | 提供方   | 简介                  |
| -------------------------------------------------------------------- | -------- | --------------------- |
| `processor_spl`<br>[SPL 处理](processor/spl/processor-spl-native.md) | SLS 官方 | 通过 SPL 语言解析数据 |

### 原生插件

| 名称                                                                                                                 | 提供方   | 简介                                                             |
| -------------------------------------------------------------------------------------------------------------------- | -------- | ---------------------------------------------------------------- |
| `processor_parse_regex_native`<br>[正则解析原生处理插件](processor/native/processor-parse-regex-native.md)           | SLS 官方 | 通过正则匹配解析事件指定字段内容并提取新字段。                   |
| `processor_parse_json_native`<br>[Json 解析原生处理插件](processor/native/processor-parse-json-native.md)            | SLS 官方 | 解析事件中 Json 格式字段内容并提取新字段。                       |
| `processor_parse_delimiter_native`<br>[分隔符解析原生处理插件](processor/native/processor-parse-delimiter-native.md) | SLS 官方 | 解析事件中分隔符格式字段内容并提取新字段。                       |
| `processor_parse_timestamp_native`<br>[时间解析原生处理插件](processor/native/processor-parse-timestamp-native.md)   | SLS 官方 | 解析事件中记录时间的字段，并将结果置为事件的 \_\_time\_\_ 字段。 |
| `processor_filter_regex_native`<br>[过滤原生处理插件](processor/native/processor-filter-regex-native.md)             | SLS 官方 | 根据事件字段内容来过滤事件。                                     |
| `processor_desensitize_native`<br>[脱敏原生处理插件](processor/native/processor-desensitize-native.md)               | SLS 官方 | 对事件指定字段内容进行脱敏。                                     |

### 扩展插件

| 名称                                                                                                        | 提供方                                                  | 简介                                                                                                          |
| ----------------------------------------------------------------------------------------------------------- | ------------------------------------------------------- | ------------------------------------------------------------------------------------------------------------- |
| `processor_add_fields`<br>[添加字段](processor/extended/processor-add-fields.md)                            | SLS 官方                                                | 添加字段。                                                                                                    |
| `processor_appender`<br>[追加字段](processor/extended/processor-appender.md)                                | SLS 官方                                                | 追加字段。                                                                                                    |
| `processor_cloud_meta`<br>[添加云资产信息](processor/extended/processor-cloudmeta.md)                       | SLS 官方                                                | 为日志增加云平台元数据信息。                                                                                  |
| `processor_default`<br>[原始数据](processor/extended/processor-default.md)                                  | SLS 官方                                                | 不对数据任何操作，只是简单的数据透传。                                                                        |
| `processor_desensitize`<br>[数据脱敏](processor/extended/processor-desensitize.md)                          | SLS 官方<br>[Takuka0311](https://github.com/Takuka0311) | 对敏感数据进行脱敏处理。                                                                                      |
| `processor_dict_map`<br>[字段值映射处理](processor/extended/processor-dict-map.md)                          | SLS 官方                                                | 对指定字段的值查表映射.                                                                                       |
| `processor_drop`<br>[丢弃字段](processor/extended/processor-drop.md)                                        | SLS 官方                                                | 丢弃字段。                                                                                                    |
| `processor_encrypt`<br>[字段加密](processor/extended/processor-encrypy.md)                                  | SLS 官方                                                | 加密字段                                                                                                      |
| `processor_fields_with_conditions`<br>[条件字段处理](processor/extended/processor-fields-with-condition.md) | 社区<br>[pj1987111](https://github.com/pj1987111)       | 根据日志部分字段的取值，动态进行字段扩展或删除。                                                              |
| `processor_filter_regex`<br>[日志过滤](processor/extended/processor-filter-regex.md)                        | SLS 官方                                                | 通过正则匹配过滤日志。                                                                                        |
| `processor_gotime`<br>[时间提取（Go 时间格式）](processor/extended/processor-gotime.md)                     | SLS 官方                                                | 以 Go 语言时间格式解析原始日志中的时间字段。                                                                  |
| `processor_grok`<br>[Grok](processor/extended/processor-grok.md)                                            | SLS 官方<br>[Takuka0311](https://github.com/Takuka0311) | 通过 Grok 语法对数据进行处理                                                                                  |
| `processor_json`<br>[Json](processor/extended/processor-json.md)                                            | SLS 官方                                                | 实现对 Json 格式日志的解析。                                                                                  |
| `processor_log_to_sls_metric`<br>[日志转 sls metric](processor/extended/processor-log-to-sls-metric.md)     | SLS 官方                                                | 将日志转 sls metric                                                                                           |
| `processor_packjson`<br>[字段打包](processor/extended/processor-packjson.md)                                | SLS 官方                                                | 可添加指定的字段（支持多个）以 JSON 格式打包成单个字段。                                                      |
| `processor_rate_limit`<br>[日志限速](processor/extended/processor-rate-limit.md)                            | SLS 官方                                                | 用于对日志进行限速处理，确保在设定的时间窗口内，具有相同索引值的日志条目的数量不超过预定的速率限制。          |
| `processor_regex`<br>[正则](processor/extended/processor-regex.md)                                          | SLS 官方                                                | 通过正则匹配的模式实现文本日志的字段提取。                                                                    |
| `processor_rename`<br>[重命名字段](processor/extended/processor-rename.md)                                  | SLS 官方                                                | 重命名字段。                                                                                                  |
| `processor_split_char`<br>[分隔符](processor/extended/processor-delimiter.md)                               | SLS 官方                                                | 通过单字符的分隔符提取字段。                                                                                  |
| `processor_split_string`<br>[分隔符](processor/extended/processor-delimiter.md)                             | SLS 官方                                                | 通过多字符的分隔符提取字段。                                                                                  |
| `processor_split_key_value`<br>[键值对](processor/extended/processor-split-key-value.md)                    | SLS 官方                                                | 通过切分键值对的方式提取字段。                                                                                |
| `processor_split_log_regex`<br>[多行切分](processor/extended/processor-split-log-regex.md)                  | SLS 官方                                                | 实现多行日志（例如 Java 程序日志）的采集。                                                                    |
| `processor_string_replace`<br>[字符串替换](processor/extended/processor-string-replace.md)                  | SLS 官方<br>[pj1987111](https://github.com/pj1987111)   | 通过全文匹配、正则匹配、去转义字符等方式对文本日志进行内容替换。                                              |
| `processor_strptime`<br>[时间提取（strptime 格式）](processor/extended/processor-string-replace.md)         | SLS 官方                                                | 从指定字段中提取日志时间，时间格式为 [Linux strptime](http://man7.org/linux/man-pages/man3/strptime.3.html)。 |

## 聚合

| 名称                                                                                            | 提供方                                              | 简介                                                    |
| ----------------------------------------------------------------------------------------------- | --------------------------------------------------- | ------------------------------------------------------- |
| `aggregator_base`<br>[基础聚合](aggregator/aggregator-base.md)                                  | SLS 官方                                            | 对单条日志进行聚合                                      |
| `aggregator_context`<br>[上下文聚合](aggregator/aggregator-context.md)                          | SLS 官方                                            | 根据日志来源对单条日志进行聚合                          |
| `aggregator_content_value_group`<br>[按 Key 聚合](aggregator/aggregator-content-value-group.md) | 社区<br>[snakorse](https://github.com/snakorse)     | 按照指定的 Key 对采集到的数据进行分组聚合               |
| `aggregator_metadata_group`<br>[GroupMetadata 聚合](aggregator/aggregator-metadata-group.md)    | 社区<br>[urnotsally](https://github.com/urnotsally) | 按照指定的 Metadata Keys 对采集到的数据进行重新分组聚合 |

## 输出

### 原生插件

| 名称                                                                            | 提供方   | 简介                                                 |
| ------------------------------------------------------------------------------- | -------- | ---------------------------------------------------- |
| `flusher_sls`<br>[SLS](flusher/native/flusher-sls.md)                           | SLS 官方 | 将采集到的数据输出到 SLS。                           |
| `flusher_file`<br>[本地文件](flusher/native/flusher-file.md)                    | SLS 官方 | 将采集到的数据写到本地文件。                         |
| `flusher_blackhole`<br>[原生 Flusher 测试](flusher/native/flusher-blackhole.md) | SLS 官方 | 直接丢弃采集的事件，属于原生输出插件，主要用于测试。 |

### 扩展插件

| 名称                                                                                  | 提供方                                              | 简介                                                                                 |
| ------------------------------------------------------------------------------------- | --------------------------------------------------- | ------------------------------------------------------------------------------------ |
| `flusher_kafka`<br>[Kafka](flusher/extended/flusher-kafka.md)                         | 社区                                                | 将采集到的数据输出到 Kafka。推荐使用下面的 flusher_kafka_v2                          |
| `flusher_kafka_v2`<br>[Kafka V2](flusher/extended/flusher-kafka-v2.md)                | 社区<br>[shalousun](https://github.com/shalousun)   | 将采集到的数据输出到 Kafka。                                                         |
| `flusher_stdout`<br>[标准输出/文件](flusher/extended/flusher-stdout.md)               | SLS 官方                                            | 将采集到的数据输出到标准输出或文件。                                                 |
| `flusher_otlp_log`<br>[OTLP 日志](flusher/extended/flusher-otlp.md)                   | 社区<br>[liuhaoyang](https://github.com/liuhaoyang) | 将采集到的数据支持`Opentelemetry log protocol`的后端。                               |
| `flusher_http`<br>[HTTP](flusher/extended/flusher-http.md)                            | 社区<br>[snakorse](https://github.com/snakorse)     | 将采集到的数据以 http 方式输出到指定的后端。                                         |
| `flusher_pulsar`<br>[Pulsar](flusher/extended/flusher-pulsar.md)                      | 社区<br>[shalousun](https://github.com/shalousun)   | 将采集到的数据输出到 Pulsar。                                                        |
| `flusher_clickhouse`<br>[ClickHouse](flusher/extended/flusher-clickhouse.md)          | 社区<br>[kl7sn](https://github.com/kl7sn)           | 将采集到的数据输出到 ClickHouse。                                                    |
| `flusher_elasticsearch`<br>[ElasticSearch](flusher/extended/flusher-elasticsearch.md) | 社区<br>[joeCarf](https://github.com/joeCarf)       | 将采集到的数据输出到 ElasticSearch。                                                 |
| `flusher_loki`<br>[Loki](flusher/extended/flusher-loki.md)                            | 社区<br>[abingcbc](https://github.com/abingcbc)     | 将采集到的数据输出到 Loki。                                                          |
| `flusher_prometheus`<br>[Prometheus](flusher/extended/flusher-prometheus.md)          | 社区<br>                                            | 将采集到的数据，经过处理后，通过 http 格式发送到指定的 Prometheus RemoteWrite 地址。 |

## 扩展

- ClientAuthenticator

  | 名称                                                        | 提供方                                          | 简介                                    |
  | ----------------------------------------------------------- | ----------------------------------------------- | --------------------------------------- |
  | `ext_basicauth`<br>[Basic 认证](extension/ext-basicauth.md) | 社区<br>[snakorse](https://github.com/snakorse) | 为 http_flusher 插件提供 basic 认证能力 |

- FlushInterceptor

  | 名称                                                                          | 提供方                                          | 简介                                                          |
  | ----------------------------------------------------------------------------- | ----------------------------------------------- | ------------------------------------------------------------- |
  | `ext_groupinfo_filter`<br>[GroupInfo 过滤](extension/ext-groupinfo-filter.md) | 社区<br>[snakorse](https://github.com/snakorse) | 为 http_flusher 插件提供根据 GroupInfo 筛选最终提交数据的能力 |

- RequestInterceptor

  | 名称                                                                    | 提供方                                          | 简介                                 |
  | ----------------------------------------------------------------------- | ----------------------------------------------- | ------------------------------------ |
  | `ext_request_breaker`<br>[请求熔断器](extension/ext-request-breaker.md) | 社区<br>[snakorse](https://github.com/snakorse) | 为 http_flusher 插件提供请求熔断能力 |

- Decoder

  | 名称                                                                             | 提供方                                          | 简介                                          |
  | -------------------------------------------------------------------------------- | ----------------------------------------------- | --------------------------------------------- |
  | `ext_default_decoder`<br>[默认的 decoder 扩展](extension/ext-default-decoder.md) | 社区<br>[snakorse](https://github.com/snakorse) | 将内置支持的 Format 以 Decoder 扩展的形式封装 |

- Encoder

  | 名称                                                                             | 提供方                                                 | 简介                                          |
  | -------------------------------------------------------------------------------- | ------------------------------------------------------ | --------------------------------------------- |
  | `ext_default_encoder`<br>[默认的 encoder 扩展](extension/ext-default-encoder.md) | 社区<br>[yuanshuai.1900](https://github.com/aiops1900) | 将内置支持的 Format 以 Encoder 扩展的形式封装 |
