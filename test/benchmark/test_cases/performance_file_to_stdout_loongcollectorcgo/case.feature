@input
Feature: performance file to stdout LoongCollector-CGo
  Performance file to stdout LoongCollector-CGo

  @e2e-performance @docker-compose @LoongCollectorCGo-stdout
  Scenario: PerformanceFileToStdoutLoongCollectorCGo
    Given {docker-compose} environment
    Given docker-compose boot type {benchmark}
    When start docker-compose {performance_file_to_stdout_loongcollectorcgo}
    When start monitor {LoongCollectorCGo}, with timeout {6} min
    When generate random nginx logs to file, speed {10}MB/s, total {5}min, to file {./test_cases/performance_file_to_stdout_loongcollectorcgo/a.log}
    When wait monitor until log processing finished
