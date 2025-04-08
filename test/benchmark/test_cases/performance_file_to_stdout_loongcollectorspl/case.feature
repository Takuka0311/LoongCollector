@input
Feature: performance file to stdout LoongCollector-SPL
  Performance file to stdout LoongCollector-SPL

  @e2e-performance @docker-compose @LoongCollectorSPL-stdout
  Scenario: PerformanceFileToStdoutLoongCollectorSPL
    Given {docker-compose} environment
    Given docker-compose boot type {benchmark}
    When start docker-compose {performance_file_to_stdout_loongcollectorspl}
    When start monitor {LoongCollectorSPL}, with timeout {6} min
    When generate random nginx logs to file, speed {5}MB/s, total {5}min, to file {./test_cases/performance_file_to_stdout_loongcollectorspl/a.log}
    When wait monitor until log processing finished
