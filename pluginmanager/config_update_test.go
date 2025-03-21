// Copyright 2021 iLogtail Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

//go:build linux || windows
// +build linux windows

package pluginmanager

import (
	"context"
	"testing"
	"time"

	"github.com/alibaba/ilogtail/pkg/logger"
	_ "github.com/alibaba/ilogtail/plugins/aggregator/baseagg"
	"github.com/alibaba/ilogtail/plugins/flusher/checker"
	"github.com/alibaba/ilogtail/plugins/input/mockd"

	"github.com/stretchr/testify/suite"
)

var updateConfigName = "update_mock_block"
var noblockUpdateConfigName = "update_mock_noblock"
var noblockUpdateNoInputConfigName = "update_mock_noblock_no_input"

func TestConfigUpdate(t *testing.T) {
	suite.Run(t, new(configUpdateTestSuite))
}

type configUpdateTestSuite struct {
	suite.Suite
}

func (s *configUpdateTestSuite) BeforeTest(suiteName, testName string) {
	logger.Infof(context.Background(), "========== %s %s test start ========================", suiteName, testName)
	logger.Info(context.Background(), "load logstore config", updateConfigName)
	s.NoError(LoadAndStartMockConfig(updateConfigName, updateConfigName, updateConfigName, GetTestConfig(updateConfigName)))
	time.Sleep(time.Second * time.Duration(1))
}

func (s *configUpdateTestSuite) AfterTest(suiteName, testName string) {
	logger.Infof(context.Background(), "========== %s %s test end ========================", suiteName, testName)
	s.NoError(StopAllPipelines(false))
	s.NoError(StopAllPipelines(true))
	LogtailConfigLock.Lock()
	LogtailConfig = make(map[string]*LogstoreConfig)
	LogtailConfigLock.Unlock()
}

func (s *configUpdateTestSuite) TestConfigUpdate() {
	// block config -> block config, unblock config, no input config
	// block config
	LogtailConfigLock.RLock()
	config := LogtailConfig[updateConfigName]
	LogtailConfigLock.RUnlock()
	s.NotNil(config, "%s logstrore config should exist", updateConfigName)
	checkFlusher, ok := GetConfigFlushers(config.PluginRunner)[0].(*checker.FlusherChecker)
	s.True(ok)
	mockInput, ok := GetConfigInputs(config.PluginRunner)[0].(*mockd.ServiceMock)
	s.True(ok)
	s.Equal(0, checkFlusher.GetLogCount(), "the block flusher checker doesn't have any logs")

	// update same hang config
	s.NoError(Stop(updateConfigName, false))
	s.Equal(0, checkFlusher.GetLogCount(), "the hold on block flusher checker doesn't have any logs")
	_ = LoadAndStartMockConfig(updateConfigName, updateConfigName, updateConfigName, GetTestConfig(updateConfigName))
	// Since independently load config, reload block config will be allowed
	s.NoError(LoadAndStartMockConfig(noblockUpdateConfigName, noblockUpdateConfigName, noblockUpdateConfigName, GetTestConfig(noblockUpdateConfigName)))
	s.NoError(LoadAndStartMockConfig(noblockUpdateNoInputConfigName, noblockUpdateNoInputConfigName, noblockUpdateNoInputConfigName, GetTestConfig(noblockUpdateNoInputConfigName)))
	LogtailConfigLock.RLock()
	s.NotNil(LogtailConfig[updateConfigName])
	s.NotNil(LogtailConfig[noblockUpdateConfigName])
	s.NotNil(LogtailConfig[noblockUpdateNoInputConfigName])
	LogtailConfigLock.RUnlock()

	time.Sleep(time.Second * time.Duration(10))
	LogtailConfigLock.RLock()
	s.Equal(20000, GetConfigFlushers(LogtailConfig[noblockUpdateConfigName].PluginRunner)[0].(*checker.FlusherChecker).GetLogCount())
	LogtailConfigLock.RUnlock()

	// unblock old config
	mockInput.Block = false
	s.Equal(0, checkFlusher.GetLogCount())
}

