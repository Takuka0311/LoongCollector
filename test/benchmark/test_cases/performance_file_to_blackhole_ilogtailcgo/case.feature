@input
Feature: performance file to blackhole iLogtail-CGo
  Performance file to blackhole iLogtail-CGo

  @e2e-performance @docker-compose @ilogtailCGo
  Scenario: PerformanceFileToBlackholeiLogtailCGo
    Given {docker-compose} environment
    Given docker-compose boot type {benchmark}
    When start docker-compose {performance_file_to_blackhole_ilogtailcgo}
    When start monitor {ilogtailCGo}
    When generate random nginx logs to file, speed {10}MB/s, total {10}min, to file {./test_cases/performance_file_to_blackhole_ilogtailcgo/a.log}
    When wait monitor until log processing finished
