@input
Feature: performance file to stdout filebeat
  Performance file to stdout filebeat

  @e2e-performance @docker-compose @filebeat-stdout
  Scenario: PerformanceFileToStdoutFilebeat
    Given {docker-compose} environment
    Given docker-compose boot type {benchmark}
    When start docker-compose {performance_file_to_stdout_filebeat}
    When start monitor {filebeat}, with timeout {6} min
    When generate random nginx logs to file, speed {5}MB/s, total {5}min, to file {./test_cases/performance_file_to_stdout_filebeat/a.log}
    When wait monitor until log processing finished