func (s *configUpdateTestSuite) TestConfigUpdateTimeout() {
	// block config -> block config, unblock config, no input config
	// block config
	LogtailConfigLock.RLock()
	config := LogtailConfig[updateConfigName]
	LogtailConfigLock.RUnlock()
	s.NotNil(config, "%s logstrore config should exist", updateConfigName)
	checkFlusher, ok := GetConfigFlushers(config.PluginRunner)[0].(*checker.FlusherChecker)
	s.True(ok)
	mockInput, ok := GetConfigInputs(config.PluginRunner)[0].(*mockd.ServiceMock)
	s.True(ok)
	s.Equal(0, checkFlusher.GetLogCount(), "the block flusher checker doesn't have any logs")

	// update same hang config
	s.NoError(Stop(updateConfigName, false))
	s.Equal(0, checkFlusher.GetLogCount(), "the hold on block flusher checker doesn't have any logs")

	// unblock old config first to mock timeout instead of block
	mockInput.Block = false
	s.Equal(0, checkFlusher.GetLogCount())

	_ = LoadAndStartMockConfig(updateConfigName, updateConfigName, updateConfigName, GetTestConfig(updateConfigName))
	// Since independently load config, reload block config will be allowed
	s.NoError(LoadAndStartMockConfig(noblockUpdateConfigName, noblockUpdateConfigName, noblockUpdateConfigName, GetTestConfig(noblockUpdateConfigName)))
	s.NoError(LoadAndStartMockConfig(noblockUpdateNoInputConfigName, noblockUpdateNoInputConfigName, noblockUpdateNoInputConfigName, GetTestConfig(noblockUpdateNoInputConfigName)))
	LogtailConfigLock.RLock()
	s.NotNil(LogtailConfig[updateConfigName])
	s.NotNil(LogtailConfig[noblockUpdateConfigName])
	s.NotNil(LogtailConfig[noblockUpdateNoInputConfigName])
	LogtailConfigLock.RUnlock()

	time.Sleep(time.Second * time.Duration(10))
	LogtailConfigLock.RLock()
	s.Equal(20000, GetConfigFlushers(LogtailConfig[noblockUpdateConfigName].PluginRunner)[0].(*checker.FlusherChecker).GetLogCount())
	LogtailConfigLock.RUnlock()
}

func (s *configUpdateTestSuite) TestConfigUpdateMany() {
	LogtailConfigLock.RLock()
	config := LogtailConfig[updateConfigName]
	LogtailConfigLock.RUnlock()
	s.NotNil(config, "%s logstrore config should exist", updateConfigName)
	checkFlusher, ok := GetConfigFlushers(config.PluginRunner)[0].(*checker.FlusherChecker)
	s.True(ok)
	mockInput, ok := GetConfigInputs(config.PluginRunner)[0].(*mockd.ServiceMock)
	s.True(ok)

	s.Equal(0, checkFlusher.GetLogCount(), "the hold on block flusher checker doesn't have any logs")
	// load block config
	Stop(updateConfigName, false)
	err := LoadAndStartMockConfig(updateConfigName, updateConfigName, updateConfigName, GetTestConfig(updateConfigName))
	s.Nil(err)
	s.NotNil(LogtailConfig[updateConfigName])
	s.Equal(0, checkFlusher.GetLogCount(), "the hold on block flusher checker doesn't have any logs")
	mockInput.Block = false
	time.Sleep(time.Second * time.Duration(5))
	s.Equal(checkFlusher.GetLogCount(), 0)

	// load normal config
	for i := 0; i < 3; i++ {
		s.NoError(StopAllPipelines(true))
		s.NoError(StopAllPipelines(false))
		s.NoError(LoadAndStartMockConfig(noblockUpdateConfigName, noblockUpdateConfigName, noblockUpdateConfigName, GetTestConfig(noblockUpdateConfigName)))
		LogtailConfigLock.RLock()
		s.NotNil(LogtailConfig[noblockUpdateConfigName])
		LogtailConfigLock.RUnlock()
		time.Sleep(time.Millisecond)
	}
	LogtailConfigLock.RLock()
	checkFlusher, ok = GetConfigFlushers(LogtailConfig[noblockUpdateConfigName].PluginRunner)[0].(*checker.FlusherChecker)
	LogtailConfigLock.RUnlock()
	s.True(ok)
	time.Sleep(time.Second * time.Duration(5))
	s.Equal(checkFlusher.GetLogCount(), 20000)
}

