@input
Feature: performance file to blackhole LoongCollector-SPL
  Performance file to blackhole LoongCollector-SPL

  @e2e-performance @docker-compose @LoongCollectorSPL
  Scenario: PerformanceFileToBlackholeLoongCollectorSPL
    Given {docker-compose} environment
    Given docker-compose boot type {benchmark}
    When start docker-compose {performance_file_to_blackhole_loongcollectorspl}
    When start monitor {LoongCollectorSPL}
    When generate random nginx logs to file, speed {10}MB/s, total {10}min, to file {./test_cases/performance_file_to_blackhole_loongcollectorspl/a.log}
    When wait monitor until log processing finished
