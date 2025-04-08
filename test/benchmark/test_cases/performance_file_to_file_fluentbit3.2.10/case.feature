@input
Feature: performance file to file fluentbit-3.2.10
  Performance file to file fluentbit-3.2.10

  @e2e-performance @docker-compose @fluentbit-3.2.10-file
  Scenario: PerformanceFileToFileFluentbit
    Given {docker-compose} environment
    Given docker-compose boot type {benchmark}
    When start docker-compose {performance_file_to_file_fluentbit3.2.10}
    When start monitor {fluent-bit}, with timeout {6} min
    When generate random nginx logs to file, speed {10}MB/s, total {5}min, to file {./test_cases/performance_file_to_file_fluentbit3.2.10/a.log}
    When wait monitor until log processing finished
