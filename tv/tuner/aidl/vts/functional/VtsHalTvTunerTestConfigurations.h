/*
 * Copyright 2021 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <binder/MemoryDealer.h>

#include "../../../config/TunerTestingConfigAidlReaderV1_0.h"

#include <aidl/android/hardware/tv/tuner/DataFormat.h>
#include <aidl/android/hardware/tv/tuner/DemuxAlpFilterType.h>
#include <aidl/android/hardware/tv/tuner/DemuxFilterMainType.h>
#include <aidl/android/hardware/tv/tuner/DemuxFilterMonitorEventType.h>
#include <aidl/android/hardware/tv/tuner/DemuxFilterSettings.h>
#include <aidl/android/hardware/tv/tuner/DemuxFilterType.h>
#include <aidl/android/hardware/tv/tuner/DemuxIpAddress.h>
#include <aidl/android/hardware/tv/tuner/DemuxIpFilterSettings.h>
#include <aidl/android/hardware/tv/tuner/DemuxIpFilterType.h>
#include <aidl/android/hardware/tv/tuner/DemuxMmtpFilterType.h>
#include <aidl/android/hardware/tv/tuner/DemuxRecordScIndexType.h>
#include <aidl/android/hardware/tv/tuner/DemuxTsFilterType.h>
#include <aidl/android/hardware/tv/tuner/DvrSettings.h>
#include <aidl/android/hardware/tv/tuner/DvrType.h>
#include <aidl/android/hardware/tv/tuner/FrontendDvbtBandwidth.h>
#include <aidl/android/hardware/tv/tuner/FrontendDvbtCoderate.h>
#include <aidl/android/hardware/tv/tuner/FrontendDvbtConstellation.h>
#include <aidl/android/hardware/tv/tuner/FrontendDvbtGuardInterval.h>
#include <aidl/android/hardware/tv/tuner/FrontendDvbtHierarchy.h>
#include <aidl/android/hardware/tv/tuner/FrontendDvbtSettings.h>
#include <aidl/android/hardware/tv/tuner/FrontendDvbtStandard.h>
#include <aidl/android/hardware/tv/tuner/FrontendDvbtTransmissionMode.h>
#include <aidl/android/hardware/tv/tuner/FrontendSettings.h>
#include <aidl/android/hardware/tv/tuner/FrontendType.h>
#include <aidl/android/hardware/tv/tuner/PlaybackSettings.h>
#include <aidl/android/hardware/tv/tuner/RecordSettings.h>

using namespace std;
using namespace aidl::android::hardware::tv::tuner;
using namespace android::media::tuner::testing::configuration::V1_0;

const int32_t FMQ_SIZE_4M = 0x400000;
const int32_t FMQ_SIZE_16M = 0x1000000;

const string configFilePath = "/vendor/etc/tuner_vts_config_aidl_V1.xml";

#define FILTER_MAIN_TYPE_BIT_COUNT 5

// Hardware configs
static map<string, FrontendConfig> frontendMap;
static map<string, FilterConfig> filterMap;
static map<string, DvrConfig> dvrMap;
static map<string, LnbConfig> lnbMap;
static map<string, TimeFilterConfig> timeFilterMap;
static map<string, vector<uint8_t>> diseqcMsgMap;
static map<string, DescramblerConfig> descramblerMap;

// Hardware and test cases connections
static LiveBroadcastHardwareConnections live;
static ScanHardwareConnections scan;
static DvrPlaybackHardwareConnections playback;
static DvrRecordHardwareConnections record;
static DescramblingHardwareConnections descrambling;
static LnbLiveHardwareConnections lnbLive;
static LnbRecordHardwareConnections lnbRecord;
static TimeFilterHardwareConnections timeFilter;

/** Config all the frontends that would be used in the tests */
inline void initFrontendConfig() {
    // The test will use the internal default fe when default fe is connected to any data flow
    // without overriding in the xml config.
    string defaultFeId = "FE_DEFAULT";
    FrontendDvbtSettings dvbtSettings{
            .frequency = 578000000,
            .transmissionMode = FrontendDvbtTransmissionMode::AUTO,
            .bandwidth = FrontendDvbtBandwidth::BANDWIDTH_8MHZ,
            .isHighPriority = true,
    };
    frontendMap[defaultFeId].type = FrontendType::DVBT;
    frontendMap[defaultFeId].settings.set<FrontendSettings::Tag::dvbt>(dvbtSettings);

    vector<FrontendStatusType> types;
    types.push_back(FrontendStatusType::UEC);
    types.push_back(FrontendStatusType::IS_MISO);

    vector<FrontendStatus> statuses;
    FrontendStatus status;
    status.set<FrontendStatus::Tag::uec>(4);
    statuses.push_back(status);
    status.set<FrontendStatus::Tag::isMiso>(true);
    statuses.push_back(status);

    frontendMap[defaultFeId].tuneStatusTypes = types;
    frontendMap[defaultFeId].expectTuneStatuses = statuses;
    frontendMap[defaultFeId].isSoftwareFe = true;
    frontendMap[defaultFeId].canConnectToCiCam = true;
    frontendMap[defaultFeId].ciCamId = 0;
    FrontendDvbtSettings dvbt;
    dvbt.transmissionMode = FrontendDvbtTransmissionMode::MODE_8K_E;
    frontendMap[defaultFeId].settings.set<FrontendSettings::Tag::dvbt>(dvbt);
    // Read customized config
    TunerTestingConfigAidlReader1_0::readFrontendConfig1_0(frontendMap);
};

