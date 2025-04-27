@input
Feature: performance file to file LoongCollector 3.0.5
  Performance file to file LoongCollector 3.0.5

  @e2e-performance @docker-compose @loongcollector-3.0.5
  Scenario: PerformanceFileToFileLoongCollector3.0.5
    Given {docker-compose} environment
    Given docker-compose boot type {benchmark}
    When start docker-compose {performance_file_to_file_loongcollector3.0.5}
    When start monitor {LoongCollector}, with timeout {6} min
    When generate random nginx logs to file, speed {10}MB/s, total {5}min, to file {./test_cases/performance_file_to_file_loongcollector3.0.5/a.log}
    When wait monitor until log processing finished