func (s *configUpdateTestSuite) TestConfigUpdateName() {
	time.Sleep(time.Second * time.Duration(1))
	LogtailConfigLock.RLock()
	config := LogtailConfig[updateConfigName]
	LogtailConfigLock.RUnlock()
	s.NotNil(config)
	checkFlusher, ok := GetConfigFlushers(config.PluginRunner)[0].(*checker.FlusherChecker)
	s.True(ok)
	mockInput, ok := GetConfigInputs(config.PluginRunner)[0].(*mockd.ServiceMock)
	s.True(ok)
	defer func() {
		mockInput.Block = false
		time.Sleep(time.Second * 5)
		s.Equal(checkFlusher.GetLogCount(), 20000)
	}()

	s.Equal(0, checkFlusher.GetLogCount(), "the hold on blocking flusher checker doesn't have any logs")
	s.NoError(LoadAndStartMockConfig(updateConfigName+"_", updateConfigName+"_", updateConfigName+"_", GetTestConfig(updateConfigName)))
	{
		LogtailConfigLock.RLock()
		s.NotNil(LogtailConfig[updateConfigName])
		s.NotNil(LogtailConfig[updateConfigName+"_"])
		checkFlusher, ok := GetConfigFlushers(LogtailConfig[updateConfigName+"_"].PluginRunner)[0].(*checker.FlusherChecker)
		s.True(ok)
		mockInput, ok := GetConfigInputs(LogtailConfig[updateConfigName+"_"].PluginRunner)[0].(*mockd.ServiceMock)
		s.True(ok)
		LogtailConfigLock.RUnlock()
		s.Equal(checkFlusher.GetLogCount(), 0)
		mockInput.Block = false
		time.Sleep(time.Second * 5)
		s.Equal(checkFlusher.GetLogCount(), 20000)
	}
}

func (s *configUpdateTestSuite) TestStopAllExit() {
	LogtailConfigLock.RLock()
	config := LogtailConfig[updateConfigName]
	LogtailConfigLock.RUnlock()
	s.NotNil(config)
	checkFlusher, ok := GetConfigFlushers(config.PluginRunner)[0].(*checker.FlusherChecker)
	s.True(ok)
	mockInput, ok := GetConfigInputs(config.PluginRunner)[0].(*mockd.ServiceMock)
	s.True(ok)
	mockInput.Block = false
	time.Sleep(time.Second * time.Duration(5))
	s.NoError(StopAllPipelines(true))
	s.NoError(StopAllPipelines(false))
	s.Equal(20000, checkFlusher.GetLogCount())
}

func (s *configUpdateTestSuite) TestStopAllExitTimeout() {
	time.Sleep(time.Second * time.Duration(1))
	LogtailConfigLock.RLock()
	config := LogtailConfig[updateConfigName]
	LogtailConfigLock.RUnlock()
	s.NotNil(config)
	checkFlusher, ok := GetConfigFlushers(config.PluginRunner)[0].(*checker.FlusherChecker)
	s.True(ok)
	mockInput, ok := GetConfigInputs(config.PluginRunner)[0].(*mockd.ServiceMock)
	s.True(ok)
	s.Equal(0, checkFlusher.GetLogCount())
	s.NoError(StopAllPipelines(true))
	s.NoError(StopAllPipelines(false))
	time.Sleep(time.Second)
	s.Equal(0, checkFlusher.GetLogCount())
	mockInput.Block = false
	time.Sleep(time.Second * time.Duration(5))
	s.Equal(0, checkFlusher.GetLogCount())
}

func (s *configUpdateTestSuite) TestUnreadyFlusher() {
	// block config
	LogtailConfigLock.RLock()
	config := LogtailConfig[updateConfigName]
	LogtailConfigLock.RUnlock()
	s.NotNil(config, "%s logstrore config should exist", updateConfigName)
	checkFlusher, ok := GetConfigFlushers(config.PluginRunner)[0].(*checker.FlusherChecker)
	s.True(ok)
	mockInput, ok := GetConfigInputs(config.PluginRunner)[0].(*mockd.ServiceMock)
	s.True(ok)
	s.Equal(0, checkFlusher.GetLogCount(), "the block flusher checker doesn't have any logs")

	// make flusher unready
	checkFlusher.Ready = false
	// unblock input
	mockInput.Block = false
	time.Sleep(time.Second * time.Duration(3))

	// update config
	s.NoError(Stop(updateConfigName, false))
	s.Equal(0, checkFlusher.GetLogCount(), "the hold on block flusher checker doesn't have any logs")
	_ = LoadAndStartMockConfig(updateConfigName, updateConfigName, updateConfigName, GetTestConfig(updateConfigName))
	LogtailConfigLock.RLock()
	config = LogtailConfig[updateConfigName]
	LogtailConfigLock.RUnlock()
	s.NotNil(config, "%s logstrore config should exist", updateConfigName)
	checkFlusher, ok = GetConfigFlushers(config.PluginRunner)[0].(*checker.FlusherChecker)
	s.True(ok)
	mockInput, ok = GetConfigInputs(config.PluginRunner)[0].(*mockd.ServiceMock)
	s.True(ok)
	mockInput.Block = false
	time.Sleep(time.Second * time.Duration(3))
	s.Equal(20000+10000, checkFlusher.GetLogCount(), "the new flusher checker should merge the old logs")
}