inline void initFilterConfig() {
    // The test will use the internal default filter when default filter is connected to any
    // data flow without overriding in the xml config.
    string defaultAudioFilterId = "FILTER_AUDIO_DEFAULT";
    string defaultVideoFilterId = "FILTER_VIDEO_DEFAULT";

    filterMap[defaultVideoFilterId].type.mainType = DemuxFilterMainType::TS;
    filterMap[defaultVideoFilterId].type.subType.set<DemuxFilterSubType::Tag::tsFilterType>(
            DemuxTsFilterType::VIDEO);
    filterMap[defaultVideoFilterId].bufferSize = FMQ_SIZE_16M;
    filterMap[defaultVideoFilterId].settings =
            DemuxFilterSettings::make<DemuxFilterSettings::Tag::ts>();
    filterMap[defaultVideoFilterId].settings.get<DemuxFilterSettings::Tag::ts>().tpid = 256;
    DemuxFilterAvSettings video;
    video.isPassthrough = false;
    filterMap[defaultVideoFilterId]
            .settings.get<DemuxFilterSettings::Tag::ts>()
            .filterSettings.set<DemuxTsFilterSettingsFilterSettings::Tag::av>(video);
    filterMap[defaultVideoFilterId].monitorEventTypes =
            static_cast<int32_t>(DemuxFilterMonitorEventType::SCRAMBLING_STATUS) |
            static_cast<int32_t>(DemuxFilterMonitorEventType::IP_CID_CHANGE);
    filterMap[defaultVideoFilterId].streamType.set<AvStreamType::Tag::video>(
            VideoStreamType::MPEG1);

    filterMap[defaultAudioFilterId].type.mainType = DemuxFilterMainType::TS;
    filterMap[defaultAudioFilterId].type.subType.set<DemuxFilterSubType::Tag::tsFilterType>(
            DemuxTsFilterType::AUDIO);
    filterMap[defaultAudioFilterId].bufferSize = FMQ_SIZE_16M;
    filterMap[defaultAudioFilterId].settings =
            DemuxFilterSettings::make<DemuxFilterSettings::Tag::ts>();
    filterMap[defaultAudioFilterId].settings.get<DemuxFilterSettings::Tag::ts>().tpid = 256;
    DemuxFilterAvSettings audio;
    audio.isPassthrough = false;
    filterMap[defaultAudioFilterId]
            .settings.get<DemuxFilterSettings::Tag::ts>()
            .filterSettings.set<DemuxTsFilterSettingsFilterSettings::Tag::av>(audio);
    filterMap[defaultAudioFilterId].monitorEventTypes =
            static_cast<int32_t>(DemuxFilterMonitorEventType::SCRAMBLING_STATUS) |
            static_cast<int32_t>(DemuxFilterMonitorEventType::IP_CID_CHANGE);
    filterMap[defaultAudioFilterId].streamType.set<AvStreamType::Tag::audio>(AudioStreamType::MP3);
    // Read customized config
    TunerTestingConfigAidlReader1_0::readFilterConfig1_0(filterMap);
};

/** Config all the dvrs that would be used in the tests */
inline void initDvrConfig() {
    // Read customized config
    TunerTestingConfigAidlReader1_0::readDvrConfig1_0(dvrMap);
};

inline void initTimeFilterConfig() {
    // Read customized config
    TunerTestingConfigAidlReader1_0::readTimeFilterConfig1_0(timeFilterMap);
};

inline void initDescramblerConfig() {
    // Read customized config
    TunerTestingConfigAidlReader1_0::readDescramblerConfig1_0(descramblerMap);
}

inline void initLnbConfig() {
    // Read customized config
    TunerTestingConfigAidlReader1_0::readLnbConfig1_0(lnbMap);
};

inline void initDiseqcMsgsConfig() {
    // Read customized config
    TunerTestingConfigAidlReader1_0::readDiseqcMessages(diseqcMsgMap);
};

inline void determineScan() {
    if (!frontendMap.empty()) {
        scan.hasFrontendConnection = true;
        ALOGD("Can support scan");
    }
}

