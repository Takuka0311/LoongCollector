@input
Feature: performance file to stdout LoongCollector
  Performance file to stdout LoongCollector

  @e2e-performance @docker-compose @LoongCollector-stdout
  Scenario: PerformanceFileToStdoutLoongCollector
    Given {docker-compose} environment
    Given docker-compose boot type {benchmark}
    When start docker-compose {performance_file_to_stdout_loongcollector}
    When start monitor {LoongCollector}, with timeout {6} min
    When generate random nginx logs to file, speed {10}MB/s, total {5}min, to file {./test_cases/performance_file_to_stdout_loongcollector/a.log}
    When wait monitor until log processing finished
