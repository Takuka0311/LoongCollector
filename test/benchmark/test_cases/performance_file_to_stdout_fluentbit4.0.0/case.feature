@input
Feature: performance file to stdout fluentbit-4.0.0
  Performance file to stdout fluentbit-4.0.0

  @e2e-performance @docker-compose @fluentbit-4.0.0-stdout
  Scenario: PerformanceFileToStdoutFluentbit
    Given {docker-compose} environment
    Given docker-compose boot type {benchmark}
    When start docker-compose {performance_file_to_stdout_fluentbit4.0.0}
    When start monitor {fluent-bit}, with timeout {6} min
    When generate random nginx logs to file, speed {10}MB/s, total {5}min, to file {./test_cases/performance_file_to_stdout_fluentbit4.0.0/a.log}
    When wait monitor until log processing finished