inline void determineTimeFilter() {
    if (!timeFilterMap.empty()) {
        timeFilter.support = true;
        ALOGD("Can support time filter");
    }
}

inline void determineDvrPlayback() {
    if (!playbackDvrIds.empty() && !audioFilterIds.empty() && !videoFilterIds.empty()) {
        playback.support = true;
        ALOGD("Can support dvr playback");
    }
}

inline void determineLnbLive() {
    if (!audioFilterIds.empty() && !videoFilterIds.empty() && !frontendMap.empty() &&
        !lnbMap.empty()) {
        lnbLive.support = true;
        ALOGD("Can support lnb live");
    }
}

inline void determineLnbRecord() {
    if (!frontendMap.empty() && !recordFilterIds.empty() && !recordDvrIds.empty() &&
        !lnbMap.empty()) {
        lnbRecord.support = true;
        ALOGD("Can support lnb record");
    }
}

inline void determineLive() {
    if (videoFilterIds.empty() || audioFilterIds.empty() || frontendMap.empty()) {
        return;
    }
    if (hasSwFe && !hasHwFe && dvrMap.empty()) {
        ALOGD("Cannot configure Live. Only software frontends and no dvr connections");
        return;
    }
    ALOGD("Can support live");
    live.hasFrontendConnection = true;
}

inline void determineDescrambling() {
    if (descramblerMap.empty() || audioFilterIds.empty() || videoFilterIds.empty()) {
        return;
    }
    if (frontendMap.empty() && playbackDvrIds.empty()) {
        ALOGD("Cannot configure descrambling. No frontends or playback dvr's");
        return;
    }
    if (hasSwFe && !hasHwFe && playbackDvrIds.empty()) {
        ALOGD("cannot configure descrambling. Only SW frontends and no playback dvr's");
        return;
    }
    ALOGD("Can support descrambling");
    descrambling.support = true;
}

inline void determineDvrRecord() {
    if (recordDvrIds.empty() || recordFilterIds.empty()) {
        return;
    }
    if (frontendMap.empty() && playbackDvrIds.empty()) {
        ALOGD("Cannot support dvr record. No frontends and  no playback dvr's");
        return;
    }
    if (hasSwFe && !hasHwFe && playbackDvrIds.empty()) {
        ALOGD("Cannot support dvr record. Only SW frontends and no playback dvr's");
        return;
    }
    ALOGD("Can support dvr record.");
    record.support = true;
}

/** Read the vendor configurations of which hardware to use for each test cases/data flows */
inline void connectHardwaresToTestCases() {
    TunerTestingConfigAidlReader1_0::connectLiveBroadcast(live);
    TunerTestingConfigAidlReader1_0::connectScan(scan);
    TunerTestingConfigAidlReader1_0::connectDvrRecord(record);
    TunerTestingConfigAidlReader1_0::connectTimeFilter(timeFilter);
    TunerTestingConfigAidlReader1_0::connectDescrambling(descrambling);
    TunerTestingConfigAidlReader1_0::connectLnbLive(lnbLive);
    TunerTestingConfigAidlReader1_0::connectLnbRecord(lnbRecord);
    TunerTestingConfigAidlReader1_0::connectDvrPlayback(playback);
};

inline void determineDataFlows() {
    determineScan();
    determineTimeFilter();
    determineDvrPlayback();
    determineLnbLive();
    determineLnbRecord();
    determineLive();
    determineDescrambling();
    determineDvrRecord();
}

