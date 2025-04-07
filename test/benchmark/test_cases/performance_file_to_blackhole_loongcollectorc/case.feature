@input
Feature: performance file to blackhole LoongCollector-C
  Performance file to blackhole LoongCollector-C

  @e2e-performance @docker-compose @LoongCollectorC
  Scenario: PerformanceFileToBlackholeiLoongCollectorC
    Given {docker-compose} environment
    Given docker-compose boot type {benchmark}
    When start docker-compose {performance_file_to_blackhole_loongcollectorc}
    When start monitor {LoongCollectorC}, with timeout {6} min
    When generate random nginx logs to file, speed {10}MB/s, total {5}min, to file {./test_cases/performance_file_to_blackhole_loongcollectorc/a.log}
    When wait monitor until log processing finished
