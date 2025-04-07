@input
Feature: performance file to blackhole LoongCollector-CGo
  Performance file to blackhole LoongCollector-CGo

  @e2e-performance @docker-compose @LoongCollectorCGo
  Scenario: PerformanceFileToBlackholeLoongCollectorCGo
    Given {docker-compose} environment
    Given docker-compose boot type {benchmark}
    When start docker-compose {performance_file_to_blackhole_loongcollectorcgo}
    When start monitor {LoongCollectorCGo}, with timeout {6} min
    When generate random nginx logs to file, speed {15}MB/s, total {5}min, to file {./test_cases/performance_file_to_blackhole_loongcollectorcgo/a.log}
    When wait monitor until log processing finished