inline bool validateConnections() {
    if (record.support && !record.hasFrontendConnection &&
        record.dvrSourceId.compare(emptyHardwareId) == 0) {
        ALOGW("[vts config] Record must support either a DVR source or a Frontend source.");
        return false;
    }
    bool feIsValid = live.hasFrontendConnection
                             ? frontendMap.find(live.frontendId) != frontendMap.end()
                             : true;
    feIsValid &= scan.hasFrontendConnection ? frontendMap.find(scan.frontendId) != frontendMap.end()
                                            : true;
    feIsValid &= record.support && record.hasFrontendConnection
                         ? frontendMap.find(record.frontendId) != frontendMap.end()
                         : true;
    feIsValid &= descrambling.support && descrambling.hasFrontendConnection
                         ? frontendMap.find(descrambling.frontendId) != frontendMap.end()
                         : true;

    feIsValid &= lnbLive.support ? frontendMap.find(lnbLive.frontendId) != frontendMap.end() : true;

    feIsValid &=
            lnbRecord.support ? frontendMap.find(lnbRecord.frontendId) != frontendMap.end() : true;

    if (!feIsValid) {
        ALOGW("[vts config] dynamic config fe connection is invalid.");
        return false;
    }

    bool dvrIsValid = frontendMap[live.frontendId].isSoftwareFe
                              ? dvrMap.find(live.dvrSoftwareFeId) != dvrMap.end()
                              : true;

    if (record.support) {
        if (record.hasFrontendConnection) {
            if (frontendMap[record.frontendId].isSoftwareFe) {
                dvrIsValid &= dvrMap.find(record.dvrSoftwareFeId) != dvrMap.end();
            }
        } else {
            dvrIsValid &= dvrMap.find(record.dvrSourceId) != dvrMap.end();
        }
        dvrIsValid &= dvrMap.find(record.dvrRecordId) != dvrMap.end();
    }

    if (descrambling.support) {
        if (descrambling.hasFrontendConnection) {
            if (frontendMap[descrambling.frontendId].isSoftwareFe) {
                dvrIsValid &= dvrMap.find(descrambling.dvrSoftwareFeId) != dvrMap.end();
            }
        } else {
            dvrIsValid &= dvrMap.find(descrambling.dvrSourceId) != dvrMap.end();
        }
    }

    dvrIsValid &= lnbRecord.support ? dvrMap.find(lnbRecord.dvrRecordId) != dvrMap.end() : true;

    dvrIsValid &= playback.support ? dvrMap.find(playback.dvrId) != dvrMap.end() : true;

    if (!dvrIsValid) {
        ALOGW("[vts config] dynamic config dvr connection is invalid.");
        return false;
    }

    bool filterIsValid = (live.hasFrontendConnection)
                             ? filterMap.find(live.audioFilterId) != filterMap.end() &&
                               filterMap.find(live.videoFilterId) != filterMap.end()
                             : true;
    filterIsValid &=
            record.support ? filterMap.find(record.recordFilterId) != filterMap.end() : true;

    filterIsValid &= descrambling.support
                             ? filterMap.find(descrambling.videoFilterId) != filterMap.end() &&
                                       filterMap.find(descrambling.audioFilterId) != filterMap.end()
                             : true;

    for (auto& filterId : descrambling.extraFilters) {
        filterIsValid &= filterMap.find(filterId) != filterMap.end();
    }

    filterIsValid &= lnbLive.support
                             ? filterMap.find(lnbLive.audioFilterId) != filterMap.end() &&
                                       filterMap.find(lnbLive.videoFilterId) != filterMap.end()
                             : true;

    filterIsValid &=
            lnbRecord.support ? filterMap.find(lnbRecord.recordFilterId) != filterMap.end() : true;

    for (auto& filterId : lnbRecord.extraFilters) {
        filterIsValid &= filterMap.find(filterId) != filterMap.end();
    }

    for (auto& filterId : lnbLive.extraFilters) {
        filterIsValid &= filterMap.find(filterId) != filterMap.end();
    }

    filterIsValid &= playback.support
                             ? filterMap.find(playback.audioFilterId) != filterMap.end() &&
                                       filterMap.find(playback.videoFilterId) != filterMap.end()
                             : true;
    filterIsValid &= playback.sectionFilterId.compare(emptyHardwareId) == 0
                             ? true
                             : filterMap.find(playback.sectionFilterId) != filterMap.end();

    for (auto& filterId : playback.extraFilters) {
        filterIsValid &=
                playback.hasExtraFilters ? filterMap.find(filterId) != filterMap.end() : true;
    }

    if (!filterIsValid) {
        ALOGW("[vts config] dynamic config filter connection is invalid.");
        return false;
    }

    bool timeFilterIsValid =
            timeFilter.support ? timeFilterMap.find(timeFilter.timeFilterId) != timeFilterMap.end()
                               : true;

    if (!timeFilterIsValid) {
        ALOGW("[vts config] dynamic config time filter connection is invalid.");
    }

    bool descramblerIsValid =
            descrambling.support
                    ? descramblerMap.find(descrambling.descramblerId) != descramblerMap.end()
                    : true;

    if (!descramblerIsValid) {
        ALOGW("[vts config] dynamic config descrambler connection is invalid.");
        return false;
    }

    bool lnbIsValid = lnbLive.support ? lnbMap.find(lnbLive.lnbId) != lnbMap.end() : true;

    lnbIsValid &= lnbRecord.support ? lnbMap.find(lnbRecord.lnbId) != lnbMap.end() : true;

    if (!lnbIsValid) {
        ALOGW("[vts config] dynamic config lnb connection is invalid.");
        return false;
    }

    bool diseqcMsgsIsValid = true;

    for (auto& msg : lnbRecord.diseqcMsgs) {
        diseqcMsgsIsValid &= diseqcMsgMap.find(msg) != diseqcMsgMap.end();
    }

    for (auto& msg : lnbLive.diseqcMsgs) {
        diseqcMsgsIsValid &= diseqcMsgMap.find(msg) != diseqcMsgMap.end();
    }

    if (!diseqcMsgsIsValid) {
        ALOGW("[vts config] dynamic config diseqcMsg is invalid.");
        return false;
    }

    return true;
}
