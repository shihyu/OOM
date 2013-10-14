// Copyright for translation

#define DEBUG_LEVEL 2
#undef LOG_TAG
#define LOG_TAG "BatteryStatsImpl"
#include <battery/hepenergystats/BatteryStatsImpl.h>
#include <gaiainternal/util/PrintWriterPrinter.h>
#include <gaiainternal/Process.h>
#include <gaiainternal/io/BufferedReader.h>
#include <gaiainternal/io/FileReader.h>
#include <lang/LongLong.h>
#include <gaiainternal/bluetooth/BluetoothDevice.h>
#include <gaiainternal/util/GList.h>
#include <gaiainternal/phone/TelephonyManager.h>
#include <gaiainternal/phone/ServiceState.h>
#include <gaiainternal/ConnectivityManager.h>
#include <gaiainternal/System.h>
#include <gaiainternal/util/TimeUtils.h>
#include <gaiainternal/text/format/DateUtils.h>
#include <utils/RecursiveMutex.h>
#include <gaiainternal/os/SystemProperties.h>
#include <gaiainternal/ParcelHelper.h>
#include <gaiainternal/os/FileUtils.h>
#include <../../nio/services/am/ProcessStats.h>
#include <gaiainternal/io/StringWriter.h>
using namespace android;

#define NULL_RTN(a, ...)\
    do {\
        if (a == NULL)\
        {\
            GLOGW(__VA_ARGS__);\
            return;\
        }\
    } while (0)

#define NULL_RTN_VAL(a, value, ...)\
    do {\
        if (a == NULL)\
        {\
            GLOGW(__VA_ARGS__);\
            return value;\
        }\
    } while (0)

#define DEBUG_ON 0
#define DEBUG_SECURITY 1

USING_NAMESPACE(GAIA_NAMESPACE)

IMPLEMENT_DYNCREATE_PARCELABLE_CREATOR(BatteryStatsImpl)
IMPLEMENT_DYNCREATE(BatteryStatsImpl, TYPEINFO(BatteryStats))
IMPLEMENT_DYNAMIC(BatteryStatsImpl::BatteryCallback, TYPEINFO(Object))
IMPLEMENT_DYNAMIC(BatteryStatsImpl::MyHandler, TYPEINFO(Handler))
IMPLEMENT_DYNAMIC(BatteryStatsImpl::WakelockHistory, TYPEINFO(Object))
IMPLEMENT_DYNAMIC(BatteryStatsImpl::WakelockHistoryComparator, TYPEINFO(GComparator<WakelockHistory>))
IMPLEMENT_DYNAMIC(BatteryStatsImpl::Unpluggable, TYPEINFO(Object))
IMPLEMENT_DYNAMIC(BatteryStatsImpl::Counter, TYPEINFO(BatteryStats::Counter), TYPEINFO(Unpluggable))
IMPLEMENT_DYNAMIC(BatteryStatsImpl::SamplingCounter, TYPEINFO(Counter))
IMPLEMENT_DYNAMIC(BatteryStatsImpl::Timer, TYPEINFO(BatteryStats::Timer), TYPEINFO(Unpluggable))
IMPLEMENT_DYNAMIC(BatteryStatsImpl::SamplingTimer, TYPEINFO(Timer))
IMPLEMENT_DYNAMIC(BatteryStatsImpl::StopwatchTimer, TYPEINFO(Timer))
IMPLEMENT_DYNAMIC(BatteryStatsImpl::GpuTimer, TYPEINFO(Timer))
IMPLEMENT_DYNAMIC(BatteryStatsImpl::KernelWakelockStats, TYPEINFO(Object))
IMPLEMENT_DYNAMIC(BatteryStatsImpl::Uid, TYPEINFO(BatteryStats::Uid))
IMPLEMENT_DYNAMIC(BatteryStatsImpl::Uid::Wakelock, TYPEINFO(BatteryStats::Uid::Wakelock))
#ifdef SENSOR_ENABLE
IMPLEMENT_DYNAMIC(BatteryStatsImpl::Uid::Sensor, TYPEINFO(BatteryStats::Uid::Sensor))
#endif  // SENSOR_ENABLE
IMPLEMENT_DYNAMIC(BatteryStatsImpl::Uid::Proc, TYPEINFO(BatteryStats::Uid::Proc), TYPEINFO(Unpluggable))
IMPLEMENT_DYNAMIC(BatteryStatsImpl::Uid::Pkg, TYPEINFO(BatteryStats::Uid::Pkg), TYPEINFO(Unpluggable))
IMPLEMENT_DYNAMIC(BatteryStatsImpl::Uid::Pkg::Serv, TYPEINFO(BatteryStats::Uid::Pkg::Serv), TYPEINFO(Unpluggable))
IMPLEMENT_DYNAMIC(BatteryStatsImpl::BatteryStatsImplCreator, TYPEINFO(Parcelable::Creator<BatteryStatsImpl>))
IMPLEMENT_DYNAMIC(BatteryStatsImpl::BatteryStatsImplThread, TYPEINFO(GThread))
IMPLEMENT_DYNAMIC(BatteryStatsImpl::HandleBatteryStatsDataThread, TYPEINFO(GThread))

int32_t BatteryStatsImpl::sNumSpeedSteps = 0;
int32_t BatteryStatsImpl::sNumGpuSpeedSteps= 0;
int32_t BatteryStatsImpl::sKernelWakelockUpdateVersion = 0;

const sp<String> BatteryStatsImpl::BATCHED_WAKELOCK_NAME() {
    static sp<String> partialName = new String("*overflow*");
    return partialName;
}

/* static */
const sp< Object > BatteryStatsImpl::sLockObject() {
    static sp< Object > sLockObject = new Object();
    return sLockObject;
}

/* static */
const sp< Object > BatteryStatsImpl::sLockPlug() {
    static sp< Object > sLockPlug = new Object();
    return sLockPlug;
}

const sp<Parcelable::Creator<BatteryStatsImpl> > BatteryStatsImpl::CREATOR() {
    static  const sp<Parcelable::Creator<BatteryStatsImpl> > creator = new BatteryStatsImplCreator();
    return creator;
}

BatteryStatsImpl::BatteryStatsImplCreator::BatteryStatsImplCreator()
    :PREINIT_DYNAMIC() {
    GLOGENTRY();
    android::CTOR_SAFE;
}

sp<BatteryStatsImpl> BatteryStatsImpl::BatteryStatsImplCreator::createFromParcel(const Parcel& in) const {
    GLOGENTRY();

    return new BatteryStatsImpl(in);
}

sp<Blob< sp<BatteryStatsImpl> > > BatteryStatsImpl::BatteryStatsImplCreator::newArray(int32_t size) const {
    GLOGENTRY();

    return new Blob<sp<BatteryStatsImpl> >(size);
}

BatteryStatsImpl::BatteryStatsImplThread::BatteryStatsImplThread(const wp<BatteryStatsImpl>& parent)
    : PREINIT_DYNAMIC(),
    mCtorSafe(new ctor_safe(this)),
    mBS(parent) {
    GLOGENTRY();
    delete mCtorSafe;
}

BatteryStatsImpl::BatteryStatsImplThread::~BatteryStatsImplThread() {
    GLOGENTRY();
}

bool BatteryStatsImpl::BatteryStatsImplThread::threadLoop() {
    GLOGENTRY();

    sp<BatteryStatsImpl> mbs = mBS.promote();
    NULL_RTN_VAL(mbs, false, "Promotion to sp<BatteryStatsImpl> fails");

    Process::setThreadPriority(Process::THREAD_PRIORITY_BACKGROUND);
    mbs->commitPendingDataToDisk();
    return false;
}

BatteryStatsImpl::HandleBatteryStatsDataThread::HandleBatteryStatsDataThread(const wp<BatteryStatsImpl>& parent)
    : PREINIT_DYNAMIC(),
    mCtorSafe(new ctor_safe(this)),
    mBS(parent) {
    GLOGENTRY();
    delete mCtorSafe;
}

BatteryStatsImpl::HandleBatteryStatsDataThread::~HandleBatteryStatsDataThread() {
    GLOGENTRY();
}

bool BatteryStatsImpl::HandleBatteryStatsDataThread::threadLoop() {
    GLOGENTRY();

    sp<BatteryStatsImpl> mbs = mBS.promote();
    NULL_RTN_VAL(mbs, false, "Promotion to sp<BatteryStatsImpl> fails");

    int32_t uidSize = 0;
    int32_t count = 0;

    // try {
    // Thread.sleep(UID_TIME_DEL_COUNT_TO_SLEEP);
    usleep(100000);  // 100ms
    // } catch (Exception e) {
    // }

    if (DEBUG_ON) {
        GLOGD("Start remove Uid data");
    }

    // synchronized (sLockObject) {
    {
        GLOGAUTOMUTEX(_l, mbs->mObjectLock);

        uidSize = mbs->mUidStatsDel.size();
        /*
        if (DEBUG_ON) {
            Slog.d(TAG, "size = " + uidSize);
        }
        */
    }

    for (int32_t i = 0; i < uidSize; i++) {
        if (mbs->mUidStatsDel.valueAt(i)->reset()) {
            mbs->mUidStatsDel.removeItem(mbs->mUidStatsDel.keyAt(i));
            i--;
        }

        count++;

        if (count >= UID_NUM_DEL_COUNT_TO_SLEEP) {
            count = 0;

            // try {
            //    Thread.sleep(UID_TIME_DEL_COUNT_TO_SLEEP);
            usleep(100000);  // 100ms
            // } catch (Exception e) {
        }
        // synchronized (sLockObject) {
        {
            GLOGAUTOMUTEX(_l, mbs->mObjectLock);

            uidSize = mbs->mUidStatsDel.size();
        }
    }


    if (DEBUG_ON) {
        GLOGD("End remove Uid data and size = %d", uidSize);
    }

    // uidSize should be 0, but add protection for uidSize > 0
    if (uidSize > 0) {
        mbs->mUidStatsDel.clear();
    }

    // synchronized (sLockObject) {
    {
        GLOGAUTOMUTEX(_l, mbs->mObjectLock);

        mbs->mUidStatsDel.clear();
    }
    return false;
}

sp<BatteryStatsImpl::WakelockHistoryComparator> BatteryStatsImpl::sWakelockHistoryComparator() {
    GLOGENTRY();

    static sp<BatteryStatsImpl::WakelockHistoryComparator> tmp = new WakelockHistoryComparator();
    return tmp;
}

// private
// static
sp<Blob<int32_t> > BatteryStatsImpl::PROC_WAKELOCKS_FORMAT
     = BatteryStatsImpl::initPROC_WAKELOCKS_FORMAT();

// private
// static
sp<Blob<int32_t> > BatteryStatsImpl::initPROC_WAKELOCKS_FORMAT() {
    static sp<Blob<int32_t> > temp = new Blob<int32_t>(6);
    (*temp)[0] = Process::PROC_TAB_TERM|Process::PROC_OUT_STRING;  // 0: name
    (*temp)[1] = Process::PROC_TAB_TERM|Process::PROC_OUT_LONG;  // 1: count
    (*temp)[2] = Process::PROC_TAB_TERM;
    (*temp)[3] = Process::PROC_TAB_TERM;
    (*temp)[4] = Process::PROC_TAB_TERM;
    (*temp)[5] = Process::PROC_TAB_TERM|Process::PROC_OUT_LONG;  // 5: totalTime
    return  temp;
}

BatteryStatsImpl::WakelockHistory::WakelockHistory()
    : PREINIT_DYNAMIC(),
    mCtorSafe(new ctor_safe(this)),
    mName(NULL),
    mSum(0),
    mTempSum(0) {
    GLOGENTRY();

    mLockTypes = new GArrayList<Integer>();
    delete mCtorSafe;
}

BatteryStatsImpl::WakelockHistory::~WakelockHistory() {
    GLOGENTRY();
}

BatteryStatsImpl::WakelockHistoryComparator::WakelockHistoryComparator()
    : PREINIT_DYNAMIC(),
    mCtorSafe(new ctor_safe(this)) {
    GLOGENTRY();

    delete mCtorSafe;
}

BatteryStatsImpl::WakelockHistoryComparator::~WakelockHistoryComparator() {
    GLOGENTRY();
}

// Customize +++
int32_t BatteryStatsImpl::WakelockHistoryComparator::compare(const android::sp<WakelockHistory>& wh1, const android::sp<WakelockHistory>& wh2) const {
    GLOGENTRY();

    if (wh1->mTempSum > wh2->mTempSum)
        return -1;
    else if (wh1->mTempSum < wh2->mTempSum)
        return 1;
    return wh1->mName->compareTo(wh2->mName);
}
// Customize ---

BatteryStatsImpl::BatteryStatsImpl()
    : PREINIT_DYNAMIC(),
    mCtorSafe(new ctor_safe(this)) {
    GLOGENTRY();

    initFields();
    delete mCtorSafe;
}

BatteryStatsImpl::~BatteryStatsImpl() {
    GLOGENTRY();

    if (mUnpluggables != NULL) {
        mUnpluggables->clear();
    }

    mUidStats.clear();
    mUidStatsDel.clear();
}

BatteryStatsImpl::BatteryStatsImpl(const sp<String>& filename)
    : PREINIT_DYNAMIC(),
    mCtorSafe(new ctor_safe(this)) {
    GLOGENTRY();

    initFields();

    mFile = new JournaledFile(new File(filename), new File(filename + ".tmp"));
    mHandler = new MyHandler(this);
    mStartCount++;

    mScreenOnTimer = new StopwatchTimer(this, NULL, -1, NULL, mUnpluggables);

#ifdef BACKLIGHT_ENABLE
    for (int32_t i = 0; i < NUM_SCREEN_BRIGHTNESS_BINS; i++) {
        (*mScreenBrightnessTimer)[i] = new StopwatchTimer(this, NULL, -100 - i, NULL, mUnpluggables);
    }
#endif  // BACKLIGHT_ENABLE

    mInputEventCounter = new Counter(this, mUnpluggables, 0);

    mPhoneOnTimer = new StopwatchTimer(this, NULL, -2, NULL, mUnpluggables);

    for (int32_t i = 0; i < SignalStrength::NUM_SIGNAL_STRENGTH_BINS; i++) {
        (*mPhoneSignalStrengthsTimer)[i] = new StopwatchTimer(this, NULL, -200-i, NULL, mUnpluggables);
    }

    mPhoneSignalScanningTimer = new StopwatchTimer(this, NULL, -200+1, NULL, mUnpluggables);
    for (int32_t i = 0; i < BatteryStats::NUM_DATA_CONNECTION_TYPES; i++) {
        (*mPhoneDataConnectionsTimer)[i] = new StopwatchTimer(this, NULL, -300-i, NULL, mUnpluggables);
    }

#ifdef WIFI_ENABLE
    mWifiOnTimer = new StopwatchTimer(this, NULL, -3, NULL, mUnpluggables);
    mGlobalWifiRunningTimer = new StopwatchTimer(this, NULL, -4, NULL, mUnpluggables);
#endif  // WIFI_ENABLE

    mBluetoothOnTimer = new StopwatchTimer(this, NULL, -5, NULL, mUnpluggables);

    mAudioOnTimer = new StopwatchTimer(this, NULL, -6, NULL, mUnpluggables);
    mVideoOnTimer = new StopwatchTimer(this, NULL, -7, NULL, mUnpluggables);
    mOnBattery = mOnBatteryInternal = false;

    // Customize +++
    if (1) {  // (ProfileConfig::getProfileDebugWakelock()) {
        mBatteryStatusList->add(new Boolean(false));
        mBatteryStatusTimeList->add(new LongLong(System::currentTimeMillis()));
    }
    // Customize ---

    initTimes();
    mTrackBatteryPastUptime = 0;
    mTrackBatteryPastRealtime = 0;
    mUptimeStart = mTrackBatteryUptimeStart = uptimeMillis() * 1000;
    mRealtimeStart = mTrackBatteryRealtimeStart = elapsedRealtime() * 1000;
    mUnpluggedBatteryUptime = getBatteryUptimeLocked(mUptimeStart);
    mUnpluggedBatteryRealtime = getBatteryRealtimeLocked(mRealtimeStart);
    mDischargeStartLevel = 0;
    mDischargeUnplugLevel = 0;
    mDischargeCurrentLevel = 0;
    initDischarge();
    clearHistoryLocked();
    delete mCtorSafe;
}

BatteryStatsImpl::BatteryStatsImpl(const Parcel& p)
    : PREINIT_DYNAMIC(),
    mCtorSafe(new ctor_safe(this)) {
    GLOGENTRY();

    initFields();
    clearHistoryLocked();
    readFromParcel(p);
    delete mCtorSafe;
}

KeyedVector<sp<String>, sp<BatteryStats::Timer> > BatteryStatsImpl::getKernelWakelockStats() {
    GLOGENTRY();

    // workaround for ignore dynamic_cast, dynamic_cast can do well.
    KeyedVector<sp<String>, sp<BatteryStats::Timer> > _tmpVector;
    _tmpVector.clear();

    for (uint32_t i = 0; i < mKernelWakelockStats.size(); i++) {
        sp<BatteryStats::Timer> _timer = mKernelWakelockStats.valueAt(i);  // upper cast, ok
        _tmpVector.add(mKernelWakelockStats.keyAt(i), _timer);
    }
    return _tmpVector;  // TODO +++ local varable
}

void BatteryStatsImpl::initFields() {
    GLOGENTRY();

    mCallbackCount = 0;
    mFile = NULL;
    mHandler = NULL;
    mCallback = NULL;
    mPartialTimers = new GArrayList<StopwatchTimer>();
    mFullTimers = new GArrayList<StopwatchTimer>();
    mWindowTimers = new GArrayList<StopwatchTimer>();
    mWifiRunningTimers = new GArrayList<StopwatchTimer>();
    mFullWifiLockTimers = new GArrayList<StopwatchTimer>();
    mScanWifiLockTimers = new GArrayList<StopwatchTimer>();
    mWifiMulticastTimers = new GArrayList<StopwatchTimer>();
    mLastPartialTimers = new GArrayList<StopwatchTimer>();
    mUnpluggables = new GArrayList<Unpluggable>();
    // mUnpluggables->trackMe(true,false);
    mHistoryLastWritten = NULL;
    mHistoryLastLastWritten = NULL;
    mHistoryReadTmp = NULL;
    mHistoryCur = NULL;
    mHistory = NULL;
    mHistoryEnd = NULL;
    mHistoryLastEnd = NULL;
    mHistoryCache = NULL;
    mHistoryIterator = NULL;
    mScreenOnTimer = NULL;

#ifdef BACKLIGHT_ENABLE
    mScreenBrightnessTimer = NULL;
#endif  // BACKLIGHT_ENABLE
    mInputEventCounter = NULL;

    mPhoneOnTimer = NULL;

    mAudioOnTimer = NULL;
    mVideoOnTimer = NULL;

    mPhoneSignalStrengthsTimer = NULL;
    mPhoneSignalScanningTimer = NULL;
    mPhoneDataConnectionsTimer = NULL;

#ifdef WIFI_ENABLE
    mWifiOnTimer = NULL;
    mGlobalWifiRunningTimer = NULL;
#endif  // WIFI_ENABLE

    mBluetoothOnTimer = NULL;
    mBtHeadset = NULL;

    mMobileDataTx = NULL;
    mMobileDataRx = NULL;
    mTotalDataTx = NULL;
    mTotalDataRx = NULL;
    mProcWakelocksName = NULL;
    mProcWakelocksData = NULL;
    mMobileIfaces = NULL;
    mWriteLock = NULL;
    mNetworkSummaryCache = NULL;
    mNetworkDetailCache = NULL;

    mShuttingDown = 0;
    mHistoryBaseTime = 0;
    mHaveBatteryLevel = false;
    mRecordingHistory = true;
    mNumHistoryItems = 0;
    mHistoryLastWritten = new HistoryItem();
    mHistoryLastLastWritten = new HistoryItem();
    mHistoryReadTmp = new HistoryItem();
    mHistoryBufferLastPos = -1;
    mHistoryOverflow = false;
    mLastHistoryTime = 0;

    mHistoryCur = new BatteryStats::HistoryItem();

    mReadOverflow = 0;
    mIteratingHistory = 0;
    mStartCount = 0;
    mBatteryUptime = 0;
    mBatteryLastUptime = 0;
    mBatteryRealtime = 0;
    mBatteryLastRealtime = 0;
    mUptime = 0;
    mUptimeStart = 0;
    mLastUptime = 0;
    mRealtime = 0;
    mRealtimeStart = 0;
    mLastRealtime = 0;
    mScreenOn = 0;

#ifdef BACKLIGHT_ENABLE
    mScreenBrightnessBin = -1;
    mScreenBrightnessTimer = new Blob<sp<StopwatchTimer> >(NUM_SCREEN_BRIGHTNESS_BINS);
#endif  // BACKLIGHT_ENABLE

    mPhoneOn = 0;

    mAudioOn = 0;
    mVideoOn = 0;

    mPhoneSignalStrengthBin = -1;
    mPhoneSignalStrengthBinRaw = -1;
    mPhoneSignalStrengthsTimer = new Blob<sp<StopwatchTimer> >(SignalStrength::NUM_SIGNAL_STRENGTH_BINS);
    mPhoneDataConnectionType = -1;
    mPhoneDataConnectionsTimer = new Blob<sp<StopwatchTimer> >(NUM_DATA_CONNECTION_TYPES);

#ifdef WIFI_ENABLE
    mWifiOn = 0;
    mWifiOnUid = -1;
    mGlobalWifiRunning = 0;
#endif  // WIFI_ENABLE

    mBluetoothOn = 0;

    /**
     * These provide time bases that discount the time the device is plugged
     * in to power.
     */
    mOnBattery = 0;
    mOnBatteryInternal = 0;
    mTrackBatteryPastUptime = 0;
    mTrackBatteryUptimeStart = 0;
    mTrackBatteryPastRealtime = 0;
    mTrackBatteryRealtimeStart = 0;
    mUnpluggedBatteryUptime = 0;
    mUnpluggedBatteryRealtime = 0;

    /*
     * These keep track of battery levels (1-100) at the last plug event and the last unplug event.
     */
    mDischargeStartLevel = 0;
    mDischargeUnplugLevel = 0;
    mDischargeCurrentLevel = 0;
    mLowDischargeAmountSinceCharge = 0;
    mHighDischargeAmountSinceCharge = 0;
    mDischargeScreenOnUnplugLevel = 0;
    mDischargeScreenOffUnplugLevel = 0;
    mDischargeAmountScreenOn = 0;
    mDischargeAmountScreenOnSinceCharge = 0;
    mDischargeAmountScreenOff = 0;
    mDischargeAmountScreenOffSinceCharge = 0;

    mLastWriteTime = 0;  // Milliseconds
    // Mobile data transferred while on battery
    mMobileDataTx = new Blob<int64_t>(4);
    mMobileDataRx = new Blob<int64_t>(4);
    mTotalDataTx = new Blob<int64_t>(4);
    mTotalDataRx = new Blob<int64_t>(4);

    for (int32_t i = 0; i < 4; i++) {
        (*mMobileDataTx)[i] = 0;
        (*mMobileDataRx)[i] = 0;
        (*mTotalDataTx)[i] = 0;
        (*mTotalDataRx)[i] = 0;
    }
    mRadioDataUptime = 0;
    mRadioDataStart = 0;

    mBluetoothPingCount = 0;
    mBluetoothPingStart = -1;

    mPhoneServiceState = -1;
    mPhoneServiceStateRaw = -1;
    mPhoneSimStateRaw = -1;

    mDisplayUid = -1;
    mHistoryFileTime = 0;
    mHistoryShutdownTime = 0;
    mHistoryStartTimeList =  new GArrayList<LongLong>(32);

    mProcWakelocksName = new Blob<sp<String> >(3);
    mProcWakelocksData = new Blob<int64_t>(3);

    for (int32_t i = 0; i < 3; i++) {
        (*mProcWakelocksName)[i] = '\0';
        (*mProcWakelocksData)[i] = 0;
    }

    mNetworkStatsFactory = new NetworkStatsFactory();

    /** Network ifaces that {@link ConnectivityManager} has claimed as mobile. */
    mMobileIfaces = new GHashSet<String>();
    mChangedBufferStates = 0;
    mChangedStates = 0;
    mWakeLockNesting = 0;
#ifdef SENSOR_ENABLE
    mSensorNesting = 0;
#endif  // SENSOR_ENABLE
#ifdef GPS_ENABLE
    mGpsNesting = 0;
#endif  // GPS_ENABLE
#ifdef WIFI_ENABLE
    mWifiFullLockNesting = 0;
    mWifiScanLockNesting = 0;
    mWifiMulticastNesting = 0;
#endif  // WIFI_ENABLE

    mPendingWrite = NULL;

    mWriteLock = new ReentrantLock();

    mWakelockHistory = new GHashMap<String, WakelockHistory>();
    mBatteryStatusList = new GArrayList<Boolean>();
    mBatteryStatusTimeList = new GArrayList<LongLong>();

    mLastGetNetworkSummaryTime = 0;
    mLastGetNetworkDetailTime = 0;

    Clear_Containers(); 
}

KeyedVector<sp<String> , sp<BatteryStatsImpl::KernelWakelockStats> >& BatteryStatsImpl::readKernelWakelockStats() {
    GLOGENTRY();

    sp<Blob<byte_t> > buffer = new Blob<byte_t>(8192);
    int32_t len;

    sp<FileInputStream> is = new FileInputStream(String("/proc/wakelocks"));
    len = is->read(buffer);
    is->close();

    if (len > 0) {
        int32_t i;
        for (i = 0; i < len; i++) {
            if ((*buffer)[i] == '\0') {
                len = i;
                break;
            }
        }
    }

    return parseProcWakelocks(buffer, len);
}

KeyedVector<sp<String> , sp<BatteryStatsImpl::KernelWakelockStats> >& BatteryStatsImpl::parseProcWakelocks(const sp<Blob<byte_t> >& wlBuffer, int32_t len) {
    GLOGENTRY();

    sp<String> name = NULL;
    int32_t count = 0;
    int64_t totalTime = 0;
    int32_t startIndex = 0;
    int32_t endIndex = 0;
    int32_t numUpdatedWlNames = 0;

    // Advance past the first line.
    int32_t i;

    for (i = 0; i < len && wlBuffer[i] != '\n' && wlBuffer[i] != '\0'; i++);
    startIndex = endIndex = i + 1;
    // synchronized(this)
    {
        GLOGAUTOMUTEX(_l, mThisLock);

        KeyedVector<sp<String> , sp<KernelWakelockStats> >& m = mProcWakelockFileStats;

        sKernelWakelockUpdateVersion++;

        int64_t start = elapsedRealtime();
        while (endIndex < len) {
            for (endIndex = startIndex;
                    endIndex < len && wlBuffer[endIndex] != '\n' && wlBuffer[endIndex] != '\0';
                    endIndex++);

            endIndex++;  // endIndex is an exclusive upper bound.
            // Don't go over the end of the buffer, Process.parseProcLine might
            // write to wlBuffer[endIndex]
            if (endIndex >= (len - 1)) {
                return m;
            }

            sp<Blob<sp<String> > > nameStringArray = mProcWakelocksName;
            sp<Blob<int64_t> > wlData = mProcWakelocksData;
            // Stomp out any bad characters since this is from a circular buffer
            // A corruption is seen sometimes that results in the vm crashing
            // This should prevent crashes and the line will probably fail to parse
            for (int32_t j = startIndex; j < endIndex; j++) {
                if ((wlBuffer[j] & 0x80) != 0) wlBuffer[j] = (byte) '?';
            }

            //-----Convert Blob to Vector-----

            Vector<char> tmpwlBuffer;
            Vector<int> tmpPROC_WAKELOCKS_FORMAT;
            Vector<sp<String> > tmpnameStringArray;
            Vector<int64_t> tmpwlData;
            Vector<float> tmpVectorNULL;

            if (wlBuffer != NULL && (static_cast<int32_t>(wlBuffer->length()) > 0)) {
                for (uint32_t i = 0; i < wlBuffer->length(); i++) {
                    tmpwlBuffer.add(static_cast<char>((*wlBuffer)[i]));
                }
            }

            if (PROC_WAKELOCKS_FORMAT != NULL && (static_cast<int32_t>(PROC_WAKELOCKS_FORMAT->length()) > 0)) {
                for (uint32_t i = 0; i < PROC_WAKELOCKS_FORMAT->length(); i++) {
                    tmpPROC_WAKELOCKS_FORMAT.add((int)(*PROC_WAKELOCKS_FORMAT)[i]);
                }
            }

            if (nameStringArray != NULL && (static_cast<int32_t>(nameStringArray->length()) > 0)) {
                for (uint32_t i = 0; i < nameStringArray->length(); i++) {
                    tmpnameStringArray.add((*nameStringArray)[i]);
                }
            }

            if (wlData != NULL && (static_cast<int32_t>(wlData->length()) > 0)) {
                for (uint32_t i = 0; i < wlData->length(); i++) {
                    tmpwlData.add((int64_t)(*wlData)[i]);
                }
            }

            //-----Convert Blob to Vector-----
            name = tmpnameStringArray[0];
            #if 1
            bool parsed = Process::parseProcLine(tmpwlBuffer,
                                                 startIndex,
                                                 endIndex,
                                                 tmpPROC_WAKELOCKS_FORMAT,
                                                 tmpnameStringArray,
                                                 tmpwlData,
                                                 tmpVectorNULL);
            #else
            bool parsed = Process::parseProcLine(wlBuffer,
                                                 startIndex,
                                                 endIndex,
                                                 PROC_WAKELOCKS_FORMAT,
                                                 nameStringArray,
                                                 wlData,
                                                 NULL);
            #endif  // temp
            name = tmpnameStringArray[0];
            count = static_cast<int32_t>(tmpwlData[1]);
            totalTime = (tmpwlData[2] + 500) / 1000;

            if (parsed && name->length() > 0) {
                int32_t index = m.indexOfKey(name);

                if (!(index >= 0)) {
                    m.add(name, new KernelWakelockStats(count, totalTime, sKernelWakelockUpdateVersion));
                    numUpdatedWlNames++;
                } else {
                    sp<KernelWakelockStats> kwlStats = m.valueAt(index);
                    if (kwlStats->mVersion == sKernelWakelockUpdateVersion) {
                        kwlStats->mCount += count;
                        kwlStats->mTotalTime += totalTime;
                    } else {
                        kwlStats->mCount = count;
                        kwlStats->mTotalTime = totalTime;
                        kwlStats->mVersion = sKernelWakelockUpdateVersion;
                        numUpdatedWlNames++;
                    }
                }
                startIndex = endIndex;
            }
        }
        int64_t end = elapsedRealtime();
        GLOGI("BatteryStatsImpl::parseProcWakelocks 1-1 spends time start=%lld end=%lld duration=%lld",start, end, (end - start));

        GLOGI("BatteryStatsImpl::parseProcWakelocks m size=%d",m.size());

        if (static_cast<int32_t>(m.size()) != numUpdatedWlNames) {
            // Don't report old data.
            start = elapsedRealtime();
            for (int32_t i = static_cast<int32_t>(m.size()) - 1; i >= 0; i--) {
                sp<KernelWakelockStats> s = safe_cast<BatteryStatsImpl::KernelWakelockStats*>(m.valueAt(i).get());
                if (s->mVersion != sKernelWakelockUpdateVersion) {
                    m.removeItem(m.keyAt(i));
                }
            }
            end = elapsedRealtime();
            GLOGI("BatteryStatsImpl::parseProcWakelocks 2-2 spends time start=%lld end=%lld duration=%lld",start, end, (end - start));
        }
        return m;
    }
}

sp<BatteryStatsImpl::SamplingTimer> BatteryStatsImpl::getKernelWakelockTimerLocked(const sp<String>& name) {
    GLOGENTRY();

    sp<SamplingTimer> kwlt = NULL;
    int32_t index = mKernelWakelockStats.indexOfKey(name);

    if (index >= 0) {
        kwlt = mKernelWakelockStats.valueAt(index);
    }

    if (kwlt == NULL) {
        kwlt = new SamplingTimer(this, mUnpluggables, mOnBatteryInternal, true);  // track reported values
        mKernelWakelockStats.add(name, kwlt);
    }
    return kwlt;
}

void BatteryStatsImpl::doDataPlug(const sp< Blob<int64_t> >& dataTransfer, int64_t currentBytes) {
    GLOGENTRY();

    dataTransfer[STATS_LAST] = dataTransfer[STATS_SINCE_UNPLUGGED];
    dataTransfer[STATS_SINCE_UNPLUGGED] = -1;
}

void BatteryStatsImpl::doDataUnplug(const sp< Blob<int64_t> >& dataTransfer, int64_t currentBytes) {
    GLOGENTRY();

    dataTransfer[STATS_SINCE_UNPLUGGED] = currentBytes;
}

int64_t BatteryStatsImpl::getCurrentRadioDataUptime() {
    GLOGENTRY();

    sp<File> awakeTimeFile = new File(new String("/sys/devices/virtual/net/rmnet0/awake_time_ms"));

    if (!awakeTimeFile->exists()) {
        return 0;
    }

    sp<BufferedReader> br = new BufferedReader(sp<FileReader>(new FileReader(awakeTimeFile)));
    sp<String> line = br->readLine();
    br->close();
    return LongLong::parseLongLong(line) * 1000;
}

int64_t BatteryStatsImpl::getRadioDataUptimeMs() {
    GLOGENTRY();

    return getRadioDataUptime() / 1000;
}

int64_t BatteryStatsImpl::getRadioDataUptime() {
    GLOGENTRY();

    if (mRadioDataStart == -1) {
        return mRadioDataUptime;
    } else {
        return getCurrentRadioDataUptime() - mRadioDataStart;
    }
}

int32_t BatteryStatsImpl::getCurrentBluetoothPingCount() {
    GLOGENTRY();

    if (mBtHeadset != NULL) {
        sp<GList<BluetoothDevice> > deviceList = mBtHeadset->getConnectedDevices();

        if (deviceList->size() > 0) {
            return mBtHeadset->getBatteryUsageHint(deviceList->get(0));
        }
    }

    return -1;
}

int32_t BatteryStatsImpl::getBluetoothPingCount() {
    GLOGENTRY();

    if (mBluetoothPingStart == -1) {
        return mBluetoothPingCount;
    } else if (mBtHeadset != NULL) {
        return getCurrentBluetoothPingCount() - mBluetoothPingStart;
    }

    return 0;
}

void BatteryStatsImpl::setBtHeadset(const sp<BluetoothHeadset>& headset) {
    GLOGENTRY();

    if (headset != NULL && mBtHeadset == NULL && isOnBattery() && mBluetoothPingStart == -1) {
        mBluetoothPingStart = getCurrentBluetoothPingCount();
    }

    if (mBtHeadset != NULL) {
        GLOGI("mBtHeadset  getStrongCount = %d", mBtHeadset->getStrongCount());
    }

    mBtHeadset = headset;
}

void BatteryStatsImpl::addHistoryBufferLocked(int64_t curTime) {
    GLOGENTRY();

    if (!mHaveBatteryLevel || !mRecordingHistory) {
        return;
    }

    // Customize +++
    if (mHistoryFileTime <= 0) {
        mHistoryFileTime = System::currentTimeMillis();

        if (DEBUG_ON) {
            sp<StringBuilder> sb = new StringBuilder(128);
            sb->append("******************current mHistoryFileTime: ");
            TimeUtils::formatDuration(mHistoryFileTime, sb);
            GLOGI("%s", sb->toString()->string());
        }
    }
    // Customize ---

    const int64_t timeDiff = (mHistoryBaseTime + curTime) - mHistoryLastWritten->time;

    if (mHistoryBufferLastPos >= 0 && mHistoryLastWritten->cmd == HistoryItem::CMD_UPDATE
            && timeDiff < 2000
            && ((mHistoryLastWritten->states ^ mHistoryCur->states) & mChangedBufferStates) == 0) {
        // If the current is the same as the one before, then we no
        // longer need the entry.
        mHistoryBuffer.setDataSize(mHistoryBufferLastPos);
        mHistoryBuffer.setDataPosition(mHistoryBufferLastPos);
        mHistoryBufferLastPos = -1;

        if (mHistoryLastLastWritten->cmd == HistoryItem::CMD_UPDATE
                && timeDiff < 500 && mHistoryLastLastWritten->same(mHistoryCur)) {
            // If this results in us returning to the state written
            // prior to the last one, then we can just delete the last
            // written one and drop the new one.  Nothing more to do.
            mHistoryLastWritten->setTo(mHistoryLastLastWritten);
            mHistoryLastLastWritten->cmd = HistoryItem::CMD_NULL;
            return;
        }

        mChangedBufferStates |= mHistoryLastWritten->states ^ mHistoryCur->states;
        curTime = mHistoryLastWritten->time - mHistoryBaseTime;
        mHistoryLastWritten->setTo(mHistoryLastLastWritten);
    } else {
        mChangedBufferStates = 0;
    }

    const int32_t dataSize = mHistoryBuffer.dataSize();

    if (dataSize >= MAX_HISTORY_BUFFER) {
        if (!mHistoryOverflow) {
            mHistoryOverflow = true;
            addHistoryBufferLocked(curTime, HistoryItem::CMD_OVERFLOW);
        }

        // Once we've reached the maximum number of items, we only
        // record changes to the battery level and the most interesting states.
        // Once we've reached the maximum maximum number of items, we only
        // record changes to the battery level.
        if (mHistoryLastWritten->batteryLevel == mHistoryCur->batteryLevel &&
                (dataSize >= MAX_MAX_HISTORY_BUFFER
                 || ((mHistoryLastWritten->states ^ mHistoryCur->states)
                     & HistoryItem::MOST_INTERESTING_STATES) == 0)) {
            return;
        }
    }

    addHistoryBufferLocked(curTime, HistoryItem::CMD_UPDATE);
}

void BatteryStatsImpl::addHistoryBufferLocked(int64_t curTime, byte_t cmd) {
    GLOGENTRY();

    int32_t origPos = 0;
    if (mIteratingHistory) {
        origPos = mHistoryBuffer.dataPosition();
        mHistoryBuffer.setDataPosition(mHistoryBuffer.dataSize());
    }
    mHistoryBufferLastPos = mHistoryBuffer.dataPosition();
    mHistoryLastLastWritten->setTo(mHistoryLastWritten);
    mHistoryLastWritten->setTo(mHistoryBaseTime + curTime, cmd, mHistoryCur);
    mHistoryLastWritten->writeDelta(mHistoryBuffer, mHistoryLastLastWritten);
    mLastHistoryTime = curTime;

    if (DEBUG_HISTORY) {
        GLOGI("%sWriting history buffer: was %d now %d size is now %d", LOG_TAG,
                                                                        mHistoryBufferLastPos,
                                                                        mHistoryBuffer.dataPosition(),
                                                                        mHistoryBuffer.dataSize());
    }

    if (mIteratingHistory) {
        mHistoryBuffer.setDataPosition(origPos);
    }
}

void BatteryStatsImpl::addHistoryRecordLocked(int64_t curTime) {
    GLOGENTRY();

    addHistoryBufferLocked(curTime);

    if (!USE_OLD_HISTORY) {
        return;
    }

    if (!mHaveBatteryLevel || !mRecordingHistory) {
        return;
    }

    // If the current time is basically the same as the last time,
    // and no states have since the last recorded entry changed and
    // are now resetting back to their original value, then just collapse
    // into one record.
    if (mHistoryEnd != NULL && mHistoryEnd->cmd == HistoryItem::CMD_UPDATE
            && (mHistoryBaseTime + curTime) < (mHistoryEnd->time + 2000)
            && ((mHistoryEnd->states ^ mHistoryCur->states) & mChangedStates) == 0) {
        // If the current is the same as the one before, then we no
        // longer need the entry.
        if (mHistoryLastEnd != NULL && mHistoryLastEnd->cmd == HistoryItem::CMD_UPDATE
                && (mHistoryBaseTime + curTime) < (mHistoryEnd->time + 500)
                && mHistoryLastEnd->same(mHistoryCur)) {
            mHistoryLastEnd->next = NULL;
            mHistoryEnd->next = mHistoryCache;
            mHistoryCache = mHistoryEnd;
            mHistoryEnd = mHistoryLastEnd;
            mHistoryLastEnd = NULL;
        } else {
            mChangedStates |= mHistoryEnd->states ^ mHistoryCur->states;
            mHistoryEnd->setTo(mHistoryEnd->time, HistoryItem::CMD_UPDATE, mHistoryCur);

            // Customize +++
            if (1) {  // (ProfileConfig::getProfileDebugBatteryHistory())
                GLOGD("%s", String::format("Record %04d Updated at %08x (%08x+%08x)", mNumHistoryItems, mHistoryBaseTime + curTime, mHistoryBaseTime, curTime)->string());
            }
            // Customize ---
        }

        return;
    }

    mChangedStates = 0;

    if (mNumHistoryItems == MAX_HISTORY_ITEMS
            || mNumHistoryItems == MAX_MAX_HISTORY_ITEMS) {
        addHistoryRecordLocked(curTime, HistoryItem::CMD_OVERFLOW);
    }

    if (mNumHistoryItems >= MAX_HISTORY_ITEMS) {
        // Once we've reached the maximum number of items, we only
        // record changes to the battery level and the most interesting states.
        // Once we've reached the maximum maximum number of items, we only
        // record changes to the battery level.
        if (mHistoryEnd != NULL && mHistoryEnd->batteryLevel
                == mHistoryCur->batteryLevel &&
                (mNumHistoryItems >= MAX_MAX_HISTORY_ITEMS
                 || ((mHistoryEnd->states ^ mHistoryCur->states)
                     & HistoryItem::MOST_INTERESTING_STATES) == 0)) {
            return;
        }
    }

    addHistoryRecordLocked(curTime, HistoryItem::CMD_UPDATE);
}

void BatteryStatsImpl::addHistoryRecordLocked(int64_t curTime, byte_t cmd) {
    GLOGENTRY();

    sp<HistoryItem> rec = mHistoryCache;

    if (rec != NULL) {
        mHistoryCache = rec->next;
    } else {
        rec = new HistoryItem();
    }

    rec->setTo(mHistoryBaseTime + curTime, cmd, mHistoryCur);
    addHistoryRecordLocked(rec);

    // Customize +++
    if (1) {  // (ProfileConfig::getProfileDebugBatteryHistory())
        GLOGD("%s", String::format("Record %04d Added at %08x (%08x+%08x)", mNumHistoryItems, mHistoryBaseTime+curTime, mHistoryBaseTime, curTime)->string());
    }
    // Customize ---
}

void BatteryStatsImpl::addHistoryRecordLocked(const sp<HistoryItem>& rec) {
    GLOGENTRY();

    mNumHistoryItems++;
    rec->next = NULL;
    mHistoryLastEnd = mHistoryEnd;

    if (mHistoryEnd != NULL) {
        mHistoryEnd->next = rec;
        mHistoryEnd = rec;
    } else {
        mHistory = mHistoryEnd = rec;
    }
}

void BatteryStatsImpl::clearHistoryLocked() {
    GLOGENTRY();

    // Customize +++
    if (DEBUG_ON) {
        GLOGD("%s", "********** CLEARING HISTORY!");
    }
    // Customize ---

    if (DEBUG_HISTORY) {
        GLOGI(LOG_TAG "********** CLEARING HISTORY!");
    }

    if (USE_OLD_HISTORY) {
        if (mHistory != NULL) {
            mHistoryEnd->next = mHistoryCache;
            mHistoryCache = mHistory;
            mHistory = mHistoryLastEnd = mHistoryEnd = NULL;
        }

        mNumHistoryItems = 0;
    }

    mHistoryBaseTime = 0;
    mLastHistoryTime = 0;

    // Customize +++
    mHistoryFileTime = 0;
    mHistoryShutdownTime = 0;

    if (DEBUG_ON) {
        GLOGD("Start times = %d", mHistoryStartTimeList->size());
    }

    mHistoryStartTimeList->clear();
    // Customize ---

    mHistoryBuffer.setDataSize(0);
    mHistoryBuffer.setDataPosition(0);
    mHistoryBuffer.setDataCapacity(MAX_HISTORY_BUFFER / 2);
    mHistoryLastLastWritten->cmd = HistoryItem::CMD_NULL;
    mHistoryLastWritten->cmd = HistoryItem::CMD_NULL;
    mHistoryBufferLastPos = -1;
    mHistoryOverflow = false;
}

void BatteryStatsImpl::doUnplugLocked(int64_t batteryUptime, int64_t batteryRealtime) {
    GLOGENTRY();

    sp<NetworkStats::Entry> entry = NULL;

    // Customize +++
    int64_t startTimeMillis = 0;
    int64_t tempTime = 0;

    if (DEBUG_ON) {
        GLOGD("doUnplugLocked Start uptime = %lld ,realtime = %lld", batteryUptime, batteryRealtime);
    }

    if (DEBUG_SECURITY) {
        startTimeMillis = uptimeMillis();
    }
    // Customize ---

    // Track UID data usage
    const sp<NetworkStats> uidStats = getNetworkStatsDetailGroupedByUid();

    // Customize +++
    if (DEBUG_SECURITY) {
        tempTime = uptimeMillis() - startTimeMillis;
        if (tempTime >= BATTERY_MAX_RUN_TIME_300MS) {
            GLOGW("getNetworkStatsDetailGroupedByUid plugout spends time = %lld", tempTime);
        }
        startTimeMillis = uptimeMillis();
    }
    // Customize ---

    const int32_t size = uidStats->size();

    {
        GLOGAUTOMUTEX(_l, mThisLock);

        for (int32_t i = 0; i < size; i++) {
            entry = uidStats->getValues(i, entry);
            sp<Uid> u = NULL;
            int32_t index = mUidStats.indexOfKey(entry->muid);
            if (index >= 0) {
                u = mUidStats.valueAt(index);
            }

            if (u == NULL) continue;

            u->mStartedTcpBytesReceived = entry->mrxBytes;
            u->mStartedTcpBytesSent = entry->mtxBytes;
            u->mTcpBytesReceivedAtLastUnplug = u->mCurrentTcpBytesReceived;
            u->mTcpBytesSentAtLastUnplug = u->mCurrentTcpBytesSent;
        }
    }

    // Customize +++
    // synchronized (sLockPlug) {
    {
        GLOGAUTOMUTEX(_l, mPlugLock);

        if (DEBUG_SECURITY) {
            GLOGD("out before mUnpluggables size = %d", mUnpluggables->size());
        }

        for (int32_t i = mUnpluggables->size() - 1; i >= 0; i--) {
            // try
            {
                mUnpluggables->get(i)->unplug(batteryUptime, batteryRealtime);
            }
            // catch (Exception *e)
            // {
            //   GLOGW("mUnpluggables.get("+i+").unplug exception");
            // }
        }

        if (DEBUG_SECURITY) {
            GLOGD("out after mUnpluggables size = %d", mUnpluggables->size());
        }
    }
    // Customize ---

    // Customize +++
    if (DEBUG_SECURITY) {
        tempTime = uptimeMillis() - startTimeMillis;
        if (tempTime >= BATTERY_MAX_RUN_TIME_300MS) {
            GLOGW("mUidStats plugout spends time = %lld", tempTime);
        }
        startTimeMillis = uptimeMillis();
    }
    // Customize ---

    // Track both mobile and total overall data
    const sp<NetworkStats> ifaceStats = getNetworkStatsSummary();

    // Customize +++
    if (DEBUG_SECURITY) {
        tempTime = uptimeMillis() - startTimeMillis;
        if (tempTime >= BATTERY_MAX_RUN_TIME_300MS) {
            GLOGW("getNetworkStatsSummary plugout spends time = %lld", tempTime);
        }
    }
    // Customize ---

    entry = ifaceStats->getTotal(entry, mMobileIfaces);
    doDataUnplug(mMobileDataRx, entry->mrxBytes);
    doDataUnplug(mMobileDataTx, entry->mtxBytes);
    entry = ifaceStats->getTotal(entry);
    doDataUnplug(mTotalDataRx, entry->mrxBytes);
    doDataUnplug(mTotalDataTx, entry->mtxBytes);
    // Track radio awake time
    mRadioDataStart = getCurrentRadioDataUptime();
    mRadioDataUptime = 0;
    // Track bt headset ping count
    mBluetoothPingStart = getCurrentBluetoothPingCount();
    mBluetoothPingCount = 0;

    // Customize +++
    if (DEBUG_ON) {
        GLOGD("doUnplugLocked End");
    }
    // Customize ---
}

void BatteryStatsImpl::doPlugLocked(int64_t batteryUptime, int64_t batteryRealtime) {
    GLOGENTRY();

    sp<NetworkStats::Entry> entry = NULL;

    // Customize +++
    int64_t startTimeMillis = 0;
    int64_t tempTime = 0;

    if (DEBUG_ON) {
        GLOGD("doPlugLocked Start uptime = %lld ,realtime = %lld", batteryUptime, batteryRealtime);
    }

    if (DEBUG_SECURITY) {
        startTimeMillis = uptimeMillis();
    }
    // Customize ---

    {
        GLOGAUTOMUTEX(_l, mThisLock);

        for (int32_t iu = mUidStats.size() - 1; iu >= 0; iu--) {
            sp<Uid> u;
            u = mUidStats.valueAt(iu);

            if (u->mStartedTcpBytesReceived >= 0) {
                u->mCurrentTcpBytesReceived = u->computeCurrentTcpBytesReceived();
                u->mStartedTcpBytesReceived = -1;
            }

            if (u->mStartedTcpBytesSent >= 0) {
                u->mCurrentTcpBytesSent = u->computeCurrentTcpBytesSent();
                u->mStartedTcpBytesSent = -1;
            }
        }
    }

    // Customize +++
    // synchronized (sLockPlug) {
    {
        GLOGAUTOMUTEX(_l, mPlugLock);

        if (DEBUG_SECURITY) {
            GLOGD("in before mUnpluggables size = ", mUnpluggables->size());
        }

        for (int32_t i = mUnpluggables->size() - 1; i >= 0; i--) {
            // try
            {
                mUnpluggables->get(i)->plug(batteryUptime, batteryRealtime);
            }
            // catch (Exception *e)
            // {
            //    GLOGW("mUnpluggables.get("+i+").plug exception");
            // }
        }

        if (DEBUG_SECURITY) {
            GLOGD("in after mUnpluggables size = %d", mUnpluggables->size());
        }
    }
    // Customize ---

    // Customize +++
    if (DEBUG_SECURITY) {
        tempTime = uptimeMillis() - startTimeMillis;
        if (tempTime >= BATTERY_MAX_RUN_TIME_300MS) {
            GLOGW("mUidStats plugin spends time = %lld", tempTime);
        }
        startTimeMillis = uptimeMillis();
    }
    // Customize ---

    // Track both mobile and total overall data
    const sp<NetworkStats> ifaceStats = getNetworkStatsSummary();

    // Customize +++
    if (DEBUG_SECURITY) {
        tempTime = uptimeMillis() - startTimeMillis;
        if (tempTime >= BATTERY_MAX_RUN_TIME_300MS) {
            GLOGW("getNetworkStatsSummary plugin spends time = %lld", tempTime);
        }
    }
    // Customize ---

    entry = ifaceStats->getTotal(entry, mMobileIfaces);
    doDataPlug(mMobileDataRx, entry->mrxBytes);
    doDataPlug(mMobileDataTx, entry->mtxBytes);
    entry = ifaceStats->getTotal(entry);
    doDataPlug(mTotalDataRx, entry->mrxBytes);
    doDataPlug(mTotalDataTx, entry->mtxBytes);
    // Track radio awake time
    mRadioDataUptime = getRadioDataUptime();
    mRadioDataStart = -1;
    // Track bt headset ping count
    mBluetoothPingCount = getBluetoothPingCount();
    mBluetoothPingStart = -1;

    // Customize +++
    if (DEBUG_ON) {
        GLOGD("doPlugLocked End");
    }
    // Customize ---
}

void BatteryStatsImpl::noteStartWakeLocked(int32_t uid, int32_t pid, const sp<String>& name, int32_t type) {
    GLOGENTRY();

    // Customize +++
    if (DEBUG_ON) {
        GLOGD("Start wake lock");
    }
    // Customize ---

    if (type == WAKE_TYPE_PARTIAL) {
        // Only care about partial wake locks, since full wake locks
        // will be canceled when the user puts the screen to sleep.
        if (mWakeLockNesting == 0) {
            mHistoryCur->states |= HistoryItem::STATE_WAKE_LOCK_FLAG;

            if (DEBUG_HISTORY) {
                GLOGV(LOG_TAG "Start wake lock to: %s", Integer::toHexString(mHistoryCur->states)->string());
            }

            addHistoryRecordLocked(elapsedRealtime());
        }

        mWakeLockNesting++;
    }

    if (uid >= 0) {
        if (!mHandler->hasMessages(MSG_UPDATE_WAKELOCKS)) {
            sp<Message> m = mHandler->obtainMessage(MSG_UPDATE_WAKELOCKS);
            mHandler->sendMessageDelayed(m, DELAY_UPDATE_WAKELOCKS);
        }

        getUidStatsLocked(uid)->noteStartWakeLocked(pid, name, type);
    }
}

void BatteryStatsImpl::noteStopWakeLocked(int32_t uid, int32_t pid, const sp<String>& name, int32_t type) {
    GLOGENTRY();

    // Customize +++
    if (DEBUG_ON) {
        GLOGD("Stop wake lock");
    }
    // Customize ---

    if (type == WAKE_TYPE_PARTIAL) {
        mWakeLockNesting--;

        if (mWakeLockNesting == 0) {
            mHistoryCur->states &= ~HistoryItem::STATE_WAKE_LOCK_FLAG;

            if (DEBUG_HISTORY) {
                GLOGV(LOG_TAG "Stop wake lock to: %s", Integer::toHexString(mHistoryCur->states)->string());
            }

            addHistoryRecordLocked(elapsedRealtime());
        }
    }

    if (uid >= 0) {
        if (!mHandler->hasMessages(MSG_UPDATE_WAKELOCKS)) {
            sp<Message> m = mHandler->obtainMessage(MSG_UPDATE_WAKELOCKS);
            mHandler->sendMessageDelayed(m, DELAY_UPDATE_WAKELOCKS);
        }

        getUidStatsLocked(uid)->noteStopWakeLocked(pid, name, type);
    }
}

void BatteryStatsImpl::noteStartWakeFromSourceLocked(const sp<WorkSource>& ws, int32_t pid, sp<String>& name, int32_t type) {
    GLOGENTRY();

    int32_t N = ws->size();

    for (int32_t i = 0; i < N; i++) {
        noteStartWakeLocked(ws->get(i), pid, name, type);
    }
}

void BatteryStatsImpl::noteStopWakeFromSourceLocked(const sp<WorkSource>& ws, int32_t pid, sp<String>& name, int32_t type) {
    GLOGENTRY();

    int32_t N = ws->size();

    for (int32_t i = 0; i < N; i++) {
        noteStopWakeLocked(ws->get(i), pid, name, type);
    }
}

int32_t BatteryStatsImpl::startAddingCpuLocked() {
    GLOGENTRY();

    mHandler->removeMessages(MSG_UPDATE_WAKELOCKS);

    if (mScreenOn) {
        return 0;
    }

    const int32_t N = mPartialTimers->size();

    if (N == 0) {
        mLastPartialTimers->clear();
        return 0;
    }

    // How many timers should consume CPU?  Only want to include ones
    // that have already been in the list.

    for (int32_t i = 0; i < N; i++) {
         sp<StopwatchTimer> st = mPartialTimers->get(i);
         if (st->mInList) {
             sp<BatteryStatsImpl::Uid> uid = (st->mUid).promote();
             NULL_RTN_VAL(uid, 0, "Promotion to sp<Uid> fails");
             // We don't include the system UID, because it so often
             // holds wake locks at one request or another of an app.
             if (uid != NULL && uid->mUid != Process::SYSTEM_UID) {
                 return 50;
             }
         }
     }

    return 0;
}

void BatteryStatsImpl::finishAddingCpuLocked(int32_t perc, int32_t utime, int32_t stime, Blob<int64_t> cpuSpeedTimes) {
    GLOGENTRY();

    const int32_t N = mPartialTimers->size();

    if (perc != 0) {
        int32_t num = 0;

        for (int32_t i = 0; i < N; i++) {
            sp<StopwatchTimer> st = mPartialTimers->get(i);
            if (st->mInList) {
                sp<BatteryStatsImpl::Uid> uid = (st->mUid).promote();
                NULL_RTN(uid, "Promotion to sp<Uid> fails");
                // We don't include the system UID, because it so often
                // holds wake locks at one request or another of an app.
                if (uid != NULL && uid->mUid != Process::SYSTEM_UID) {
                    num++;
                }
            }
        }

        if (num != 0) {
            for (int32_t i = 0; i < N; i++) {
                sp<StopwatchTimer> st = mPartialTimers->get(i);
                if (st->mInList) {
                    sp<BatteryStatsImpl::Uid> uid = (st->mUid).promote();
                    NULL_RTN(uid, "Promotion to sp<BatteryStatsImpl::Uid> fails");

                    if (uid != NULL && uid->mUid != Process::SYSTEM_UID) {
                        int32_t myUTime = utime / num;
                        int32_t mySTime = stime / num;
                        utime -= myUTime;
                        stime -= mySTime;
                        num--;
                        sp<BatteryStatsImpl::Uid::Proc> proc = safe_cast<BatteryStatsImpl::Uid::Proc*>(uid->getProcessStatsLocked(new String("*wakelock*")));
                        proc->addCpuTimeLocked(myUTime, mySTime);
                        proc->addSpeedStepTimes(cpuSpeedTimes);
                    }
                }
            }
        }

        // Just in case, collect any lost CPU time.
        if (utime != 0 || stime != 0) {
            sp<Uid> uid = getUidStatsLocked(Process::SYSTEM_UID);

            if (uid != NULL) {
                sp<Uid::Proc> proc = safe_cast<BatteryStatsImpl::Uid::Proc*>(uid->getProcessStatsLocked(new String("*lost*")));
                proc->addCpuTimeLocked(utime, stime);
                proc->addSpeedStepTimes(cpuSpeedTimes);
            }
        }
    }

    const int32_t NL = mLastPartialTimers->size();
    bool diff = N != NL;

    for (int32_t i = 0; i < NL && !diff; i++) {
        diff |= mPartialTimers->get(i) != mLastPartialTimers->get(i);
    }

    if (!diff) {
        for (int32_t i = 0; i < NL; i++) {
            mPartialTimers->get(i)->mInList = true;
        }
        return;
    }

    for (int32_t i = 0; i < NL; i++) {
        mLastPartialTimers->get(i)->mInList = false;
    }

    mLastPartialTimers->clear();

    for (int32_t i = 0; i < N; i++) {
        sp<StopwatchTimer> st = mPartialTimers->get(i);
        st->mInList = true;
        mLastPartialTimers->add(st);
    }
}

void BatteryStatsImpl::noteProcessDiedLocked(int32_t uid, int32_t pid) {
    GLOGENTRY();

    sp<Uid> u;

    {
        GLOGAUTOMUTEX(_l, mThisLock);

        int32_t index = mUidStats.indexOfKey(uid);
        if (index >= 0) {
            u = mUidStats.valueAt(index);
        }
    }

    if (u != NULL) {
        u->mPids.removeItem(pid);
    }
}

int64_t BatteryStatsImpl::getProcessWakeTime(int32_t uid, int32_t pid, int64_t realtime) {
    GLOGENTRY();

    sp<Uid> u;

    {
        GLOGAUTOMUTEX(_l, mThisLock);

        int32_t index = mUidStats.indexOfKey(uid);
        if (index >= 0) {
            u = mUidStats.valueAt(index);
        }
    }

    if (u != NULL) {
        sp<Uid::Pid> p;
        int32_t index = u->mPids.indexOfKey(pid);
        if (index >= 0) {
            p = u->mPids.valueAt(index);
        }

        if (p != NULL) {
            return p->mWakeSum + (p->mWakeStart != 0 ? (realtime - p->mWakeStart) : 0);
        }
    }

    return 0;
}

void BatteryStatsImpl::reportExcessiveWakeLocked(int32_t uid, const sp<String>& proc, int64_t overTime, int64_t usedTime) {
    GLOGENTRY();

    sp<Uid> u;

    {
        GLOGAUTOMUTEX(_l, mThisLock);

        int32_t index = mUidStats.indexOfKey(uid);

        if (index >= 0) {
            u = mUidStats.valueAt(index);
        }
    }

    if (u != NULL) {
        u->reportExcessiveWakeLocked(proc, overTime, usedTime);
    }
}

void BatteryStatsImpl::reportExcessiveCpuLocked(int32_t uid, const sp<String>& proc, int64_t overTime, int64_t usedTime) {
    GLOGENTRY();

    sp<Uid> u;

    {
        GLOGAUTOMUTEX(_l, mThisLock);

        int32_t index = mUidStats.indexOfKey(uid);

        if (index >= 0) {
            u = mUidStats.valueAt(index);
        }
    }

    if (u != NULL) {
        u->reportExcessiveCpuLocked(proc, overTime, usedTime);
    }
}

#ifdef SENSOR_ENABLE
void BatteryStatsImpl::noteStartSensorLocked(int32_t uid, int32_t sensor) {
    GLOGENTRY();

    if (mSensorNesting == 0) {
        mHistoryCur->states |= HistoryItem::STATE_SENSOR_ON_FLAG;

        if (DEBUG_HISTORY) {
            GLOGV(LOG_TAG "Start sensor to: %s", Integer::toHexString(mHistoryCur->states)->string());
        }

        addHistoryRecordLocked(elapsedRealtime());
    }

    mSensorNesting++;
    getUidStatsLocked(uid)->noteStartSensor(sensor);
}

void BatteryStatsImpl::noteStopSensorLocked(int32_t uid, int32_t sensor) {
    GLOGENTRY();

    mSensorNesting--;

    if (mSensorNesting == 0) {
        mHistoryCur->states &= ~HistoryItem::STATE_SENSOR_ON_FLAG;

        if (DEBUG_HISTORY) {
            GLOGV("%s Stop sensor to: %s", LOG_TAG, Integer::toHexString(mHistoryCur->states)->string());
        }

        addHistoryRecordLocked(elapsedRealtime());
    }

    getUidStatsLocked(uid)->noteStopSensor(sensor);
}
#endif  // SENSOR_ENABLE

#ifdef GPS_ENABLE
void BatteryStatsImpl::noteStartGpsLocked(int32_t uid) {
    GLOGENTRY();

    if (mGpsNesting == 0) {
        mHistoryCur->states |= HistoryItem::STATE_GPS_ON_FLAG;

        if (DEBUG_HISTORY) {
            GLOGV("%s Start GPS to: %s", LOG_TAG, Integer::toHexString(mHistoryCur->states)->string());
        }

        addHistoryRecordLocked(elapsedRealtime());
    }

    mGpsNesting++;
    getUidStatsLocked(uid)->noteStartGps();
}

void BatteryStatsImpl::noteStopGpsLocked(int32_t uid) {
    GLOGENTRY();

    mGpsNesting--;

    if (mGpsNesting == 0) {
        mHistoryCur->states &= ~HistoryItem::STATE_GPS_ON_FLAG;

        if (DEBUG_HISTORY) {
            GLOGV("%s Stop GPS to: %s", LOG_TAG, Integer::toHexString(mHistoryCur->states)->string());
        }

        addHistoryRecordLocked(elapsedRealtime());
    }

    getUidStatsLocked(uid)->noteStopGps();
}
#endif  // GPS_ENABLE

void BatteryStatsImpl::noteScreenOnLocked() {
    GLOGENTRY();

    if (!mScreenOn) {
        mHistoryCur->states |= HistoryItem::STATE_SCREEN_ON_FLAG;

        if (DEBUG_HISTORY) {
            GLOGV("%s Screen on to: %s", LOG_TAG, Integer::toHexString(mHistoryCur->states)->string());
        }

        addHistoryRecordLocked(elapsedRealtime());
        mScreenOn = true;
        mScreenOnTimer->startRunningLocked(this);

#ifdef BACKLIGHT_ENABLE
        if (mScreenBrightnessBin >= 0) {
            (*mScreenBrightnessTimer)[mScreenBrightnessBin]->startRunningLocked(this);
        }
#endif  // BACKLIGHT_ENABLE

        // Fake a wake lock, so we consider the device waked as long
        // as the screen is on.
        noteStartWakeLocked(-1, -1, new String("dummy"), BatteryStats::WAKE_TYPE_PARTIAL);

        // Update discharge amounts.
        if (mOnBatteryInternal) {
            updateDischargeScreenLevelsLocked(false, true);
        }
    }

    // Customize +++
    if (mDisplayUid >= 0) {
        sp<Uid> u;

        {
            GLOGAUTOMUTEX(_l, mThisLock);

            int32_t index = mUidStats.indexOfKey(mDisplayUid);

            if (index >= 0) {
                u = mUidStats.valueAt(index);
            }
        }

        if (u != NULL) {
            u->noteDisplayTurnedOnLocked();
        }
    }
    // Customize ---
}

void BatteryStatsImpl::noteScreenOffLocked() {
    GLOGENTRY();

    if (mScreenOn) {
        mHistoryCur->states &= ~HistoryItem::STATE_SCREEN_ON_FLAG;

        if (DEBUG_HISTORY) {
            GLOGV("%s Screen off to: %s", LOG_TAG, Integer::toHexString(mHistoryCur->states)->string());
        }

        addHistoryRecordLocked(elapsedRealtime());
        mScreenOn = false;
        mScreenOnTimer->stopRunningLocked(this);

#ifdef BACKLIGHT_ENABLE
        if (mScreenBrightnessBin >= 0) {
            (*mScreenBrightnessTimer)[mScreenBrightnessBin]->stopRunningLocked(this);
        }
#endif  // BACKLIGHT_ENABLE

        noteStopWakeLocked(-1, -1, new String("dummy"), BatteryStats::WAKE_TYPE_PARTIAL);

        // Update discharge amounts.
        if (mOnBatteryInternal) {
            updateDischargeScreenLevelsLocked(true, false);
        }
    }

    // Customize +++
    if (mDisplayUid >= 0) {
        sp<Uid> u;

        {
            GLOGAUTOMUTEX(_l, mThisLock);

            int32_t index = mUidStats.indexOfKey(mDisplayUid);

            if (index >= 0) {
                u = mUidStats.valueAt(index);
            }
        }

        if (u != NULL) {
            u->noteDisplayTurnedOffLocked();
        }
    }
    // Customize ---
}

#ifdef BACKLIGHT_ENABLE
void BatteryStatsImpl::noteScreenBrightnessLocked(int32_t brightness) {
    GLOGENTRY();

    // Bin the brightness.
    int32_t bin = brightness / (256 / NUM_SCREEN_BRIGHTNESS_BINS);

    if (bin < 0) bin = 0;
    else if (bin >= NUM_SCREEN_BRIGHTNESS_BINS) bin = NUM_SCREEN_BRIGHTNESS_BINS - 1;

    if (mScreenBrightnessBin != bin) {
        mHistoryCur->states = (mHistoryCur->states&~HistoryItem::STATE_BRIGHTNESS_MASK)
                             | (bin << HistoryItem::STATE_BRIGHTNESS_SHIFT);

        if (DEBUG_HISTORY) {
            GLOGV(LOG_TAG "Screen brightness  %d to: %s", bin, Integer::toHexString(mHistoryCur->states)->string());
        }

        addHistoryRecordLocked(elapsedRealtime());

        if (mScreenOn) {
            if (mScreenBrightnessBin >= 0) {
                (*mScreenBrightnessTimer)[mScreenBrightnessBin]->stopRunningLocked(this);
            }

            (*mScreenBrightnessTimer)[bin]->startRunningLocked(this);
        }

        // Customize +++
        if (mDisplayUid >= 0) {
            sp<Uid> u;

            {
                GLOGAUTOMUTEX(_l, mThisLock);

                int32_t index = mUidStats.indexOfKey(mDisplayUid);

                if (index >= 0) {
                     u = mUidStats.valueAt(index);
                }
            }

            if (u != NULL) {
                u->noteDisplayBrightnessLocked(bin);
            }
        }
        // Customize ---

        mScreenBrightnessBin = bin;
    }
}
#endif  // BACKLIGHT_ENABLE

void BatteryStatsImpl::noteInputEventAtomic() {
    GLOGENTRY();

    mInputEventCounter->stepAtomic();
}

void BatteryStatsImpl::noteUserActivityLocked(int32_t uid, int32_t event) {
    GLOGENTRY();

    getUidStatsLocked(uid)->noteUserActivityLocked(event);
}

void BatteryStatsImpl::notePhoneOnLocked() {
    GLOGENTRY();

    if (!mPhoneOn) {
        mHistoryCur->states |= HistoryItem::STATE_PHONE_IN_CALL_FLAG;

        if (DEBUG_HISTORY) {
            GLOGV(LOG_TAG "Phone on to: %s", Integer::toHexString(mHistoryCur->states)->string());
        }

        addHistoryRecordLocked(elapsedRealtime());
        mPhoneOn = true;
        mPhoneOnTimer->startRunningLocked(this);
    }
}

void BatteryStatsImpl::notePhoneOffLocked() {
    GLOGENTRY();

    if (mPhoneOn) {
        mHistoryCur->states &= ~HistoryItem::STATE_PHONE_IN_CALL_FLAG;

        if (DEBUG_HISTORY) {
            GLOGV("%s Phone off to: %s", LOG_TAG , Integer::toHexString(mHistoryCur->states)->string());
        }

        addHistoryRecordLocked(elapsedRealtime());
        mPhoneOn = false;
        mPhoneOnTimer->stopRunningLocked(this);
    }
}

void BatteryStatsImpl::stopAllSignalStrengthTimersLocked(int32_t except) {
    GLOGENTRY();

    for (int32_t i = 0; i < SignalStrength::NUM_SIGNAL_STRENGTH_BINS; i++) {
        if (i == except) {
            continue;
        }

        while ((*mPhoneSignalStrengthsTimer)[i]->isRunningLocked()) {
            (*mPhoneSignalStrengthsTimer)[i]->stopRunningLocked(this);
        }
    }
}

int32_t BatteryStatsImpl::fixPhoneServiceState(int32_t state, int32_t signalBin) {
    GLOGENTRY();

    if (mPhoneSimStateRaw == TelephonyManager::SIM_STATE_ABSENT) {
        // In this case we will always be STATE_OUT_OF_SERVICE, so need
        // to infer that we are scanning from other data.
        if (state == ServiceState::STATE_OUT_OF_SERVICE
                && signalBin > SignalStrength::SIGNAL_STRENGTH_NONE_OR_UNKNOWN) {
            state = ServiceState::STATE_IN_SERVICE;
        }
    }

    return state;
}

void BatteryStatsImpl::updateAllPhoneStateLocked(int32_t state, int32_t simState, int32_t bin) {
    GLOGENTRY();

    bool scanning = false;
    bool newHistory = false;
    mPhoneServiceStateRaw = state;
    mPhoneSimStateRaw = simState;
    mPhoneSignalStrengthBinRaw = bin;

    // Customize +++
    if (DEBUG_ON) {
        GLOGD("update State Locked state = %d, simState = %d, bin = %d", state, simState, bin);
    }
    // Customize ---

    if (simState == TelephonyManager::SIM_STATE_ABSENT) {
        // In this case we will always be STATE_OUT_OF_SERVICE, so need
        // to infer that we are scanning from other data.
        if (state == ServiceState::STATE_OUT_OF_SERVICE
                && bin > SignalStrength::SIGNAL_STRENGTH_NONE_OR_UNKNOWN) {
            state = ServiceState::STATE_IN_SERVICE;
        }
    }

    // If the phone is powered off, stop all timers.
    if (state == ServiceState::STATE_POWER_OFF) {
        bin = -1;
        // If we are in service, make sure the correct signal string timer is running.
    } else if (state == ServiceState::STATE_IN_SERVICE) {
        // Bin will be changed below.
        // If we're out of service, we are in the lowest signal strength
        // bin and have the scanning bit set.
    } else if (state == ServiceState::STATE_OUT_OF_SERVICE) {
        scanning = true;
        bin = SignalStrength::SIGNAL_STRENGTH_NONE_OR_UNKNOWN;

        if (!mPhoneSignalScanningTimer->isRunningLocked()) {
            mHistoryCur->states |= HistoryItem::STATE_PHONE_SCANNING_FLAG;
            newHistory = true;

            if (DEBUG_HISTORY) {
                GLOGV(LOG_TAG "Phone started scanning to: %s", Integer::toHexString(mHistoryCur->states)->string());
            }

            mPhoneSignalScanningTimer->startRunningLocked(this);
        }
    }

    if (!scanning) {
        // If we are no longer scanning, then stop the scanning timer.
        if (mPhoneSignalScanningTimer->isRunningLocked()) {
            mHistoryCur->states &= ~HistoryItem::STATE_PHONE_SCANNING_FLAG;

            if (DEBUG_HISTORY) {
                GLOGV(LOG_TAG "Phone stopped scanning to: %s", Integer::toHexString(mHistoryCur->states)->string());
            }

            newHistory = true;
            mPhoneSignalScanningTimer->stopRunningLocked(this);
        }
    }

    if (mPhoneServiceState != state) {
        mHistoryCur->states = (mHistoryCur->states&~HistoryItem::STATE_PHONE_STATE_MASK)
                             | (state << HistoryItem::STATE_PHONE_STATE_SHIFT);

        if (DEBUG_HISTORY) {
            GLOGV(LOG_TAG "Phone state  %d  to: %s", state, Integer::toHexString(mHistoryCur->states)->string());
        }

        newHistory = true;
        mPhoneServiceState = state;
    }

    if (mPhoneSignalStrengthBin != bin) {
        if (mPhoneSignalStrengthBin >= 0) {
            (*mPhoneSignalStrengthsTimer)[mPhoneSignalStrengthBin]->stopRunningLocked(this);
        }

        if (bin >= 0) {
            if (!(*mPhoneSignalStrengthsTimer)[bin]->isRunningLocked()) {
                (*mPhoneSignalStrengthsTimer)[bin]->startRunningLocked(this);
            }

            mHistoryCur->states = (mHistoryCur->states&~HistoryItem::STATE_SIGNAL_STRENGTH_MASK)
                                 | (bin << HistoryItem::STATE_SIGNAL_STRENGTH_SHIFT);

            if (DEBUG_HISTORY) {
                GLOGV(LOG_TAG "Signal strength %d to: %s ", bin, Integer::toHexString(mHistoryCur->states)->string());
            }

            newHistory = true;
        } else {
            stopAllSignalStrengthTimersLocked(-1);
        }

        mPhoneSignalStrengthBin = bin;
    }

    if (newHistory) {
        addHistoryRecordLocked(elapsedRealtime());
    }
}

void BatteryStatsImpl::notePhoneStateLocked(int32_t state, int32_t simState) {
    GLOGENTRY();

    updateAllPhoneStateLocked(state, simState, mPhoneSignalStrengthBinRaw);
}

void BatteryStatsImpl::notePhoneSignalStrengthLocked(const sp<SignalStrength>& signalStrength) {
    GLOGENTRY();

    // Bin the strength.
    int32_t bin = signalStrength->getLevel();
    updateAllPhoneStateLocked(mPhoneServiceStateRaw, mPhoneSimStateRaw, bin);
}

void BatteryStatsImpl::notePhoneDataConnectionStateLocked(int32_t dataType, bool hasData) {
    GLOGENTRY();

    int32_t bin = DATA_CONNECTION_NONE;

    if (hasData) {
        switch (dataType) {
            case TelephonyManager::NETWORK_TYPE_EDGE:
                bin = DATA_CONNECTION_EDGE;
                break;
            case TelephonyManager::NETWORK_TYPE_GPRS:
                bin = DATA_CONNECTION_GPRS;
                break;
            case TelephonyManager::NETWORK_TYPE_UMTS:
                bin = DATA_CONNECTION_UMTS;
                break;
            case TelephonyManager::NETWORK_TYPE_CDMA:
                bin = DATA_CONNECTION_CDMA;
                break;
            case TelephonyManager::NETWORK_TYPE_EVDO_0:
                bin = DATA_CONNECTION_EVDO_0;
                break;
            case TelephonyManager::NETWORK_TYPE_EVDO_A:
                bin = DATA_CONNECTION_EVDO_A;
                break;
            case TelephonyManager::NETWORK_TYPE_1xRTT:
                bin = DATA_CONNECTION_1xRTT;
                break;
            case TelephonyManager::NETWORK_TYPE_HSDPA:
                bin = DATA_CONNECTION_HSDPA;
                break;
            case TelephonyManager::NETWORK_TYPE_HSUPA:
                bin = DATA_CONNECTION_HSUPA;
                break;
            case TelephonyManager::NETWORK_TYPE_HSPA:
                bin = DATA_CONNECTION_HSPA;
                break;
            case TelephonyManager::NETWORK_TYPE_IDEN:
                bin = DATA_CONNECTION_IDEN;
                break;
            case TelephonyManager::NETWORK_TYPE_EVDO_B:
                bin = DATA_CONNECTION_EVDO_B;
                break;
            case TelephonyManager::NETWORK_TYPE_LTE:
                bin = DATA_CONNECTION_LTE;
                break;
            case TelephonyManager::NETWORK_TYPE_EHRPD:
                bin = DATA_CONNECTION_EHRPD;
                break;
            default:
                bin = DATA_CONNECTION_OTHER;
                break;
        }
    }

    if (DEBUG) {
        GLOGI(LOG_TAG "Phone Data Connection ->  %d  = %d", dataType, hasData);
    }

    if (mPhoneDataConnectionType != bin) {
        mHistoryCur->states = (mHistoryCur->states&~HistoryItem::STATE_DATA_CONNECTION_MASK)
                             | (bin << HistoryItem::STATE_DATA_CONNECTION_SHIFT);

        if (DEBUG_HISTORY) {
            GLOGV(LOG_TAG "Data connection %d to: %s", bin, Integer::toHexString(mHistoryCur->states)->string());
        }

        addHistoryRecordLocked(elapsedRealtime());

        if (mPhoneDataConnectionType >= 0) {
            (*mPhoneDataConnectionsTimer)[mPhoneDataConnectionType]->stopRunningLocked(this);
        }

        mPhoneDataConnectionType = bin;
        (*mPhoneDataConnectionsTimer)[bin]->startRunningLocked(this);
    }
}

#ifdef WIFI_ENABLE
void BatteryStatsImpl::noteWifiOnLocked() {
    GLOGENTRY();

    if (!mWifiOn) {
        mHistoryCur->states |= HistoryItem::STATE_WIFI_ON_FLAG;

        if (DEBUG_HISTORY) {
            GLOGV(LOG_TAG "WIFI on to: %s", Integer::toHexString(mHistoryCur->states)->string());
        }

        addHistoryRecordLocked(elapsedRealtime());
        mWifiOn = true;
        mWifiOnTimer->startRunningLocked(this);
    }
}

void BatteryStatsImpl::noteWifiOffLocked() {
    GLOGENTRY();

    if (mWifiOn) {
        mHistoryCur->states &= ~HistoryItem::STATE_WIFI_ON_FLAG;

        if (DEBUG_HISTORY) {
            GLOGV(LOG_TAG "WIFI off to: %s", Integer::toHexString(mHistoryCur->states)->string());
        }

        addHistoryRecordLocked(elapsedRealtime());
        mWifiOn = false;
        mWifiOnTimer->stopRunningLocked(this);
    }

    if (mWifiOnUid >= 0) {
        getUidStatsLocked(mWifiOnUid)->noteWifiStoppedLocked();
        mWifiOnUid = -1;
    }
}
#endif  // WIFI_ENABLE

void BatteryStatsImpl::noteAudioOnLocked(int32_t uid) {
    GLOGENTRY();

    if (!mAudioOn) {
        mHistoryCur->states |= HistoryItem::STATE_AUDIO_ON_FLAG;

        if (DEBUG_HISTORY) {
            GLOGV(LOG_TAG  "Audio on to: %s", Integer::toHexString(mHistoryCur->states)->string());
        }

        addHistoryRecordLocked(elapsedRealtime());
        mAudioOn = true;
        mAudioOnTimer->startRunningLocked(this);
    }

    getUidStatsLocked(uid)->noteAudioTurnedOnLocked();
}

void BatteryStatsImpl::noteAudioOffLocked(int32_t uid) {
    GLOGENTRY();

    if (mAudioOn) {
        mHistoryCur->states &= ~HistoryItem::STATE_AUDIO_ON_FLAG;

        if (DEBUG_HISTORY) {
            GLOGV(LOG_TAG "Audio off to: %s", Integer::toHexString(mHistoryCur->states)->string());
        }

        addHistoryRecordLocked(elapsedRealtime());
        mAudioOn = false;
        mAudioOnTimer->stopRunningLocked(this);
    }

    getUidStatsLocked(uid)->noteAudioTurnedOffLocked();
}

void BatteryStatsImpl::noteVideoOnLocked(int32_t uid) {
    GLOGENTRY();

    if (!mVideoOn) {
        mHistoryCur->states |= HistoryItem::STATE_VIDEO_ON_FLAG;

        if (DEBUG_HISTORY) {
            GLOGV(LOG_TAG "Video on to: %s", Integer::toHexString(mHistoryCur->states)->string());
        }

        addHistoryRecordLocked(elapsedRealtime());
        mVideoOn = true;
        mVideoOnTimer->startRunningLocked(this);
    }

    getUidStatsLocked(uid)->noteVideoTurnedOnLocked();
}

void BatteryStatsImpl::noteVideoOffLocked(int32_t uid) {
    GLOGENTRY();

    if (mVideoOn) {
        mHistoryCur->states &= ~HistoryItem::STATE_VIDEO_ON_FLAG;

        if (DEBUG_HISTORY) {
            GLOGV(LOG_TAG "Video off to: %s", Integer::toHexString(mHistoryCur->states)->string());
        }

        addHistoryRecordLocked(elapsedRealtime());
        mVideoOn = false;
        mVideoOnTimer->stopRunningLocked(this);
    }

    getUidStatsLocked(uid)->noteVideoTurnedOffLocked();
}

//  Customize +++
void BatteryStatsImpl::noteGpuOnLocked(int32_t uid) {
    GLOGENTRY();

    if (DEBUG_GPU) {
        GLOGD("noteGpuOnLocked uid = %d", uid);
    }

    getUidStatsLocked(uid)->noteGpuTurnedOnLocked();
}
//  Customize ---

//  Customize +++
void BatteryStatsImpl::noteGpuOffLocked(int32_t uid) {
    GLOGENTRY();

    if (DEBUG_GPU) {
        GLOGD("noteGpuOffLocked uid = %d", uid);
    }

    getUidStatsLocked(uid)->noteGpuTurnedOffLocked();
}
//  Customize ---

//  Customize +++
void BatteryStatsImpl::noteDisplayOnLocked(int32_t uid) {
    GLOGENTRY();

    mDisplayUid = uid;
    getUidStatsLocked(uid)->noteDisplayTurnedOnLocked();
}
//  Customize ---

//  Customize +++
void BatteryStatsImpl::noteDisplayOffLocked(int32_t uid) {
    GLOGENTRY();

    getUidStatsLocked(uid)->noteDisplayTurnedOffLocked();
    mDisplayUid = -1;
}
//  Customize ---

#ifdef WIFI_ENABLE
void BatteryStatsImpl::noteWifiRunningLocked(const sp<WorkSource>& ws) {
    GLOGENTRY();

    if (!mGlobalWifiRunning) {
        mHistoryCur->states |= HistoryItem::STATE_WIFI_RUNNING_FLAG;

        if (DEBUG_HISTORY) {
            GLOGV(LOG_TAG "WIFI running to: %s", Integer::toHexString(mHistoryCur->states)->string());
        }

        addHistoryRecordLocked(elapsedRealtime());
        mGlobalWifiRunning = true;
        mGlobalWifiRunningTimer->startRunningLocked(this);
        int32_t N = ws->size();

        for (int32_t i = 0; i < N; i++) {
            getUidStatsLocked(ws->get(i))->noteWifiRunningLocked();
        }
    } else {
        GLOGW("%s noteWifiRunningLocked -- called while WIFI running", LOG_TAG);
    }
}


void BatteryStatsImpl::noteWifiRunningChangedLocked(const sp<WorkSource>& oldWs, const sp<WorkSource>& newWs) {
    GLOGENTRY();

    if (mGlobalWifiRunning) {
        int32_t N = oldWs->size();

        for (int32_t i = 0; i < N; i++) {
            getUidStatsLocked(oldWs->get(i))->noteWifiStoppedLocked();
        }

        N = newWs->size();

        for (int32_t i = 0; i < N; i++) {
            getUidStatsLocked(newWs->get(i))->noteWifiRunningLocked();
        }
    } else {
        GLOGW("%s noteWifiRunningChangedLocked -- called while WIFI not running", LOG_TAG);
    }
}

void BatteryStatsImpl::noteWifiStoppedLocked(const sp<WorkSource>& ws) {
    GLOGENTRY();

    if (mGlobalWifiRunning) {
        mHistoryCur->states &= ~HistoryItem::STATE_WIFI_RUNNING_FLAG;

        if (DEBUG_HISTORY) {
            GLOGV(LOG_TAG "WIFI stopped to: %s", Integer::toHexString(mHistoryCur->states)->string());
        }

        addHistoryRecordLocked(elapsedRealtime());
        mGlobalWifiRunning = false;
        mGlobalWifiRunningTimer->stopRunningLocked(this);
        int32_t N = ws->size();

        for (int32_t i = 0; i < N; i++) {
            getUidStatsLocked(ws->get(i))->noteWifiStoppedLocked();
        }
    } else {
        GLOGW(LOG_TAG "noteWifiStoppedLocked -- called while WIFI not running");
    }
}
#endif  // WIFI_ENABLE

void BatteryStatsImpl::noteBluetoothOnLocked() {
    GLOGENTRY();

    if (!mBluetoothOn) {
        mHistoryCur->states |= HistoryItem::STATE_BLUETOOTH_ON_FLAG;

        if (DEBUG_HISTORY) {
            GLOGV(LOG_TAG "Bluetooth on to: %s", Integer::toHexString(mHistoryCur->states)->string());
        }

        addHistoryRecordLocked(elapsedRealtime());
        mBluetoothOn = true;
        mBluetoothOnTimer->startRunningLocked(this);
    }
}

void BatteryStatsImpl::noteBluetoothOffLocked() {
    GLOGENTRY();

    if (mBluetoothOn) {
        mHistoryCur->states &= ~HistoryItem::STATE_BLUETOOTH_ON_FLAG;

        if (DEBUG_HISTORY) {
            GLOGV(LOG_TAG "Bluetooth off to: %s", Integer::toHexString(mHistoryCur->states)->string());
        }

        addHistoryRecordLocked(elapsedRealtime());
        mBluetoothOn = false;
        mBluetoothOnTimer->stopRunningLocked(this);
    }
}

#ifdef WIFI_ENABLE
void BatteryStatsImpl::noteFullWifiLockAcquiredLocked(int32_t uid) {
    GLOGENTRY();

    if (mWifiFullLockNesting == 0) {
        mHistoryCur->states |= HistoryItem::STATE_WIFI_FULL_LOCK_FLAG;

        if (DEBUG_HISTORY) {
            GLOGV(LOG_TAG "WIFI full lock on to: %s", Integer::toHexString(mHistoryCur->states)->string());
        }

        addHistoryRecordLocked(elapsedRealtime());
    }

    mWifiFullLockNesting++;
    getUidStatsLocked(uid)->noteFullWifiLockAcquiredLocked();
}

void BatteryStatsImpl::noteFullWifiLockReleasedLocked(int32_t uid) {
    GLOGENTRY();

    mWifiFullLockNesting--;

    if (mWifiFullLockNesting == 0) {
        mHistoryCur->states &= ~HistoryItem::STATE_WIFI_FULL_LOCK_FLAG;

        if (DEBUG_HISTORY) {
            GLOGV(LOG_TAG "WIFI full lock off to: %s", Integer::toHexString(mHistoryCur->states)->string());
        }

        addHistoryRecordLocked(elapsedRealtime());
    }

    getUidStatsLocked(uid)->noteFullWifiLockReleasedLocked();
}

void BatteryStatsImpl::noteScanWifiLockAcquiredLocked(int32_t uid) {
    GLOGENTRY();

    if (mWifiScanLockNesting == 0) {
        mHistoryCur->states |= HistoryItem::STATE_WIFI_SCAN_LOCK_FLAG;

        if (DEBUG_HISTORY) {
            GLOGV(LOG_TAG "WIFI scan lock on to: %s", Integer::toHexString(mHistoryCur->states)->string());
        }

        addHistoryRecordLocked(elapsedRealtime());
    }

    mWifiScanLockNesting++;
    getUidStatsLocked(uid)->noteScanWifiLockAcquiredLocked();
}

void BatteryStatsImpl::noteScanWifiLockReleasedLocked(int32_t uid) {
    GLOGENTRY();

    mWifiScanLockNesting--;

    if (mWifiScanLockNesting == 0) {
        mHistoryCur->states &= ~HistoryItem::STATE_WIFI_SCAN_LOCK_FLAG;

        if (DEBUG_HISTORY) {
            GLOGV(LOG_TAG "WIFI scan lock off to: %s", Integer::toHexString(mHistoryCur->states)->string());
        }

        addHistoryRecordLocked(elapsedRealtime());
    }

    getUidStatsLocked(uid)->noteScanWifiLockReleasedLocked();
}

void BatteryStatsImpl::noteWifiMulticastEnabledLocked(int32_t uid) {
    GLOGENTRY();

    if (mWifiMulticastNesting == 0) {
        mHistoryCur->states |= HistoryItem::STATE_WIFI_MULTICAST_ON_FLAG;

        if (DEBUG_HISTORY) {
            GLOGV(LOG_TAG "WIFI multicast on to: %s", Integer::toHexString(mHistoryCur->states)->string());
        }

        addHistoryRecordLocked(elapsedRealtime());
    }

    mWifiMulticastNesting++;
    getUidStatsLocked(uid)->noteWifiMulticastEnabledLocked();
}

void BatteryStatsImpl::noteWifiMulticastDisabledLocked(int32_t uid) {
    GLOGENTRY();

    mWifiMulticastNesting--;

    if (mWifiMulticastNesting == 0) {
        mHistoryCur->states &= ~HistoryItem::STATE_WIFI_MULTICAST_ON_FLAG;

        if (DEBUG_HISTORY) {
            GLOGV(LOG_TAG "WIFI multicast off to: %s", Integer::toHexString(mHistoryCur->states)->string());
        }

        addHistoryRecordLocked(elapsedRealtime());
    }

    getUidStatsLocked(uid)->noteWifiMulticastDisabledLocked();
}

void BatteryStatsImpl::noteFullWifiLockAcquiredFromSourceLocked(const sp<WorkSource>& ws) {
    GLOGENTRY();

    int32_t N = ws->size();

    for (int32_t i = 0; i < N; i++) {
        noteFullWifiLockAcquiredLocked(ws->get(i));
    }
}

void BatteryStatsImpl::noteFullWifiLockReleasedFromSourceLocked(const sp<WorkSource>& ws) {
    GLOGENTRY();

    int32_t N = ws->size();

    for (int32_t i = 0; i < N; i++) {
        noteFullWifiLockReleasedLocked(ws->get(i));
    }
}

void BatteryStatsImpl::noteScanWifiLockAcquiredFromSourceLocked(const sp<WorkSource>& ws) {
    GLOGENTRY();

    int32_t N = ws->size();

    for (int32_t i = 0; i < N; i++) {
        noteScanWifiLockAcquiredLocked(ws->get(i));
    }
}

void BatteryStatsImpl::noteScanWifiLockReleasedFromSourceLocked(const sp<WorkSource>& ws) {
    GLOGENTRY();

    int32_t N = ws->size();

    for (int32_t i = 0; i < N; i++) {
        noteScanWifiLockReleasedLocked(ws->get(i));
    }
}

void BatteryStatsImpl::noteWifiMulticastEnabledFromSourceLocked(const sp<WorkSource>& ws) {
    GLOGENTRY();

    int32_t N = ws->size();

    for (int32_t i = 0; i < N; i++) {
        noteWifiMulticastEnabledLocked(ws->get(i));
    }
}

void BatteryStatsImpl::noteWifiMulticastDisabledFromSourceLocked(const sp<WorkSource>& ws) {
    GLOGENTRY();

    int32_t N = ws->size();

    for (int32_t i = 0; i < N; i++) {
        noteWifiMulticastDisabledLocked(ws->get(i));
    }
}
#endif  // WIFI_ENABLE

void BatteryStatsImpl::noteNetworkInterfaceTypeLocked(const sp<String>& iface, int32_t networkType) {
    GLOGENTRY();

    if (ConnectivityManager::isNetworkTypeMobile(networkType)) {
        mMobileIfaces->add(iface);
    } else {
        mMobileIfaces->remove(iface);
    }
}

int64_t BatteryStatsImpl::getScreenOnTime(int64_t batteryRealtime, int32_t which) {
    GLOGENTRY();

    return mScreenOnTimer->getTotalTimeLocked(batteryRealtime, which);
}

#ifdef BACKLIGHT_ENABLE
int64_t BatteryStatsImpl::getScreenBrightnessTime(int32_t brightnessBin, int64_t batteryRealtime, int32_t which) {
    GLOGENTRY();

    return (*mScreenBrightnessTimer)[brightnessBin]->getTotalTimeLocked(batteryRealtime, which);
}
#endif  // BACKLIGHT_ENABLE

int32_t BatteryStatsImpl::getInputEventCount(int32_t which) {
    GLOGENTRY();

    return mInputEventCounter->getCountLocked(which);
}

int64_t BatteryStatsImpl::getPhoneOnTime(int64_t batteryRealtime, int32_t which) {
    GLOGENTRY();

    return mPhoneOnTimer->getTotalTimeLocked(batteryRealtime, which);
}

int64_t BatteryStatsImpl::getPhoneSignalStrengthTime(int32_t strengthBin, int64_t batteryRealtime, int32_t which) {
    GLOGENTRY();

    return (*mPhoneSignalStrengthsTimer)[strengthBin]->getTotalTimeLocked(batteryRealtime, which);
}

int64_t BatteryStatsImpl::getPhoneSignalScanningTime(int64_t batteryRealtime, int32_t which) {
    GLOGENTRY();

    return mPhoneSignalScanningTimer->getTotalTimeLocked(batteryRealtime, which);
}

int32_t BatteryStatsImpl::getPhoneSignalStrengthCount(int32_t dataType, int32_t which) {
    GLOGENTRY();

    return (*mPhoneDataConnectionsTimer)[dataType]->getCountLocked(which);
}

int64_t BatteryStatsImpl::getPhoneDataConnectionTime(int32_t dataType, int64_t batteryRealtime, int32_t which) {
    GLOGENTRY();

    return (*mPhoneDataConnectionsTimer)[dataType]->getTotalTimeLocked(batteryRealtime, which);
}

int32_t BatteryStatsImpl::getPhoneDataConnectionCount(int32_t dataType, int32_t which) {
    GLOGENTRY();

    return (*mPhoneDataConnectionsTimer)[dataType]->getCountLocked(which);
}

#ifdef WIFI_ENABLE
int64_t BatteryStatsImpl::getWifiOnTime(int64_t batteryRealtime, int32_t which) {
    GLOGENTRY();

    return mWifiOnTimer->getTotalTimeLocked(batteryRealtime, which);
}

int64_t BatteryStatsImpl::getGlobalWifiRunningTime(int64_t batteryRealtime, int32_t which) {
    GLOGENTRY();

    return mGlobalWifiRunningTimer->getTotalTimeLocked(batteryRealtime, which);
}
#endif  // WIFI_ENABLE

int64_t BatteryStatsImpl::getBluetoothOnTime(int64_t batteryRealtime, int32_t which) {
    GLOGENTRY();

    return mBluetoothOnTimer->getTotalTimeLocked(batteryRealtime, which);
}

bool BatteryStatsImpl::getIsOnBattery() {
    GLOGENTRY();

    return mOnBattery;
}

KeyedVector<int32_t, sp<BatteryStats::Uid> > BatteryStatsImpl::getUidStats() {
    GLOGENTRY();

    KeyedVector<int32_t, sp<BatteryStats::Uid> > _tmpVector;
    _tmpVector.clear();

    {
        GLOGAUTOMUTEX(_l, mThisLock);

        for (uint32_t i = 0; i < mUidStats.size(); i++) {
            sp<BatteryStats::Uid> _uid = mUidStats.valueAt(i);  // upper cast, ok
            _tmpVector.add(mUidStats.keyAt(i), _uid);
        }
    }

    return _tmpVector;  // TODO +++ local varable
}

void BatteryStatsImpl::setCallback(const sp<BatteryCallback>& cb) {
    GLOGENTRY();

    mCallback = cb;
}

void BatteryStatsImpl::setNumSpeedSteps(int32_t steps) {
    GLOGENTRY();

    if (sNumSpeedSteps == 0) {
        sNumSpeedSteps = steps;
    }
}

// Customize +++
void BatteryStatsImpl::setNumGpuSpeedSteps(int32_t steps) {
    GLOGENTRY();

    if (DEBUG_ON) {
        GLOGD("set Gpu step num current = %d , new = %d", sNumGpuSpeedSteps, steps);
    }

    if (steps < 0) {
         return;
    }

    if (sNumGpuSpeedSteps == 0) {
        sNumGpuSpeedSteps = steps;
    }
}
// Customize ---

void BatteryStatsImpl::setRadioScanningTimeout(int64_t timeout) {
    GLOGENTRY();

    if (mPhoneSignalScanningTimer != NULL) {
        mPhoneSignalScanningTimer->setTimeout(timeout);
    }
}

bool BatteryStatsImpl::startIteratingHistoryLocked() {
    GLOGENTRY();


    if (DEBUG_HISTORY) {
        GLOGI(LOG_TAG "ITERATING: buff size= %d  pos= %d", mHistoryBuffer.dataSize(), mHistoryBuffer.dataPosition());
    }

    mHistoryBuffer.setDataPosition(0);
    mReadOverflow = false;
    mIteratingHistory = true;
    return mHistoryBuffer.dataSize() > 0;
}

bool BatteryStatsImpl::getNextHistoryLocked(const sp<HistoryItem>& out) {
    GLOGENTRY();

    const uint32_t pos = mHistoryBuffer.dataPosition();

    if (pos == 0) {
        out->clear();
    }

    bool end = pos >= mHistoryBuffer.dataSize();

    if (end) {
        return false;
    }

    out->readDelta(mHistoryBuffer);
    return true;
}

void BatteryStatsImpl::finishIteratingHistoryLocked() {
    GLOGENTRY();

    mIteratingHistory = false;
    mHistoryBuffer.setDataPosition(mHistoryBuffer.dataSize());
}

bool BatteryStatsImpl::startIteratingOldHistoryLocked() {
    GLOGENTRY();

    if (DEBUG_HISTORY) {
        GLOGI(LOG_TAG "ITERATING: buff size=%d pos=%d", mHistoryBuffer.dataSize(), mHistoryBuffer.dataPosition());
    }

    mHistoryBuffer.setDataPosition(0);
    mHistoryReadTmp->clear();
    mReadOverflow = false;
    mIteratingHistory = true;
    return (mHistoryIterator = mHistory) != NULL;
}

bool BatteryStatsImpl::getNextOldHistoryLocked(const sp<HistoryItem>& out) {
    GLOGENTRY();

    bool end = mHistoryBuffer.dataPosition() >= mHistoryBuffer.dataSize();

    if (!end) {
        mHistoryReadTmp->readDelta(mHistoryBuffer);
        mReadOverflow |= mHistoryReadTmp->cmd == HistoryItem::CMD_OVERFLOW;
    }

    sp<HistoryItem> cur = mHistoryIterator;

    if (cur == NULL) {
        if (!mReadOverflow && !end) {
            GLOGW(LOG_TAG "Old history ends before new history!");
        }

        return false;
    }

    out->setTo(cur);
    mHistoryIterator = cur->next;

    if (!mReadOverflow) {
        if (end) {
            GLOGW(LOG_TAG "New history ends before old history!");
        } else if (!out->same(mHistoryReadTmp)) {
            #if 0
            int64_t now = getHistoryBaseTime() + elapsedRealtime();
            sp<PrintWriter> pw = new PrintWriter(new LogWriter(LOG::WARN, LOG_TAG));
            pw->println("Histories differ!");
            pw->println("Old history:");
            (new HistoryPrinter())->printNextItem(pw, out, now);
            pw->println("New history:");
            (new HistoryPrinter())->printNextItem(pw, mHistoryReadTmp, now);
            #endif  // TODO  LogWriter
        }
    }

    return true;
}

void BatteryStatsImpl::finishIteratingOldHistoryLocked() {
    GLOGENTRY();

    mIteratingHistory = false;
    mHistoryBuffer.setDataPosition(mHistoryBuffer.dataSize());
}

int64_t BatteryStatsImpl::getHistoryBaseTime() {
    GLOGENTRY();

    return mHistoryBaseTime;
}

// Customize +++
int64_t BatteryStatsImpl::getFileBaseTime() {
    GLOGENTRY();

    return mHistoryFileTime;
}
// Customize ---

// Customize +++
int64_t BatteryStatsImpl::getStartHistoryTime(int32_t index) {
    GLOGENTRY();

    int32_t tmpSize = mHistoryStartTimeList->size();
    int64_t tmpTime = 0;

    if (DEBUG_ON) {
        GLOGD("get Start History Time: index = %d, tmpSize = %d", index, tmpSize);
    }

    if ((index < 0) ||(index >= tmpSize)) {
        GLOGW("Out the range for get History Time : %d", index);
        return 0;
    }

    tmpTime = mHistoryStartTimeList->get(index)->longlongValue();

    if (DEBUG_ON) {
        GLOGD("History Time Value: %lld", tmpTime);
    }

    return tmpTime;
}
// Customize ---

int32_t BatteryStatsImpl::getStartCount() {
    GLOGENTRY();

    return mStartCount;
}

bool BatteryStatsImpl::isOnBattery() {
    GLOGENTRY();

    return mOnBattery;
}

bool BatteryStatsImpl::isScreenOn() {
    GLOGENTRY();

    return mScreenOn;
}

void BatteryStatsImpl::initTimes() {
    GLOGENTRY();

    mBatteryRealtime = mTrackBatteryPastUptime = 0;
    mBatteryUptime = mTrackBatteryPastRealtime = 0;
    mUptimeStart = mTrackBatteryUptimeStart = uptimeMillis() * 1000;
    mRealtimeStart = mTrackBatteryRealtimeStart = elapsedRealtime() * 1000;
    mUnpluggedBatteryUptime = getBatteryUptimeLocked(mUptimeStart);
    mUnpluggedBatteryRealtime = getBatteryRealtimeLocked(mRealtimeStart);
}

void BatteryStatsImpl::initDischarge() {
    GLOGENTRY();

    mLowDischargeAmountSinceCharge = 0;
    mHighDischargeAmountSinceCharge = 0;
    mDischargeAmountScreenOn = 0;
    mDischargeAmountScreenOnSinceCharge = 0;
    mDischargeAmountScreenOff = 0;
    mDischargeAmountScreenOffSinceCharge = 0;
}

void BatteryStatsImpl::resetAllStatsLocked() {
    GLOGENTRY();

    // Customize +++
    int64_t startTimeMillis = 0;
    int64_t tempTime = 0;
    bool tmpValue = false;
    sp<Uid> uidValue = NULL;

    if (DEBUG_ON) {
        GLOGD("reset All Stats Locked Start");
    }

    if (DEBUG_SECURITY) {
        startTimeMillis = uptimeMillis();
    }
    // Customize ---

    mStartCount = 0;
    initTimes();
    mScreenOnTimer->reset(this, false);

    for (int32_t i = 0; i < NUM_SCREEN_BRIGHTNESS_BINS; i++) {
        mScreenBrightnessTimer[i]->reset(this, false);
    }

    mInputEventCounter->reset(false);
    mPhoneOnTimer->reset(this, false);
    mAudioOnTimer->reset(this, false);
    mVideoOnTimer->reset(this, false);

    for (int32_t i = 0; i < SignalStrength::NUM_SIGNAL_STRENGTH_BINS; i++) {
        (*mPhoneSignalStrengthsTimer)[i]->reset(this, false);
    }

    mPhoneSignalScanningTimer->reset(this, false);

    for (int32_t i = 0; i < NUM_DATA_CONNECTION_TYPES; i++) {
        (*mPhoneDataConnectionsTimer)[i]->reset(this, false);
    }

    mWifiOnTimer->reset(this, false);
    mGlobalWifiRunningTimer->reset(this, false);
    mBluetoothOnTimer->reset(this, false);

    // Customize +++
    if (DEBUG_SECURITY) {
        tempTime = uptimeMillis() - startTimeMillis;
        GLOGD("step1 spends time = %lld", tempTime);
    }

    if (useNewMethodToResetUidData()) {
        // TODO  clone  -> memleak ??
        // mUidStatsDel = mUidStats.clone();

        for (int32_t i = 0; i < static_cast<int32_t>(mUidStats.size()); i++) {
             mUidStatsDel.add(mUidStats.keyAt(i), mUidStats.valueAt(i));
        }

        if (DEBUG_ON) {
            GLOGD("before total size = mUidStatsDel.size() = %d , mUidStats.size() = %d", mUidStatsDel.size(), mUidStats.size());
        }

        int32_t uidSize = mUidStats.size();
        sp<Blob<int32_t> > mUidRunTable = new Blob<int32_t>(uidSize);

        if (DEBUG_SECURITY) {
            startTimeMillis = uptimeMillis();
        }

        for (int32_t i = 0; i < uidSize; i++) {
            if (mUidStats.valueAt(i)->isRunning()) {
                mUidRunTable[i] = 1;
            } else {
                mUidRunTable[i] = 0;
            }
        }

        if (DEBUG_SECURITY) {
            tempTime = uptimeMillis() - startTimeMillis;
            GLOGD("step2-1 spends time = %lld", tempTime);
            startTimeMillis = uptimeMillis();
        }

        for (int32_t i = uidSize - 1; i >= 0; i--) {
            if (mUidRunTable[i] == 0) {
                {
                    GLOGAUTOMUTEX(_l, mThisLock);
                    mUidStats.removeItemsAt(i);
                }
            } else {
                mUidStatsDel.removeItemsAt(i);
            }
        }

        if (DEBUG_ON) {
            GLOGD("after total size = mUidStatsDel.size() = %d , mUidStats.size() = %d", 
                    mUidStatsDel.size(), mUidStats.size());
        }

        int64_t start = elapsedRealtime();
        GLOGI("mUidStats size = %d", mUidStats.size());
        for (int32_t i = mUidStats.size() - 1; i >= 0; i--) {
            {
                GLOGAUTOMUTEX(_l, mThisLock);
                uidValue = mUidStats.valueAt(i);

                if (uidValue == NULL) {
                    if (DEBUG_ON) {
                        GLOGD("index = %d,size = %d", i, mUidStats.size());
                    }
                    break;
                }

                if (uidValue->reset()) {
                    mUidStats.removeItem(mUidStats.keyAt(i));
                }
            }
        }
        int64_t end = elapsedRealtime();
        GLOGI("step2-2-2 spends time start=%lld end=%lld duration=%lld",start, end, (end - start));

        if (DEBUG_SECURITY) {
            tempTime = uptimeMillis() - startTimeMillis;
            GLOGD("step2-2 spends time = %lld", tempTime);
            startTimeMillis = uptimeMillis();
        }

        // re add class to mUnpluggables
        // addBatteryStatsDataToUnpluggables();

        if (DEBUG_SECURITY) {
            tempTime = uptimeMillis() - startTimeMillis;
            GLOGD("step2-3 spends time = %lld", tempTime);
            startTimeMillis = uptimeMillis();
        }

        handleBatteryStatsData();

        if (DEBUG_SECURITY) {
            tempTime = uptimeMillis() - startTimeMillis;
            GLOGD("step2-4 spends time = %lld", tempTime);
        }

    } else {
        if (DEBUG_ON) {
            GLOGD("use original design size = %d", mUidStats.size());
        }

        if (DEBUG_SECURITY) {
            startTimeMillis = uptimeMillis();
        }

        {
            GLOGAUTOMUTEX(_l, mThisLock);

            for (int32_t i = 0; i < static_cast<int32_t>(mUidStats.size()); i++) {
                uidValue = mUidStats.valueAt(i);

                if (uidValue == NULL) {
                    if (DEBUG_ON) {
                        GLOGD("index = %d,size = %d", i, mUidStats.size());
                    }
                    break;
                }

                if (uidValue->reset()) {
                    mUidStats.removeItem(mUidStats.keyAt(i));
                    i--;
                }
            }
        }

        if (DEBUG_SECURITY) {
            tempTime = uptimeMillis() - startTimeMillis;
            GLOGD("step2-5 spends time = %lld", tempTime);
        }
    }

    // synchronized (sLockPlug)
    {
        GLOGAUTOMUTEX(_l, mPlugLock);

        if (DEBUG_SECURITY) {
            startTimeMillis = uptimeMillis();
        }

        if (mKernelWakelockStats.size() > 0) {
            const int32_t NKW = mKernelWakelockStats.size();
            for (int32_t i = 0; i < NKW; i++) {
                sp<SamplingTimer> timer = mKernelWakelockStats.valueAt(i);

                mUnpluggables->remove(timer);
            }
            mKernelWakelockStats.clear();
        }

        if (DEBUG_SECURITY) {
            tempTime = uptimeMillis() - startTimeMillis;
            GLOGD("step3 spends time = %lld", tempTime);
        }
    }

    if (DEBUG_SECURITY) {
        startTimeMillis = uptimeMillis();
    }
    // Customize ---

    initDischarge();
    clearHistoryLocked();

    // Customize +++
    if (DEBUG_SECURITY) {
        tempTime = uptimeMillis() - startTimeMillis;
        GLOGD("step4 spends time = %lld", tempTime);
    }

    if (DEBUG_ON) {
        GLOGI("reset All Stats Locked End");
    }
    // Customize ---
}

void BatteryStatsImpl::updateDischargeScreenLevelsLocked(bool oldScreenOn, bool newScreenOn) {
    GLOGENTRY();

    if (oldScreenOn) {
        int32_t diff = mDischargeScreenOnUnplugLevel - mDischargeCurrentLevel;

        if (diff > 0) {
            mDischargeAmountScreenOn += diff;
            mDischargeAmountScreenOnSinceCharge += diff;
        }
    } else {
        int32_t diff = mDischargeScreenOffUnplugLevel - mDischargeCurrentLevel;

        if (diff > 0) {
            mDischargeAmountScreenOff += diff;
            mDischargeAmountScreenOffSinceCharge += diff;
        }
    }

    if (newScreenOn) {
        mDischargeScreenOnUnplugLevel = mDischargeCurrentLevel;
        mDischargeScreenOffUnplugLevel = 0;
    } else {
        mDischargeScreenOnUnplugLevel = 0;
        mDischargeScreenOffUnplugLevel = mDischargeCurrentLevel;
    }
}

void BatteryStatsImpl::setOnBattery(bool onBattery, int32_t oldStatus, int32_t level) {
    GLOGENTRY();

    {
        GLOGAUTOMUTEX(_l, mThisLock);
        setOnBatteryLocked(onBattery, oldStatus, level);
    }
}

void BatteryStatsImpl::setOnBatteryLocked(bool onBattery, int32_t oldStatus, int32_t level) {
    GLOGENTRY();

    bool doWrite = false;
    sp<Message> m = mHandler->obtainMessage(MSG_REPORT_POWER_CHANGE);
    m->arg1 = onBattery ? 1 : 0;
    mHandler->sendMessage(m);
    mOnBattery = mOnBatteryInternal = onBattery;

    // Customize +++
    int64_t startTimeMillis = 0;
    int64_t tempTime = 0;

    if (DEBUG_SECURITY) {
        startTimeMillis = uptimeMillis();
    }

    if (1) {  // (ProfileConfig::getProfileDebugWakelock()) {
        mBatteryStatusList->add(new Boolean(mOnBattery));
        mBatteryStatusTimeList->add(new LongLong(System::currentTimeMillis()));

        if (mBatteryStatusList->size() > 100) {
            for (int32_t  i = 0 ; i < 50 ; ++i) {
                mBatteryStatusList->remove(0);
                mBatteryStatusTimeList->remove(0);
            }
        }
    }

    if (DEBUG_SECURITY) {
        tempTime = uptimeMillis() - startTimeMillis;
        if (tempTime >= BATTERY_MAX_RUN_TIME_300MS) {
            GLOGW("getProfileDebugWakelock spends time = %lld", tempTime);
        }
    }
    // Customize ---

    int64_t uptime = uptimeMillis() * 1000;
    int64_t mSecRealtime = elapsedRealtime();
    int64_t realtime = mSecRealtime * 1000;

    if (onBattery) {
    // We will reset our status if we are unplugging after the
    // battery was last full, or the level is at 100, or
    // we have gone through a significant charge (from a very low
    // level to a now very high level).

    // Customize +++
    if (oldStatus == BatteryManager::BATTERY_STATUS_FULL || level >= 70) {
            // || level >= 90
            // || (mDischargeCurrentLevel < 20 && level >= 80)) {
        doWrite = true;

        if (DEBUG_SECURITY) {
            startTimeMillis = uptimeMillis();
        }

        resetAllStatsLocked();

        if (DEBUG_SECURITY) {
            tempTime = uptimeMillis() - startTimeMillis;
            if (tempTime >= BATTERY_MAX_RUN_TIME_300MS) {
                GLOGW("resetAllStatsLocked spends time = %lld", tempTime);
            }
        }

        mDischargeStartLevel = level;
    }

    if (DEBUG_SECURITY) {
        startTimeMillis = uptimeMillis();
    }
    // Customize ---

    updateKernelWakelocksLocked();

    // Customize +++
    if (DEBUG_SECURITY) {
        tempTime = uptimeMillis() - startTimeMillis;
        if (tempTime >= BATTERY_MAX_RUN_TIME_300MS) {
            GLOGW("updateKernelWakelocksLocked spends time = %lld", tempTime);
        }
    }
    // Customize ---

    mHistoryCur->batteryLevel = static_cast<byte_t>(level);
    mHistoryCur->states &= ~HistoryItem::STATE_BATTERY_PLUGGED_FLAG;

    if (DEBUG_HISTORY) {
        // Slog.v(TAG, "Battery unplugged to: "
        // + Integer.toHexString(mHistoryCur.states));
    }

    // Customize +++
    if (DEBUG_SECURITY) {
        startTimeMillis = uptimeMillis();
    }
    // Customize ---

    addHistoryRecordLocked(mSecRealtime);

    // Customize +++
    if (DEBUG_SECURITY) {
        tempTime = uptimeMillis() - startTimeMillis;
        if (tempTime >= BATTERY_MAX_RUN_TIME_300MS) {
            GLOGW("addHistoryRecordLocked spends time = %lld", tempTime);
        }
    }
    // Customize ---

    mTrackBatteryUptimeStart = uptime;
    mTrackBatteryRealtimeStart = realtime;
    mUnpluggedBatteryUptime = getBatteryUptimeLocked(uptime);
    mUnpluggedBatteryRealtime = getBatteryRealtimeLocked(realtime);
    mDischargeCurrentLevel = mDischargeUnplugLevel = level;

    if (mScreenOn) {
        mDischargeScreenOnUnplugLevel = level;
        mDischargeScreenOffUnplugLevel = 0;
    } else {
        mDischargeScreenOnUnplugLevel = 0;
        mDischargeScreenOffUnplugLevel = level;
    }

    mDischargeAmountScreenOn = 0;
    mDischargeAmountScreenOff = 0;
    doUnplugLocked(mUnpluggedBatteryUptime, mUnpluggedBatteryRealtime);
    } else {
        // Customize +++
        if (DEBUG_SECURITY) {
            startTimeMillis = uptimeMillis();
        }
        // Customize ---

        updateKernelWakelocksLocked();

        // Customize +++
        if (DEBUG_SECURITY) {
            tempTime = uptimeMillis() - startTimeMillis;
            if (tempTime >= BATTERY_MAX_RUN_TIME_300MS) {
                GLOGW("updateKernelWakelocksLocked-1 spends time = %lld", tempTime);
            }
        }
        // Customize ---

        mHistoryCur->batteryLevel = static_cast<byte_t>(level);
        mHistoryCur->states |= HistoryItem::STATE_BATTERY_PLUGGED_FLAG;

        if (DEBUG_HISTORY) {
            GLOGV("%s Battery plugged to: %s", LOG_TAG, Integer::toHexString(mHistoryCur->states)->string());
        }

        // Customize +++
        if (DEBUG_SECURITY) {
            startTimeMillis = uptimeMillis();
        }
        // Customize ---

        addHistoryRecordLocked(mSecRealtime);

        // Customize +++
        if (DEBUG_SECURITY) {
            tempTime = uptimeMillis() - startTimeMillis;
            if (tempTime >= BATTERY_MAX_RUN_TIME_300MS) {
                GLOGW("addHistoryRecordLocked-1 spends time = %lld", tempTime);
            }
        }
        // Customize ---

        mTrackBatteryPastUptime += uptime - mTrackBatteryUptimeStart;
        mTrackBatteryPastRealtime += realtime - mTrackBatteryRealtimeStart;
        mDischargeCurrentLevel = level;

        if (level < mDischargeUnplugLevel) {
            mLowDischargeAmountSinceCharge += mDischargeUnplugLevel - level - 1;
            mHighDischargeAmountSinceCharge += mDischargeUnplugLevel - level;
        }

        updateDischargeScreenLevelsLocked(mScreenOn, mScreenOn);
        doPlugLocked(getBatteryUptimeLocked(uptime), getBatteryRealtimeLocked(realtime));
    }

    if (doWrite || (mLastWriteTime + (60 * 1000)) < mSecRealtime) {
        if (mFile != NULL) {
            // Customize +++
            if (DEBUG_SECURITY) {
                startTimeMillis = uptimeMillis();
            }
            // Customize ---

            writeAsyncLocked();

            // Customize +++
            if (DEBUG_SECURITY) {
                tempTime = uptimeMillis() - startTimeMillis;
                if (tempTime >= BATTERY_MAX_RUN_TIME_300MS) {
                    GLOGW("writeAsyncLocked spends time = %lld", tempTime);
                }
            }
            // Customize ---
        }
    }
}

void BatteryStatsImpl::setBatteryState(int32_t status, int32_t health, int32_t plugType, int32_t level, int32_t temp, int32_t volt) {
    GLOGENTRY();

    // Customize +++
    int64_t startTimeMillis = 0;
    int64_t tempTime = 0;

    if (DEBUG_SECURITY) {
        startTimeMillis = uptimeMillis();
    }
    // Customize ---

    {
        GLOGAUTOMUTEX(_l, mThisLock);
        bool onBattery = plugType == BATTERY_PLUGGED_NONE;
        int32_t oldStatus = mHistoryCur->batteryStatus;

        if (!mHaveBatteryLevel) {
            mHaveBatteryLevel = true;

            // We start out assuming that the device is plugged in (not
            // on battery).  If our first report is now that we are indeed
            // plugged in, then twiddle our state to correctly reflect that
            // since we won't be going through the full setOnBattery().
            if (onBattery == mOnBattery) {
                if (onBattery) {
                    mHistoryCur->states &= ~HistoryItem::STATE_BATTERY_PLUGGED_FLAG;
                } else {
                    mHistoryCur->states |= HistoryItem::STATE_BATTERY_PLUGGED_FLAG;
                }
            }

            oldStatus = status;
        }

        if (onBattery) {
            mDischargeCurrentLevel = level;
            mRecordingHistory = true;
        }

        if (onBattery != mOnBattery) {
            mHistoryCur->batteryLevel = static_cast<byte_t>(level);
            mHistoryCur->batteryStatus = static_cast<byte_t>(status);
            mHistoryCur->batteryHealth = static_cast<byte_t>(health);
            mHistoryCur->batteryPlugType = static_cast<byte_t>(plugType);
            mHistoryCur->batteryTemperature = static_cast<char16_t>(temp);
            mHistoryCur->batteryVoltage = static_cast<char16_t>(volt);
            setOnBatteryLocked(onBattery, oldStatus, level);
        } else {
            bool changed = false;

            if (mHistoryCur->batteryLevel != level) {
                mHistoryCur->batteryLevel = static_cast<byte_t>(level);
                changed = true;
            }

            if (mHistoryCur->batteryStatus != status) {
                mHistoryCur->batteryStatus = static_cast<byte_t>(status);
                changed = true;
            }

            if (mHistoryCur->batteryHealth != health) {
                mHistoryCur->batteryHealth = static_cast<byte_t>(health);
                changed = true;
            }

            if (mHistoryCur->batteryPlugType != plugType) {
                mHistoryCur->batteryPlugType = static_cast<byte_t>(plugType);
                changed = true;
            }

            if (temp >= (mHistoryCur->batteryTemperature + 10)
                    || temp <= (mHistoryCur->batteryTemperature - 10)) {
                mHistoryCur->batteryTemperature = static_cast<char16_t>(temp);
                changed = true;
            }

            if (volt > (mHistoryCur->batteryVoltage + 20)
                    || volt < (mHistoryCur->batteryVoltage - 20)) {
                mHistoryCur->batteryVoltage = static_cast<char16_t>(volt);
                changed = true;
            }

            if (changed) {
                addHistoryRecordLocked(elapsedRealtime());
            }
        }

        if (!onBattery && status == BatteryManager::BATTERY_STATUS_FULL) {
            // We don't record history while we are plugged in and fully charged.
            // The next time we are unplugged, history will be cleared.
            mRecordingHistory = false;
        }
    }

    // Customize +++
    if (DEBUG_SECURITY) {
        tempTime = uptimeMillis() - startTimeMillis;
        if (tempTime >= BATTERY_MAX_RUN_TIME_1S) {
            GLOGW("setBatteryState spends time = %lld", tempTime);
        }
    }
    // Customize ---
}

void BatteryStatsImpl::updateKernelWakelocksLocked() {
    GLOGENTRY();

    KeyedVector<sp<String>, sp<KernelWakelockStats> >& m = readKernelWakelockStats();

    if (m.size() == 0) {
        // Not crashing might make board bringup easier.
        GLOGW("%s Couldn't get kernel wake lock stats", LOG_TAG);
        return;
    }

    const int32_t NKW = mKernelWakelockStats.size();

    for (uint32_t i = 0; i < m.size(); i++) {
        sp<String> name =  m.keyAt(i);
        sp<KernelWakelockStats> kws = safe_cast<BatteryStatsImpl::KernelWakelockStats*>(m.valueAt(i));

        if (kws == NULL) {
            GLOGW("BatteryStatsImpl::updateKernelWakelocksLocked kws is NULL");
            break;
        }

        if (name == NULL) {
            GLOGW("BatteryStatsImpl::updateKernelWakelocksLocked name is NULL");
            break;
        }

        sp<SamplingTimer> kwlt;
        int32_t index = mKernelWakelockStats.indexOfKey(name);

        if (index >= 0) {
            kwlt = mKernelWakelockStats.valueFor(name);
        }

        if (kwlt == NULL) {
            kwlt = new SamplingTimer(this, mUnpluggables, mOnBatteryInternal, true /* track reported values */);

            if (kwlt == NULL) {
                GLOGW("BatteryStatsImpl::updateKernelWakelocksLocked kwlt is NULL");
                break;
            }

            mKernelWakelockStats.add(name, kwlt);
        }

        if (kwlt == NULL) {
            GLOGW("BatteryStatsImpl::updateKernelWakelocksLocked kwlt is NULL");
            break;
        }

        kwlt->updateCurrentReportedCount(kws->mCount);
        kwlt->updateCurrentReportedTotalTime(kws->mTotalTime);
        kwlt->setUpdateVersion(sKernelWakelockUpdateVersion);
    }

    if (m.size() != mKernelWakelockStats.size()) {
        // Set timers to stale if they didn't appear in /proc/wakelocks this time.

        const int32_t NKW = mKernelWakelockStats.size();

        for (int32_t i = 0; i < NKW; i++) {
            sp<SamplingTimer> st = mKernelWakelockStats.valueAt(i);

            if (st == NULL) {
                GLOGW("BatteryStatsImpl::updateKernelWakelocksLocked st is NULL");
                break;
            }

            if (st->getUpdateVersion() != sKernelWakelockUpdateVersion) {
                st->setStale();
            }
        }
    }
}

int64_t BatteryStatsImpl::getAwakeTimeBattery() {
    GLOGENTRY();

    return computeBatteryUptime(getBatteryUptimeLocked(), BatteryStats::STATS_CURRENT);
}

int64_t BatteryStatsImpl::getAwakeTimePlugged() {
    GLOGENTRY();

    return (uptimeMillis() * 1000) - getAwakeTimeBattery();
}

int64_t BatteryStatsImpl::computeUptime(int64_t curTime, int32_t which) {
    GLOGENTRY();

    switch (which) {
        case BatteryStats::STATS_SINCE_CHARGED:
            return mUptime + (curTime - mUptimeStart);
        case BatteryStats::STATS_LAST:
            return mLastUptime;
        case BatteryStats::STATS_CURRENT:
            return (curTime - mUptimeStart);
        case BatteryStats::STATS_SINCE_UNPLUGGED:
            return (curTime - mTrackBatteryUptimeStart);
    }

    return 0;
}

int64_t BatteryStatsImpl::computeRealtime(int64_t curTime, int32_t which) {
    GLOGENTRY();

    switch (which) {
        case BatteryStats::STATS_SINCE_CHARGED:
            return mRealtime + (curTime - mRealtimeStart);
        case BatteryStats::STATS_LAST:
            return mLastRealtime;
        case BatteryStats::STATS_CURRENT:
            return (curTime - mRealtimeStart);
        case BatteryStats::STATS_SINCE_UNPLUGGED:
            return (curTime - mTrackBatteryRealtimeStart);
    }

    return 0;
}

int64_t BatteryStatsImpl::computeBatteryUptime(int64_t curTime, int32_t which) {
    GLOGENTRY();

    switch (which) {
        case BatteryStats::STATS_SINCE_CHARGED:
            return mBatteryUptime + getBatteryUptime(curTime);
        case BatteryStats::STATS_LAST:
            return mBatteryLastUptime;
        case BatteryStats::STATS_CURRENT:
            return getBatteryUptime(curTime);
        case BatteryStats::STATS_SINCE_UNPLUGGED:
            return getBatteryUptimeLocked(curTime) - mUnpluggedBatteryUptime;
    }

    return 0;
}

int64_t BatteryStatsImpl::computeBatteryRealtime(int64_t curTime, int32_t which) {
    GLOGENTRY();

    switch (which) {
        case BatteryStats::STATS_SINCE_CHARGED:
            return mBatteryRealtime + getBatteryRealtimeLocked(curTime);
        case BatteryStats::STATS_LAST:
            return mBatteryLastRealtime;
        case BatteryStats::STATS_CURRENT:
            return getBatteryRealtimeLocked(curTime);
        case BatteryStats::STATS_SINCE_UNPLUGGED:
            return getBatteryRealtimeLocked(curTime) - mUnpluggedBatteryRealtime;
    }

    return 0;
}

int64_t BatteryStatsImpl::getBatteryUptimeLocked(int64_t curTime) {
    GLOGENTRY();

    int64_t time = mTrackBatteryPastUptime;

    if (mOnBatteryInternal) {
        time += curTime - mTrackBatteryUptimeStart;
    }

    return time;
}

int64_t BatteryStatsImpl::getBatteryUptimeLocked() {
    GLOGENTRY();

    return getBatteryUptime(uptimeMillis() * 1000);
}

int64_t BatteryStatsImpl::getBatteryUptime(int64_t curTime) {
    GLOGENTRY();

    return getBatteryUptimeLocked(curTime);
}

int64_t BatteryStatsImpl::getBatteryRealtimeLocked(int64_t curTime) {
    GLOGENTRY();

    int64_t time = mTrackBatteryPastRealtime;

    if (mOnBatteryInternal) {
        time += curTime - mTrackBatteryRealtimeStart;
    }

    return time;
}

int64_t BatteryStatsImpl::getBatteryRealtime(int64_t curTime) {
    GLOGENTRY();

    return getBatteryRealtimeLocked(curTime);
}

int64_t BatteryStatsImpl::getTcpBytes(int64_t current, const sp<Blob<int64_t> >& dataBytes, int32_t which) {
    GLOGENTRY();

    if (which == BatteryStats::STATS_LAST) {
        return dataBytes[BatteryStats::STATS_LAST];
    } else {
        if (which == BatteryStats::STATS_SINCE_UNPLUGGED) {
            if (dataBytes[BatteryStats::STATS_SINCE_UNPLUGGED] < 0) {
                return dataBytes[BatteryStats::STATS_LAST];
            } else {
                return current - dataBytes[BatteryStats::STATS_SINCE_UNPLUGGED];
            }
        } else if (which == BatteryStats::STATS_SINCE_CHARGED) {
            return (current - dataBytes[BatteryStats::STATS_CURRENT]) + dataBytes[BatteryStats::STATS_SINCE_CHARGED];
        }

        return current - dataBytes[BatteryStats::STATS_CURRENT];
    }
}

int64_t BatteryStatsImpl::getMobileTcpBytesSent(int32_t which) {
    GLOGENTRY();

    const int64_t mobileTxBytes = getNetworkStatsSummary()->getTotal(NULL, mMobileIfaces)->mtxBytes;
    return getTcpBytes(mobileTxBytes, mMobileDataTx, which);
}

int64_t BatteryStatsImpl::getMobileTcpBytesReceived(int32_t which) {
    GLOGENTRY();

    const int64_t mobileRxBytes = getNetworkStatsSummary()->getTotal(NULL, mMobileIfaces)->mrxBytes;
    return getTcpBytes(mobileRxBytes, mMobileDataRx, which);
}

int64_t BatteryStatsImpl::getTotalTcpBytesSent(int32_t which) {
    GLOGENTRY();

    const int64_t totalTxBytes = getNetworkStatsSummary()->getTotal(NULL)->mtxBytes;
    return getTcpBytes(totalTxBytes, mTotalDataTx, which);
}

int64_t BatteryStatsImpl::getTotalTcpBytesReceived(int32_t which) {
    GLOGENTRY();

    const int64_t totalRxBytes = getNetworkStatsSummary()->getTotal(NULL)->mrxBytes;
    return getTcpBytes(totalRxBytes, mTotalDataRx, which);
}

int32_t BatteryStatsImpl::getDischargeStartLevel() {
    GLOGENTRY();

    {
        GLOGAUTOMUTEX(_l, mThisLock);
        return getDischargeStartLevelLocked();
    }
}

int32_t BatteryStatsImpl::getDischargeStartLevelLocked() {
    GLOGENTRY();

    return mDischargeUnplugLevel;
}

int32_t BatteryStatsImpl::getDischargeCurrentLevel() {
    GLOGENTRY();

    {
        GLOGAUTOMUTEX(_l, mThisLock);
        return getDischargeCurrentLevelLocked();
    }
}

int32_t BatteryStatsImpl::getDischargeCurrentLevelLocked() {
    return mDischargeCurrentLevel;
}

int32_t BatteryStatsImpl::getLowDischargeAmountSinceCharge() {
    GLOGENTRY();

    {
        GLOGAUTOMUTEX(_l, mThisLock);
        int32_t val = mLowDischargeAmountSinceCharge;

        if (mOnBattery && mDischargeCurrentLevel < mDischargeUnplugLevel) {
            val += mDischargeUnplugLevel - mDischargeCurrentLevel - 1;
        }

        return val;
    }
}

int32_t BatteryStatsImpl::getHighDischargeAmountSinceCharge() {
    GLOGENTRY();

    {
        GLOGAUTOMUTEX(_l, mThisLock);
        int32_t val = mHighDischargeAmountSinceCharge;

        if (mOnBattery && mDischargeCurrentLevel < mDischargeUnplugLevel) {
            val += mDischargeUnplugLevel - mDischargeCurrentLevel;
        }

        return val;
    }
}
int32_t BatteryStatsImpl::getDischargeAmountScreenOn() {
    GLOGENTRY();

    {
        GLOGAUTOMUTEX(_l, mThisLock);
        int32_t val = mDischargeAmountScreenOn;

        if (mOnBattery && mScreenOn
                && mDischargeCurrentLevel < mDischargeScreenOnUnplugLevel) {
            val += mDischargeScreenOnUnplugLevel - mDischargeCurrentLevel;
        }

        return val;
    }
}

int32_t BatteryStatsImpl::getDischargeAmountScreenOnSinceCharge() {
    GLOGENTRY();

    {
        GLOGAUTOMUTEX(_l, mThisLock);
        int32_t val = mDischargeAmountScreenOnSinceCharge;

        if (mOnBattery && mScreenOn
                && mDischargeCurrentLevel < mDischargeScreenOnUnplugLevel) {
            val += mDischargeScreenOnUnplugLevel - mDischargeCurrentLevel;
        }

        return val;
    }
}

int32_t BatteryStatsImpl::getDischargeAmountScreenOff() {
    GLOGENTRY();

    {
        GLOGAUTOMUTEX(_l, mThisLock);
        int32_t val = mDischargeAmountScreenOff;

        if (mOnBattery && !mScreenOn
                && mDischargeCurrentLevel < mDischargeScreenOffUnplugLevel) {
            val += mDischargeScreenOffUnplugLevel - mDischargeCurrentLevel;
        }

        return val;
    }
}

int32_t BatteryStatsImpl::getDischargeAmountScreenOffSinceCharge() {
    GLOGENTRY();

    {
        GLOGAUTOMUTEX(_l, mThisLock);
        int32_t val = mDischargeAmountScreenOffSinceCharge;

        if (mOnBattery && !mScreenOn
                && mDischargeCurrentLevel < mDischargeScreenOffUnplugLevel) {
            val += mDischargeScreenOffUnplugLevel - mDischargeCurrentLevel;
        }

        return val;
    }
}

int32_t BatteryStatsImpl::getCpuSpeedSteps() {
    GLOGENTRY();

    return sNumSpeedSteps;
}

// Customize +++
int32_t BatteryStatsImpl::getGpuSpeedSteps() {
    GLOGENTRY();

    if (sNumGpuSpeedSteps <= 1) {
        return 1;
    }

    if (sNumGpuSpeedSteps >= MAX_GPU_SPEED_NUM) {
        return MAX_GPU_SPEED_NUM;
    }

    return sNumGpuSpeedSteps;
}
// Customize ---

sp<BatteryStatsImpl::Uid> BatteryStatsImpl::getUidStatsLocked(int32_t uid) {
    GLOGENTRY();

    sp<Uid> u = NULL;

    {
        GLOGAUTOMUTEX(_l, mThisLock);

        int32_t index = mUidStats.indexOfKey(uid);

        if (index >= 0) {
            u = mUidStats.valueAt(index);
        }

        if (u == NULL) {
            u = new Uid(this, uid);
            mUidStats.add(uid, u);
        }
    }

    return u;
}

void BatteryStatsImpl::removeUidStatsLocked(int32_t uid) {
    GLOGENTRY();

    {
        GLOGAUTOMUTEX(_l, mThisLock);
        mUidStats.removeItem(uid);
    }
}

sp<BatteryStats::Uid::Proc> BatteryStatsImpl::getProcessStatsLocked(int32_t uid, const sp<String>& name) {
    GLOGENTRY();

    sp<Uid> u = getUidStatsLocked(uid);
    return u->getProcessStatsLocked(name);
}

sp<BatteryStats::Uid::Proc> BatteryStatsImpl::getProcessStatsLocked(const sp<String>& name, int32_t pid) {
    GLOGENTRY();

    int32_t uid;
    int32_t index = mUidCache.indexOfKey(name);

    if (index >= 0) {
        uid = mUidCache.valueAt(index);
    } else {
        uid = Process::getUidForPid(pid);
        mUidCache.add(name, uid);
    }

    sp<Uid> u = getUidStatsLocked(uid);
    return u->getProcessStatsLocked(name);
}

sp<BatteryStatsImpl::Uid::Pkg> BatteryStatsImpl::getPackageStatsLocked(int32_t uid, const sp<String>& pkg) {
    GLOGENTRY();

    sp<Uid> u = getUidStatsLocked(uid);
    return u->getPackageStatsLocked(pkg);
}

sp<BatteryStatsImpl::Uid::Pkg::Serv> BatteryStatsImpl::getServiceStatsLocked(int32_t uid, const sp<String>& pkg, const sp<String>& name) {
    GLOGENTRY();

    sp<Uid> u = getUidStatsLocked(uid);
    return u->getServiceStatsLocked(pkg, name);
}

void BatteryStatsImpl::distributeWorkLocked(int32_t which) {
    GLOGENTRY();

    // Aggregate all CPU time associated with WIFI.
    sp<Uid> wifiUid;

    {
        GLOGAUTOMUTEX(_l, mThisLock);

        int32_t index = mUidStats.indexOfKey(Process::WIFI_UID);

        if (index >= 0) {
            wifiUid = mUidStats.valueAt(index);
        }

        if (wifiUid != NULL) {
            int64_t uSecTime = computeBatteryRealtime(elapsedRealtime() * 1000, which);
            const int32_t WF = wifiUid->mProcessStats.size();

            for (int32_t j = 0; j < WF; j++) {
                sp<Uid::Proc> proc = safe_cast<BatteryStatsImpl::Uid::Proc*>(wifiUid->mProcessStats.valueAt(j));
                int64_t totalRunningTime = getGlobalWifiRunningTime(uSecTime, which);

                for (int32_t i = 0; i < static_cast<int32_t>(mUidStats.size()); i++) {
                    sp<Uid> uid = mUidStats.valueAt(i);

                    if (uid->mUid != Process::WIFI_UID) {
                        int64_t uidRunningTime = uid->getWifiRunningTime(uSecTime, which);

                        if (uidRunningTime > 0) {
                            sp<Uid::Proc> uidProc = safe_cast<BatteryStatsImpl::Uid::Proc*>(uid->getProcessStatsLocked(new String("*wifi*")));
                            int64_t time = proc->getUserTime(which);
                            time = (time * uidRunningTime) / totalRunningTime;
                            uidProc->mUserTime += time;
                            proc->mUserTime -= time;
                            time = proc->getSystemTime(which);
                            time = (time * uidRunningTime) / totalRunningTime;
                            uidProc->mSystemTime += time;
                            proc->mSystemTime -= time;
                            time = proc->getForegroundTime(which);
                            time = (time * uidRunningTime) / totalRunningTime;
                            uidProc->mForegroundTime += time;
                            proc->mForegroundTime -= time;

                            for (int32_t sb = 0; sb < static_cast<int32_t>(proc->mSpeedBins->length()); sb++) {
                                sp<SamplingCounter> sc = (*proc->mSpeedBins)[sb];

                                if (sc != NULL) {
                                    time = sc->getCountLocked(which);
                                    time = (time * uidRunningTime) / totalRunningTime;
                                    sp<SamplingCounter> uidSc = (*uidProc->mSpeedBins)[sb];

                                    if (uidSc == NULL) {
                                        uidSc = new SamplingCounter(this, mUnpluggables, 1);
                                        (*uidProc->mSpeedBins)[sb] = uidSc;
                                    }

                                    uidSc->mCount->addAndGet(static_cast<int32_t>(time));
                                    sc->mCount->addAndGet(static_cast<int32_t>(-time));
                                }
                            }

                            totalRunningTime -= uidRunningTime;
                        }
                    }
                }
            }
        }
    }
}

void BatteryStatsImpl::shutdownLocked() {
    GLOGENTRY();

    // Customize +++
    if (DEBUG_ON) {
        GLOGD("shutdownLocked Enter");
    }

    mHistoryShutdownTime = System::currentTimeMillis();

    if (DEBUG_ON) {
        GLOGD("current mHistoryShutdownTime: %lld", mHistoryShutdownTime);
    }
    // Customize ---

    writeSyncLocked();
    mShuttingDown = true;
}

void BatteryStatsImpl::writeAsyncLocked() {
    GLOGENTRY();

    writeLocked(false);
}

void BatteryStatsImpl::writeSyncLocked() {
    GLOGENTRY();

    writeLocked(true);
}

void BatteryStatsImpl::writeLocked(bool sync) {
    GLOGENTRY();

    if (mFile == NULL) {
        GLOGW("%s writeLocked: no file associated with this instance", "BatteryStats");
        return;
    }

    // Customize +++
    if (DEBUG_ON) {
        GLOGD("writeLocked Start sync = %d", sync);
    }
    // Customize ---

    if (mShuttingDown) {
        return;
    }

    Parcel *out = new Parcel();  // = Parcel::obtain();
    writeSummaryToParcel(out);
    mLastWriteTime = elapsedRealtime();

    if (mPendingWrite != NULL) {
        // mPendingWrite->recycle();
        mPendingWrite->freeData();
    }

    mPendingWrite = out;

    if (sync) {
        commitPendingDataToDisk();
    } else {
        sp<BatteryStatsImplThread> thr = new BatteryStatsImplThread(this);
        thr->run("BatteryStats-Write");
    }

    // Customize +++
    if (DEBUG_ON) {
        GLOGD("writeLocked End");
    }
    // Customize ---
}

void BatteryStatsImpl::commitPendingDataToDisk() {
    GLOGENTRY();

    Parcel *next = NULL;

    // Customize +++
    if (DEBUG_ON) {
        GLOGD("commitPendingDataToDisk Start");
    }
    // Customize ---

    {
        GLOGAUTOMUTEX(_l, mThisLock);
        next = mPendingWrite;
        mPendingWrite = NULL;

        if (next == NULL) {
            return;
        }

        mWriteLock->lock();
    }

    sp<File> file = mFile->chooseForWrite();
        sp<FileOutputStream> stream = new FileOutputStream(file);
    if (file->exists()) {
        stream->write(ParcelHelper::marshall(*next));
        stream->flush();
        FileUtils::sync(stream);
        stream->close();
        mFile->commit();

        // Customize +++
        if (1) {  // (ProfileConfig::getProfileDebugBatteryHistory()) {
            int64_t curTime = elapsedRealtime();
            GLOGD("%s", String::format("Histroy sync to disk at %08x (%08x+%08x)", mHistoryBaseTime, curTime, mHistoryBaseTime, curTime)->string());
        }
        // Customize ---
    } else {
        GLOGW("BatteryStats Error writing battery statistics");
        mFile->rollback();
    }

    // next->recycle();
    next->freeData();
    mWriteLock->unlock();

    // Customize +++
    if (DEBUG_ON) {
        GLOGD("commitPendingDataToDisk End");
    }
    // Customize ---
}

sp< Blob<byte_t> > BatteryStatsImpl::readFully(const sp<FileInputStream>& stream) {
    GLOGENTRY();

    int32_t pos = 0;
    uint32_t avail = stream->available();

    // Customize +++
    if (avail >= (MAX_MAX_HISTORY_ITEMS * 1024)) {  // guess a HistoryItem Object cost 1KB
        // TODO
        // GLOGE(String::format("The file stream is huge (%dKB). ", static_cast<int32_t>(avail / 1024)));
        return 0;
    }
    // Customize ---

    sp< Blob<byte_t> > data = new Blob<byte_t>(avail);

    while (true) {
        int32_t amt = stream->read(data, pos, data->length() - pos);
        // Log.i("foo", "Read " + amt + " bytes at " + pos
        //        + " of avail " + data.length);
        if (amt <= 0) {
            // Log.i("foo", "**** FINISHED READING: pos=" + pos
            //        + " len=" + data.length);
            return data;
        }
        pos += amt;
        avail = stream->available();
        if (avail > data->length() - pos) {
            sp< Blob<byte_t> > newData =  new Blob<byte_t>(pos+avail);
            System::arraycopy(data, 0, newData, 0, pos);
            data = newData;
        }
    }
}

void BatteryStatsImpl::readLocked() {
    GLOGENTRY();

    if (mFile == NULL) {
        GLOGW("BatteryStats readLocked: no file associated with this instance");
        return;
    }

    {
        GLOGAUTOMUTEX(_l, mThisLock);
        mUidStats.clear();
    }
    //  try {
    sp<File> file = mFile->chooseForRead();

    if (!file->exists()) {
        return;
    }

    sp<FileInputStream> stream = new FileInputStream(file);
    sp< Blob<byte_t> > raw = readFully(stream);

    // Customize +++
    if (raw == 0) {
        GLOGE("Read file failed!! Abort reading.");
        return;
    }
    // Customize ---

    Parcel *in = new Parcel();  // Parcel.obtain();
    ParcelHelper::unmarshall(*in, raw, 0, raw->length());
    in->setDataPosition(0);
    stream->close();
    readSummaryFromParcel(*in);
    /*
    } catch (java.io.IOException e) {
    Slog.e("BatteryStats", "Error reading battery statistics", e);
    }
    */

    // Customize +++
    int64_t tmpTime = System::currentTimeMillis();
    int64_t diff = 0;

    if (DEBUG_ON) {
        GLOGD("Current Open : %lld, Last shutdown : %lld", tmpTime, mHistoryShutdownTime);
    }

    if (mHistoryShutdownTime > 0) {
        diff = (tmpTime >= mHistoryShutdownTime) ? (tmpTime - mHistoryShutdownTime) : 0;
    }

    mHistoryStartTimeList->add(new LongLong(diff));
    // Customize ---

    int64_t now = elapsedRealtime();

    if (USE_OLD_HISTORY) {
        addHistoryRecordLocked(now, HistoryItem::CMD_START);
    }

    addHistoryBufferLocked(now, HistoryItem::CMD_START);
}

int32_t BatteryStatsImpl::describeContents() {
    GLOGENTRY();

    return 0;
}

void BatteryStatsImpl::readHistory(const Parcel& in, bool andOldHistory) {
    GLOGENTRY();

    // Customize +++
    if (DEBUG_ON) {
        GLOGD("readHistory Start");
    }

    if (DEBUG_ON) {
        sp<StringBuilder> sb = new StringBuilder(128);
        sb->append("****************** OLD mHistoryFileTime: ");
        TimeUtils::formatDuration(mHistoryFileTime, sb);
        GLOGI("%s", sb->toString()->string());
    }

    mHistoryFileTime = in.readInt64();

    if (DEBUG_ON) {
        sp<StringBuilder> sb = new StringBuilder(128);
        sb->append("****************** New mHistoryFileTime: ");
        TimeUtils::formatDuration(mHistoryFileTime, sb);
        GLOGI("%s", sb->toString()->string());
    }

    if (DEBUG_ON) {
        GLOGD("Old mHistoryShutdownTime: %lld", mHistoryShutdownTime);
    }

    mHistoryShutdownTime = in.readInt64();

    if (DEBUG_ON) {
        GLOGD("New mHistoryShutdownTime: %lld", mHistoryShutdownTime);
    }

    int32_t tmpSize = in.readInt32();

    if (DEBUG_ON) {
        GLOGD("READING open size: %d", tmpSize);
    }

    mHistoryStartTimeList->clear();

    if (tmpSize <= 10000) {
        for (int32_t i = 0; i < tmpSize; i++) {
            mHistoryStartTimeList->add(new LongLong(in.readInt64()));
        }
    } else {
        GLOGW("File corrupt: the power off times not correct");
    }
    // Customize ---

    const int64_t historyBaseTime = in.readInt64();
    mHistoryBuffer.setDataSize(0);
    mHistoryBuffer.setDataPosition(0);
    int32_t bufSize = in.readInt32();
    int32_t curPos = in.dataPosition();

    if (bufSize >= (MAX_MAX_HISTORY_BUFFER*3)) {
        GLOGW("%s File corrupt: history data buffer too large %d", LOG_TAG, bufSize);
    } else if ((bufSize&~3) != bufSize) {
        GLOGW("%s File corrupt: history data buffer not aligned %d", LOG_TAG, bufSize);
    } else {
        if (DEBUG_HISTORY) {
        GLOGI("%s ***************** READING NEW HISTORY: %d bytes at %d", LOG_TAG, bufSize, curPos);
    }
        mHistoryBuffer.appendFrom(const_cast<Parcel*>(&in), curPos, bufSize);
        in.setDataPosition(curPos + bufSize);
    }

    if (andOldHistory) {
        readOldHistory(in);
    }

    if (DEBUG_HISTORY) {
        sp<StringBuilder> sb = new StringBuilder(128);
        sb->append("****************** OLD mHistoryBaseTime: ");
        TimeUtils::formatDuration(mHistoryBaseTime, sb);
        GLOGI("%s  %s", LOG_TAG, sb->toString()->string());
    }

    mHistoryBaseTime = historyBaseTime;

    if (DEBUG_HISTORY) {
        sp<StringBuilder> sb = new StringBuilder(128);
        sb->append("****************** NEW mHistoryBaseTime: ");
        TimeUtils::formatDuration(mHistoryBaseTime, sb);
        GLOGI("%s  %s", LOG_TAG, sb->toString()->string());
    }

    // We are just arbitrarily going to insert 1 minute from the sample of
    // the last run until samples in this run.
    if (mHistoryBaseTime > 0) {
        int64_t oldnow = elapsedRealtime();
        mHistoryBaseTime = (mHistoryBaseTime - oldnow) + 60 * 1000;

        if (DEBUG_HISTORY) {
            sp<StringBuilder> sb = new StringBuilder(128);
            sb->append("****************** ADJUSTED mHistoryBaseTime: ");
            TimeUtils::formatDuration(mHistoryBaseTime, sb);
            GLOGI("%s  %s", LOG_TAG, sb->toString()->string());
        }
    }

    // Customize +++
    if (DEBUG_ON) {
        GLOGD("readHistory End");
    }
    // Customize ---
}

void BatteryStatsImpl::readOldHistory(const Parcel& in) {
    GLOGENTRY();

    if (!USE_OLD_HISTORY) {
        return;
    }

    mHistory = mHistoryEnd = mHistoryCache = NULL;
    int64_t time;

    while (in.dataAvail() > 0 && (time = in.readInt64()) >= 0) {
        sp<HistoryItem> rec = new HistoryItem(time, in);
        addHistoryRecordLocked(rec);
    }
}

void BatteryStatsImpl::writeHistory(Parcel* out, bool andOldHistory) {
    GLOGENTRY();

    // Customize +++
    if (DEBUG_ON) {
        GLOGD("writeHistory Start");
    }
    // Customize ---

    if (DEBUG_HISTORY) {
        sp<StringBuilder> sb = new StringBuilder(128);
        sb->append("****************** WRITING mHistoryBaseTime: ");
        TimeUtils::formatDuration(mHistoryBaseTime, sb);
        sb->append(" mLastHistoryTime: ");
        TimeUtils::formatDuration(mLastHistoryTime, sb);
        GLOGI("%s  %s", LOG_TAG, sb->toString()->string());
    }

    // Customize +++
    if (DEBUG_ON) {
        sp<StringBuilder> sb = new StringBuilder(128);
        sb->append("****************** WRITING mHistoryFileTime: ");
        TimeUtils::formatDuration(mHistoryFileTime, sb);
        GLOGI("%s", sb->toString()->string());
    }
    out->writeInt64(mHistoryFileTime);

    mHistoryShutdownTime = System::currentTimeMillis();

    if (DEBUG_ON) {
        GLOGD("WRITING mHistoryShutdownTime: %lld", mHistoryShutdownTime);
    }

    out->writeInt64(mHistoryShutdownTime);

    int32_t tmpSize = mHistoryStartTimeList->size();

    out->writeInt32(tmpSize);

    for (int32_t i = 0; i < tmpSize; i++) {
        out->writeInt64(mHistoryStartTimeList->get(i)->longlongValue());
    }

    if (DEBUG_ON) {
        GLOGD("WRITING open size: %d", tmpSize);
    }
    // Customize ---

    out->writeInt64(mHistoryBaseTime + mLastHistoryTime);
    out->writeInt32(mHistoryBuffer.dataSize());

    if (DEBUG_HISTORY) {
        GLOGI("%s ***************** WRITING HISTORY: %d bytes at %d", LOG_TAG, mHistoryBuffer.dataSize(), out->dataPosition());
    }

    out->appendFrom(&mHistoryBuffer, 0, mHistoryBuffer.dataSize());

    if (andOldHistory) {
        writeOldHistory(out);
    }

    // Customize +++
    if (DEBUG_ON) {
        GLOGD("writeHistory End");
    }
    // Customize ---
}

void BatteryStatsImpl::writeOldHistory(Parcel* out) {  // NOLINT
    GLOGENTRY();

    if (!USE_OLD_HISTORY) {
        return;
    }

    sp<HistoryItem> rec = mHistory;

    while (rec != NULL) {
        if (rec->time >= 0) rec->writeToParcel(out, 0);

        rec = rec->next;
    }

    out->writeInt64(-1);
}

void BatteryStatsImpl::readSummaryFromParcel(Parcel& in) {  // NOLINT
    GLOGENTRY();

    const int32_t version = in.readInt32();

    if (version != VERSION) {
        GLOGW("BatteryStatsImpl, readFromParcel: version got %d, expected %d; erasing old stats", version, VERSION);
        return;
    }

    // Customize +++
    if (DEBUG_ON) {
        GLOGD("readSummaryFromParcel Start");
    }
    // Customize ---

    readHistory(in, true);
    mStartCount = in.readInt32();
    mBatteryUptime = in.readInt64();
    mBatteryRealtime = in.readInt64();
    mUptime = in.readInt64();
    mRealtime = in.readInt64();
    mDischargeUnplugLevel = in.readInt32();
    mDischargeCurrentLevel = in.readInt32();
    mLowDischargeAmountSinceCharge = in.readInt32();
    mHighDischargeAmountSinceCharge = in.readInt32();
    mDischargeAmountScreenOnSinceCharge = in.readInt32();
    mDischargeAmountScreenOffSinceCharge = in.readInt32();
    mStartCount++;
    mScreenOn = false;
    mScreenOnTimer->readSummaryFromParcelLocked(in);

#ifdef BACKLIGHT_ENABLE
    for (int32_t i = 0; i < NUM_SCREEN_BRIGHTNESS_BINS; i++) {
        (*mScreenBrightnessTimer)[i]->readSummaryFromParcelLocked(in);
    }
#endif  // BACKLIGHT_ENABLE

    mInputEventCounter->readSummaryFromParcelLocked(in);

    mPhoneOn = false;
    mPhoneOnTimer->readSummaryFromParcelLocked(in);

    for (int32_t i = 0; i < SignalStrength::NUM_SIGNAL_STRENGTH_BINS; i++) {
        (*mPhoneSignalStrengthsTimer)[i]->readSummaryFromParcelLocked(in);
    }

    mPhoneSignalScanningTimer->readSummaryFromParcelLocked(in);

    for (int32_t i = 0; i < NUM_DATA_CONNECTION_TYPES; i++) {
        (*mPhoneDataConnectionsTimer)[i]->readSummaryFromParcelLocked(in);
    }

#ifdef WIFI_ENABLE
    mWifiOn = false;
    mWifiOnTimer->readSummaryFromParcelLocked(in);
    mGlobalWifiRunning = false;
    mGlobalWifiRunningTimer->readSummaryFromParcelLocked(in);
#endif  // WIFI_ENABLE

    mBluetoothOn = false;
    mBluetoothOnTimer->readSummaryFromParcelLocked(in);
    int32_t NKW = in.readInt32();

    if (NKW > 10000) {
        GLOGW("File corrupt: too many kernel wake locks %d", NKW);
        return;
    }

    for (int32_t ikw = 0; ikw < NKW; ikw++) {
        if (in.readInt32() != 0) {
            sp<String> kwltName = in.readString();
            getKernelWakelockTimerLocked(kwltName)->readSummaryFromParcelLocked(in);
        }
    }

    // Customize +++
    int32_t sNumSpeedStepsTemp = in.readInt32();
    if (sNumSpeedStepsTemp > 100) {
        GLOGW("File corrupt: too many cpu speed steps %d", sNumSpeedStepsTemp);
        return;
    }
    sNumSpeedSteps = sNumSpeedStepsTemp;

    int32_t sNumGpuSpeedStepsTemp = in.readInt32();
    if (sNumGpuSpeedStepsTemp > 100) {
        GLOGW("File corrupt: too many gpu speed steps %d", sNumGpuSpeedStepsTemp);
        return;
    }
    sNumGpuSpeedSteps = sNumGpuSpeedStepsTemp;

    if (DEBUG_ON) {
        GLOGD("Gpu level num = %d", sNumGpuSpeedSteps);
    }
    // Customize ---

    const int32_t NU = in.readInt32();

    if (NU > 10000) {
        GLOGW("File corrupt: too many uids %d", NU);
        return;
    }

    for (int32_t iu = 0; iu < NU; iu++) {
        int32_t uid = in.readInt32();

        sp<Uid> u = new Uid(this, uid);

        {
            GLOGAUTOMUTEX(_l, mThisLock);
            mUidStats.add(uid, u);
        }

#ifdef WIFI_ENABLE
        u->mWifiRunning = false;

        if (in.readInt32() != 0) {
            u->mWifiRunningTimer->readSummaryFromParcelLocked(in);
        }

        u->mFullWifiLockOut = false;

        if (in.readInt32() != 0) {
            u->mFullWifiLockTimer->readSummaryFromParcelLocked(in);
        }

        u->mScanWifiLockOut = false;

        if (in.readInt32() != 0) {
            u->mScanWifiLockTimer->readSummaryFromParcelLocked(in);
        }

        u->mWifiMulticastEnabled = false;

        if (in.readInt32() != 0) {
            u->mWifiMulticastTimer->readSummaryFromParcelLocked(in);
        }
#endif  // WIFI_ENABLE

        u->mAudioTurnedOn = false;

        if (in.readInt32() != 0) {
            u->mAudioTurnedOnTimer->readSummaryFromParcelLocked(in);
        }

        u->mVideoTurnedOn = false;

        if (in.readInt32() != 0) {
            u->mVideoTurnedOnTimer->readSummaryFromParcelLocked(in);
        }

        // Customize +++
        u->mGpuTurnedOn = false;
        if (in.readInt32() != 0) {
            u->mGpuTurnedOnTimer->readSummaryFromParcelLocked(in);
        }

        u->mDisplayTurnedOn = false;
        if (in.readInt32() != 0) {
            u->mDisplayTurnedOnTimer->readSummaryFromParcelLocked(in);
        }

        if (in.readInt32() != 0) {
            for (int i = 0; i < NUM_SCREEN_BRIGHTNESS_BINS; i++) {
                u->mDisplayBrightnessTimer[i]->readSummaryFromParcelLocked(in);
            }
        }
        // Customize ---

        if (in.readInt32() != 0) {
            if (u->mUserActivityCounters == NULL) {
                u->initUserActivityLocked();
            }

            for (int32_t i = 0; i < Uid::NUM_USER_ACTIVITY_TYPES; i++) {
                (*u->mUserActivityCounters)[i]->readSummaryFromParcelLocked(in);
            }
        }

        int32_t NW = in.readInt32();

        if (NW > 100) {
            GLOGW("File corrupt: too many wake locks %d", NW);
            return;
        }

        for (int32_t iw = 0; iw < NW; iw++) {
            sp<String> wlName = in.readString();

            if (in.readInt32() != 0) {
                u->getWakeTimerLocked(wlName, WAKE_TYPE_FULL)->readSummaryFromParcelLocked(in);
            }

            if (in.readInt32() != 0) {
                u->getWakeTimerLocked(wlName, WAKE_TYPE_PARTIAL)->readSummaryFromParcelLocked(in);
            }

            if (in.readInt32() != 0) {
                u->getWakeTimerLocked(wlName, WAKE_TYPE_WINDOW)->readSummaryFromParcelLocked(in);
            }
        }

        int32_t NP = in.readInt32();
#ifdef SENSOR_ENABLE
        if (NP > 1000) {
            GLOGW("File corrupt: too many sensors %d", NP);
            return;
        }


        for (int32_t is = 0; is < NP; is++) {
            int32_t seNumber = in.readInt32();

            if (in.readInt32() != 0) {
                u->getSensorTimerLocked(seNumber, true)->readSummaryFromParcelLocked(in);
            }
        }
#endif  // SENSOR_ENABLE
        NP = in.readInt32();

        if (NP > 1000) {
            GLOGW("File corrupt: too many processes %d", NP);
            return;
        }

        for (int32_t ip = 0; ip < NP; ip++) {
            sp<String> procName = in.readString();
            sp<Uid::Proc> p = safe_cast<BatteryStatsImpl::Uid::Proc*>(u->getProcessStatsLocked(procName).get());  // ??????
            p->mUserTime = p->mLoadedUserTime = in.readInt64();
            p->mSystemTime = p->mLoadedSystemTime = in.readInt64();
            p->mStarts = p->mLoadedStarts = in.readInt32();
            int32_t NSB = in.readInt32();

            if (NSB > 100) {
                GLOGW("File corrupt: too many speed bins %d", NSB);
                return;
            }

            p->mSpeedBins = new Blob<sp<SamplingCounter> >(NSB);

            for (int32_t i = 0; i < NSB; i++) {
                if (in.readInt32() != 0) {
                    (*p->mSpeedBins)[i] = new SamplingCounter(this, mUnpluggables, 1);
                    (*p->mSpeedBins)[i]->readSummaryFromParcelLocked(in);
                }
            }

            if (!p->readExcessivePowerFromParcelLocked(in)) {
                return;
            }
        }

        NP = in.readInt32();

        if (NP > 10000) {
            GLOGW("File corrupt: too many packages %d", NP);
            return;
        }

        for (int32_t ip = 0; ip < NP; ip++) {
            sp<String> pkgName = in.readString();
            sp<Uid::Pkg> p = u->getPackageStatsLocked(pkgName);
            p->mWakeups = p->mLoadedWakeups = in.readInt32();
            const int32_t NS = in.readInt32();

            if (NS > 1000) {
                GLOGW("File corrupt: too many services %d", NS);
                return;
            }

            for (int32_t is = 0; is < NS; is++) {
                sp<String> servName = in.readString();
                sp<Uid::Pkg::Serv> s = u->getServiceStatsLocked(pkgName, servName);
                s->mStartTime = s->mLoadedStartTime = in.readInt64();
                s->mStarts = s->mLoadedStarts = in.readInt32();
                s->mLaunches = s->mLoadedLaunches = in.readInt32();
            }
        }

        u->mLoadedTcpBytesReceived = in.readInt64();
        u->mLoadedTcpBytesSent = in.readInt64();
    }

    // Customize +++
    if (DEBUG_ON) {
        GLOGD("readSummaryFromParcel End");
    }
    // Customize ---
}

void BatteryStatsImpl::writeSummaryToParcel(Parcel* out) {
    GLOGENTRY();

    // Customize +++
    if (DEBUG_ON) {
        GLOGD("writeSummaryToParcel Start");
    }
    // Customize ---

    // Need to update with current kernel wake lock counts.
    updateKernelWakelocksLocked();
    const int64_t NOW_SYS = uptimeMillis() * 1000;
    const int64_t NOWREAL_SYS = elapsedRealtime() * 1000;
    const int64_t NOW = getBatteryUptimeLocked(NOW_SYS);
    const int64_t NOWREAL = getBatteryRealtimeLocked(NOWREAL_SYS);
    out->writeInt32(VERSION);
    writeHistory(out, true);
    out->writeInt32(mStartCount);
    out->writeInt64(computeBatteryUptime(NOW_SYS, STATS_SINCE_CHARGED));
    out->writeInt64(computeBatteryRealtime(NOWREAL_SYS, STATS_SINCE_CHARGED));
    out->writeInt64(computeUptime(NOW_SYS, STATS_SINCE_CHARGED));
    out->writeInt64(computeRealtime(NOWREAL_SYS, STATS_SINCE_CHARGED));
    out->writeInt32(mDischargeUnplugLevel);
    out->writeInt32(mDischargeCurrentLevel);
    out->writeInt32(getLowDischargeAmountSinceCharge());
    out->writeInt32(getHighDischargeAmountSinceCharge());
    out->writeInt32(getDischargeAmountScreenOnSinceCharge());
    out->writeInt32(getDischargeAmountScreenOffSinceCharge());
    mScreenOnTimer->writeSummaryFromParcelLocked(out, NOWREAL);

#ifdef BACKLIGHT_ENABLE
    for (int32_t i = 0; i < NUM_SCREEN_BRIGHTNESS_BINS; i++) {
        (*mScreenBrightnessTimer)[i]->writeSummaryFromParcelLocked(out, NOWREAL);
    }
#endif  // BACKLIGHT_ENABLE

    mInputEventCounter->writeSummaryFromParcelLocked(out);

    mPhoneOnTimer->writeSummaryFromParcelLocked(out, NOWREAL);

    for (int32_t i = 0; i < SignalStrength::NUM_SIGNAL_STRENGTH_BINS; i++) {
        (*mPhoneSignalStrengthsTimer)[i]->writeSummaryFromParcelLocked(out, NOWREAL);
    }

    mPhoneSignalScanningTimer->writeSummaryFromParcelLocked(out, NOWREAL);

    for (int32_t i = 0; i < NUM_DATA_CONNECTION_TYPES; i++) {
        mPhoneDataConnectionsTimer[i]->writeSummaryFromParcelLocked(out, NOWREAL);
    }

#ifdef WIFI_ENABLE
    mWifiOnTimer->writeSummaryFromParcelLocked(out, NOWREAL);
    mGlobalWifiRunningTimer->writeSummaryFromParcelLocked(out, NOWREAL);
#endif  // WIFI_ENABLE

    mBluetoothOnTimer->writeSummaryFromParcelLocked(out, NOWREAL);

    const int32_t NKW = mKernelWakelockStats.size();
    out->writeInt32(NKW);

    for (int32_t i = 0; i < NKW; i++) {
        sp<SamplingTimer> kwlt = mKernelWakelockStats.valueAt(i);
        if (kwlt != 0) {
            out->writeInt32(1);
            out->writeString(mKernelWakelockStats.keyAt(i));
            kwlt->writeSummaryFromParcelLocked(out, NOWREAL);
        } else {
            out->writeInt32(0);
        }
    }


    out->writeInt32(sNumSpeedSteps);

    // Customize +++
    out->writeInt32(sNumGpuSpeedSteps);
    if (DEBUG_ON) {
        GLOGD("write Gpu level num = %d", sNumGpuSpeedSteps);
    }
    // Customize ---

    {
        GLOGAUTOMUTEX(_l, mThisLock);

        const int32_t NU = mUidStats.size();
        out->writeInt32(NU);

        for (int32_t iu = 0; iu < NU; iu++) {
            out->writeInt32(mUidStats.keyAt(iu));
            sp<Uid> u = safe_cast<BatteryStatsImpl::Uid*>(mUidStats.valueAt(iu));

    #ifdef WIFI_ENABLE
            if (u->mWifiRunningTimer != NULL) {
                out->writeInt32(1);
                u->mWifiRunningTimer->writeSummaryFromParcelLocked(out, NOWREAL);
            } else {
                out->writeInt32(0);
            }

            if (u->mFullWifiLockTimer != NULL) {
                out->writeInt32(1);
                u->mFullWifiLockTimer->writeSummaryFromParcelLocked(out, NOWREAL);
            } else {
                out->writeInt32(0);
            }

            if (u->mScanWifiLockTimer != NULL) {
                out->writeInt32(1);
                u->mScanWifiLockTimer->writeSummaryFromParcelLocked(out, NOWREAL);
            } else {
                out->writeInt32(0);
            }

            if (u->mWifiMulticastTimer != NULL) {
                out->writeInt32(1);
                u->mWifiMulticastTimer->writeSummaryFromParcelLocked(out, NOWREAL);
            } else {
                out->writeInt32(0);
            }
    #endif  // WIFI_ENABLE

            if (u->mAudioTurnedOnTimer != NULL) {
                out->writeInt32(1);
                u->mAudioTurnedOnTimer->writeSummaryFromParcelLocked(out, NOWREAL);
            } else {
                out->writeInt32(0);
            }

            if (u->mVideoTurnedOnTimer != NULL) {
                out->writeInt32(1);
                u->mVideoTurnedOnTimer->writeSummaryFromParcelLocked(out, NOWREAL);
            } else {
                out->writeInt32(0);
            }

            // Customize +++
            if (u->mGpuTurnedOnTimer != NULL) {
                out->writeInt32(1);
                u->mGpuTurnedOnTimer->writeSummaryFromParcelLocked(out, NOWREAL);
            } else {
                out->writeInt32(0);
            }

            if (u->mDisplayTurnedOnTimer != NULL) {
                out->writeInt32(1);
                u->mDisplayTurnedOnTimer->writeSummaryFromParcelLocked(out, NOWREAL);
            } else {
                out->writeInt32(0);
            }

            if (u->mDisplayBrightnessTimer == NULL) {
                out->writeInt32(0);
            } else {
                out->writeInt32(1);
                for (int32_t i = 0; i < NUM_SCREEN_BRIGHTNESS_BINS; i++) {
                    u->mDisplayBrightnessTimer[i]->writeSummaryFromParcelLocked(out, NOWREAL);
                }
            }
            // Customize ---

            if (u->mUserActivityCounters == NULL) {
                out->writeInt32(0);
            } else {
                out->writeInt32(1);

                for (int32_t i = 0; i < Uid::NUM_USER_ACTIVITY_TYPES; i++) {
                    (*u->mUserActivityCounters)[i]->writeSummaryFromParcelLocked(out);
                }
            }

            int32_t NW = u->mWakelockStats.size();
            out->writeInt32(NW);

            if (NW > 0) {
                for (int32_t i = 0; i < NW; i++) {
                    out->writeString(u->mWakelockStats.keyAt(i));
                    sp<Uid::Wakelock> wl = safe_cast<BatteryStatsImpl::Uid::Wakelock*>(u->mWakelockStats.valueAt(i));

                    if (wl->mTimerFull != NULL) {
                        out->writeInt32(1);
                        wl->mTimerFull->writeSummaryFromParcelLocked(out, NOWREAL);
                    } else {
                        out->writeInt32(0);
                    }

                    if (wl->mTimerPartial != NULL) {
                        out->writeInt32(1);
                        wl->mTimerPartial->writeSummaryFromParcelLocked(out, NOWREAL);
                    } else {
                        out->writeInt32(0);
                    }

                    if (wl->mTimerWindow != NULL) {
                        out->writeInt32(1);
                        wl->mTimerWindow->writeSummaryFromParcelLocked(out, NOWREAL);
                    } else {
                        out->writeInt32(0);
                    }
                }
            }
    #ifdef SENSOR_ENABLE
            int32_t NSE = u->mSensorStats.size();
            out->writeInt32(NSE);

            if (NSE > 0) {
                for (int32_t i = 0; i < NSE; i++) {
                    out->writeInt32(u->mSensorStats.keyAt(i));
                    sp<Uid::Sensor> se = safe_cast<BatteryStatsImpl::Uid::Sensor*>(u->mSensorStats.valueAt(i));
                    if (se->mTimer != NULL) {
                        out->writeInt32(1);
                        se->mTimer->writeSummaryFromParcelLocked(out, NOWREAL);
                    } else {
                        out->writeInt32(0);
                    }
                }
            }
    #endif  // SENSOR_ENABLE

            int32_t NP = u->mProcessStats.size();
            out->writeInt32(NP);

            if (NP > 0) {
                for (int32_t i = 0; i < NP; i++) {
                    out->writeString(u->mProcessStats.keyAt(i));
                    sp<Uid::Proc> ps = safe_cast<BatteryStatsImpl::Uid::Proc*>(u->mProcessStats.valueAt(i));
                    out->writeInt64(ps->mUserTime);
                    out->writeInt64(ps->mSystemTime);
                    out->writeInt32(ps->mStarts);
                    const int32_t N = ps->mSpeedBins->length();
                    out->writeInt32(N);

                    for (int32_t i = 0; i < N; i++) {
                        if ((*ps->mSpeedBins)[i] != NULL) {
                            out->writeInt32(1);
                            (*ps->mSpeedBins)[i]->writeSummaryFromParcelLocked(out);
                        } else {
                            out->writeInt32(0);
                        }
                    }

                    ps->writeExcessivePowerToParcelLocked(out);
                }
            }

            NP = u->mPackageStats.size();
            out->writeInt32(NP);

            if (NP > 0) {
                for (int32_t i = 0; i < NP; i++) {
                    out->writeString(u->mPackageStats.keyAt(i));
                    sp<Uid::Pkg> ps = safe_cast<BatteryStatsImpl::Uid::Pkg*>(u->mPackageStats.valueAt(i));
                    out->writeInt32(ps->mWakeups);
                    const int32_t NS = ps->mServiceStats.size();
                    out->writeInt32(NS);

                    if (NS > 0) {
                        for (int32_t j = 0; j < NS; j++) {
                            out->writeString(ps->mServiceStats.keyAt(j));
                            sp<BatteryStatsImpl::Uid::Pkg::Serv> ss = safe_cast<BatteryStatsImpl::Uid::Pkg::Serv*>(ps->mServiceStats.valueAt(j));
                            int64_t time = ss->getStartTimeToNowLocked(NOW);
                            out->writeInt64(time);
                            out->writeInt32(ss->mStarts);
                            out->writeInt32(ss->mLaunches);
                        }
                    }
                }
            }

            out->writeInt64(u->getTcpBytesReceived(STATS_SINCE_CHARGED));
            out->writeInt64(u->getTcpBytesSent(STATS_SINCE_CHARGED));
        }
    }

    // Customize +++
    if (DEBUG_ON) {
        GLOGD("writeSummaryToParcel End");
    }
    // Customize ---
}

void BatteryStatsImpl::readFromParcel(const Parcel& in) {
    GLOGENTRY();

    readFromParcelLocked(in);
}

void BatteryStatsImpl::readFromParcelLocked(const Parcel& in) {
    GLOGENTRY();

    int32_t magic = in.readInt32();

    // Customize +++
    if (DEBUG_ON) {
        GLOGD("readFromParcelLocked Start");
    }
    // Customize ---

    if (magic != MAGIC) {
        // throw new ParcelFormatException("Bad magic number");
    }

    readHistory(in, false);
    mStartCount = in.readInt32();
    mBatteryUptime = in.readInt64();
    mBatteryLastUptime = 0;
    mBatteryRealtime = in.readInt64();
    mBatteryLastRealtime = 0;
    mScreenOn = false;
    mScreenOnTimer = new StopwatchTimer(this, NULL, -1, NULL, mUnpluggables, in);

#ifdef BACKLIGHT_ENABLE
    for (int32_t i = 0; i < NUM_SCREEN_BRIGHTNESS_BINS; i++) {
        (*mScreenBrightnessTimer)[i] = new StopwatchTimer(this, NULL, -100 - i,
                NULL, mUnpluggables, in);
    }
#endif  // BACKLIGHT_ENABLE

    mInputEventCounter = new Counter(this, mUnpluggables, in, 0);

    mPhoneOn = false;
    mPhoneOnTimer = new StopwatchTimer(this, NULL, -2, NULL, mUnpluggables, in);

    for (int32_t i = 0; i < SignalStrength::NUM_SIGNAL_STRENGTH_BINS; i++) {
        (*mPhoneSignalStrengthsTimer)[i] = new StopwatchTimer(this, NULL, -200 - i,
                NULL, mUnpluggables, in);
    }

    mPhoneSignalScanningTimer = new StopwatchTimer(this, NULL, -200 + 1, NULL, mUnpluggables, in);

    for (int32_t i = 0; i < NUM_DATA_CONNECTION_TYPES; i++) {
        mPhoneDataConnectionsTimer[i] = new StopwatchTimer(this, NULL, -300 - i,
                NULL, mUnpluggables, in);
    }

#ifdef WIFI_ENABLE
    mWifiOn = false;
    mWifiOnTimer = new StopwatchTimer(this, NULL, -2, NULL, mUnpluggables, in);
    mGlobalWifiRunning = false;
    mGlobalWifiRunningTimer = new StopwatchTimer(this, NULL, -2, NULL, mUnpluggables, in);
#endif  // WIFI_ENABLE

    mBluetoothOn = false;
    mBluetoothOnTimer = new StopwatchTimer(this, NULL, -2, NULL, mUnpluggables, in);

    mUptime = in.readInt64();
    mUptimeStart = in.readInt64();
    mLastUptime = 0;
    mRealtime = in.readInt64();
    mRealtimeStart = in.readInt64();
    mLastRealtime = 0;
    mOnBattery = in.readInt32() != 0;
    mOnBatteryInternal = false;  // we are no longer really running.
    mTrackBatteryPastUptime = in.readInt64();
    mTrackBatteryUptimeStart = in.readInt64();
    mTrackBatteryPastRealtime = in.readInt64();
    mTrackBatteryRealtimeStart = in.readInt64();
    mUnpluggedBatteryUptime = in.readInt64();
    mUnpluggedBatteryRealtime = in.readInt64();
    mDischargeUnplugLevel = in.readInt32();
    mDischargeCurrentLevel = in.readInt32();
    mLowDischargeAmountSinceCharge = in.readInt32();
    mHighDischargeAmountSinceCharge = in.readInt32();
    mDischargeAmountScreenOn = in.readInt32();
    mDischargeAmountScreenOnSinceCharge = in.readInt32();
    mDischargeAmountScreenOff = in.readInt32();
    mDischargeAmountScreenOffSinceCharge = in.readInt32();
    mLastWriteTime = in.readInt64();
    (*mMobileDataRx)[STATS_LAST] = in.readInt64();
    (*mMobileDataRx)[STATS_SINCE_UNPLUGGED] = -1;
    (*mMobileDataTx)[STATS_LAST] = in.readInt64();
    (*mMobileDataTx)[STATS_SINCE_UNPLUGGED] = -1;
    (*mTotalDataRx)[STATS_LAST] = in.readInt64();
    (*mTotalDataRx)[STATS_SINCE_UNPLUGGED] = -1;
    (*mTotalDataTx)[STATS_LAST] = in.readInt64();
    (*mTotalDataTx)[STATS_SINCE_UNPLUGGED] = -1;
    mRadioDataUptime = in.readInt64();
    mRadioDataStart = -1;

    mBluetoothPingCount = in.readInt32();
    mBluetoothPingStart = -1;

    mKernelWakelockStats.clear();
    int32_t NKW = in.readInt32();

    for (int32_t ikw = 0; ikw < NKW; ikw++) {
        if (in.readInt32() != 0) {
            sp<String> wakelockName = in.readString();
            in.readInt32();  // Extra 0/1 written by Timer.writeTimerToParcel
            sp<SamplingTimer> kwlt = new SamplingTimer(this, mUnpluggables, mOnBattery, in);
            mKernelWakelockStats.add(wakelockName, kwlt);
        }
    }

    mPartialTimers->clear();
    mFullTimers->clear();
    mWindowTimers->clear();
#ifdef WIFI_ENABLE
    mWifiRunningTimers->clear();
    mFullWifiLockTimers->clear();
    mScanWifiLockTimers->clear();
    mWifiMulticastTimers->clear();
#endif  // WIFI_ENABLE

    sNumSpeedSteps = in.readInt32();

    // Customize +++
    sNumGpuSpeedSteps = in.readInt32();
    // Customize ---

    int32_t numUids = in.readInt32();

    {
        GLOGAUTOMUTEX(_l, mThisLock);
        mUidStats.clear();
        for (int32_t i = 0; i < numUids; i++) {
            int32_t uid = in.readInt32();
            sp<Uid> u = new Uid(this, uid);
            u->readFromParcelLocked(mUnpluggables, in);
            mUidStats.add(uid, u);
        }
    }


    // Customize +++
    if (DEBUG_ON) {
        GLOGD("readFromParcelLocked End");
    }
    // Customize ---
}

void BatteryStatsImpl::writeToParcel(Parcel* out, int32_t flags) {  // NOLINT
    GLOGENTRY();

    writeToParcelLocked(out, true, flags);
}

void BatteryStatsImpl::writeToParcelWithoutUids(Parcel* out, int32_t flags) {  // NOLINT
    GLOGENTRY();

    writeToParcelLocked(out, false, flags);
}

void BatteryStatsImpl::writeToParcelLocked(Parcel* out, bool inclUids, int32_t flags) {  // NOLINT
    GLOGENTRY();

    // Customize +++
    if (DEBUG_ON) {
        GLOGD("writeToParcelLocked Start");
    }
    // Customize ---

    // Need to update with current kernel wake lock counts.
    updateKernelWakelocksLocked();
    const int64_t uSecUptime = uptimeMillis() * 1000;
    const int64_t uSecRealtime = elapsedRealtime() * 1000;
    const int64_t batteryUptime = getBatteryUptimeLocked(uSecUptime);
    const int64_t batteryRealtime = getBatteryRealtimeLocked(uSecRealtime);
    out->writeInt32(MAGIC);
    writeHistory(out, false);
    out->writeInt32(mStartCount);
    out->writeInt64(mBatteryUptime);
    out->writeInt64(mBatteryRealtime);
    mScreenOnTimer->writeToParcel(out, batteryRealtime);

#ifdef BACKLIGHT_ENABLE
    for (int32_t i = 0; i < BatteryStats::NUM_SCREEN_BRIGHTNESS_BINS; i++) {
        (*mScreenBrightnessTimer)[i]->writeToParcel(out, batteryRealtime);
    }
#endif  // BACKLIGHT_ENABLE

    mInputEventCounter->writeToParcel(out);

    mPhoneOnTimer->writeToParcel(out, batteryRealtime);

    for (int32_t i = 0; i < SignalStrength::NUM_SIGNAL_STRENGTH_BINS; i++) {
        (*mPhoneSignalStrengthsTimer)[i]->writeToParcel(out, batteryRealtime);
    }

    mPhoneSignalScanningTimer->writeToParcel(out, batteryRealtime);

    for (int32_t i = 0; i < BatteryStats::NUM_DATA_CONNECTION_TYPES; i++) {
        mPhoneDataConnectionsTimer[i]->writeToParcel(out, batteryRealtime);
    }

#ifdef WIFI_ENABLE
    mWifiOnTimer->writeToParcel(out, batteryRealtime);
    mGlobalWifiRunningTimer->writeToParcel(out, batteryRealtime);
#endif  // WIFI_ENABLE

    mBluetoothOnTimer->writeToParcel(out, batteryRealtime);
    out->writeInt64(mUptime);
    out->writeInt64(mUptimeStart);
    out->writeInt64(mRealtime);
    out->writeInt64(mRealtimeStart);
    out->writeInt32(mOnBattery ? 1 : 0);
    out->writeInt64(batteryUptime);
    out->writeInt64(mTrackBatteryUptimeStart);
    out->writeInt64(batteryRealtime);
    out->writeInt64(mTrackBatteryRealtimeStart);
    out->writeInt64(mUnpluggedBatteryUptime);
    out->writeInt64(mUnpluggedBatteryRealtime);
    out->writeInt32(mDischargeUnplugLevel);
    out->writeInt32(mDischargeCurrentLevel);
    out->writeInt32(mLowDischargeAmountSinceCharge);
    out->writeInt32(mHighDischargeAmountSinceCharge);
    out->writeInt32(mDischargeAmountScreenOn);
    out->writeInt32(mDischargeAmountScreenOnSinceCharge);
    out->writeInt32(mDischargeAmountScreenOff);
    out->writeInt32(mDischargeAmountScreenOffSinceCharge);
    out->writeInt64(mLastWriteTime);
    out->writeInt64(getMobileTcpBytesReceived(STATS_SINCE_UNPLUGGED));
    out->writeInt64(getMobileTcpBytesSent(STATS_SINCE_UNPLUGGED));
    out->writeInt64(getTotalTcpBytesReceived(STATS_SINCE_UNPLUGGED));
    out->writeInt64(getTotalTcpBytesSent(STATS_SINCE_UNPLUGGED));
    // Write radio uptime for data
    out->writeInt64(getRadioDataUptime());
    out->writeInt32(getBluetoothPingCount());
    if (inclUids) {
        const int32_t NKW = mKernelWakelockStats.size();
        out->writeInt32(NKW);
        for (int32_t i = 0; i < NKW; i++) {
            sp<SamplingTimer> kwlt = mKernelWakelockStats.valueAt(i);
            if (kwlt != NULL) {
            out->writeInt32(1);
            out->writeString(mKernelWakelockStats.keyAt(i));
            Timer::writeTimerToParcel(out, kwlt, batteryRealtime);
            } else {
                out->writeInt32(0);
            }
        }
    } else {
        out->writeInt32(0);
    }

    out->writeInt32(sNumSpeedSteps);

    // Customize +++
    out->writeInt32(sNumGpuSpeedSteps);
    // Customize ---

    if (inclUids) {
        {
            GLOGAUTOMUTEX(_l, mThisLock);

            int32_t size = mUidStats.size();
            out->writeInt32(size);

            for (int32_t i = 0; i < size; i++) {
                out->writeInt32(mUidStats.keyAt(i));
                sp<Uid> uid = safe_cast<BatteryStatsImpl::Uid*>(mUidStats.valueAt(i));
                uid->writeToParcelLocked(out, batteryRealtime);
            }
        }
    } else {
        out->writeInt32(0);
    }

    // Customize +++
    if (DEBUG_ON) {
        GLOGD("writeToParcelLocked End");
    }
    // Customize ---
}

// Customize +++
sp<String> BatteryStatsImpl::toTwoDigits(int64_t digit) {
    GLOGENTRY();

    if (digit >= 10) {
        return String("") + Integer::toHexString(digit);
    } else {
        return String("0") + Integer::toHexString(digit);
    }
}
// Customize ---

// Customize +++
sp<String> BatteryStatsImpl::toThreeDigits(int64_t digit) {
    GLOGENTRY();

    if (digit >= 100) {
        return String("") + Integer::toHexString(digit);
    } else if (digit >= 10) {
        return String("0") + Integer::toHexString(digit);
    } else {
        return String("00") + Integer::toHexString(digit);
    }
}
// Customize ---

// Customize +++
void BatteryStatsImpl::formatTimeRaw(const android::sp<StringBuilder>& out, int64_t seconds) {
    GLOGENTRY();

    int64_t days = seconds / (60 * 60 * 24);

    out->append(toTwoDigits(days));
    out->append("d ");

    int64_t used = days * 60 * 60 * 24;

    int64_t hours = (seconds - used) / (60 * 60);
    out->append(toTwoDigits(hours));
    out->append("h ");

    used += hours * 60 * 60;

    int64_t mins = (seconds - used) / 60;
    out->append(toTwoDigits(mins));
    out->append("m ");

    used += mins * 60;

    out->append(toTwoDigits(seconds - used));
    out->append("s ");
}
// Customize ---

// Customize +++
void BatteryStatsImpl::formatTimeMs(const android::sp<StringBuilder>& sb, int64_t time) {
    GLOGENTRY();

    int64_t sec = time / 1000;

    formatTimeRaw(sb, sec);
    sb->append(toThreeDigits(time - (sec * 1000)));
    sb->append("ms ");
}
// Customize ---

void BatteryStatsImpl::prepareForDumpLocked() {
    GLOGENTRY();

    // Need to retrieve current kernel wake lock stats before printing.
    updateKernelWakelocksLocked();
}

void BatteryStatsImpl::dumpLocked(int32_t fd, sp<PrintWriter>& pw) {  // NOLINT
    GLOGENTRY();

    if (fd != -1) {
        sp<StringWriter> sw = new StringWriter();
        sp<PrintWriter> pw_ = new PrintWriter(sp<Writer>(sw.get()));

        if (pw_ == NULL) {
            GLOGW("pw_ = NULL");
            return;
        }

        if (DEBUG) {
            sp<Printer> pr = new PrintWriterPrinter(pw_);
            pr->println(new String("*** Screen timer:"));
            mScreenOnTimer->logState(pr, new String("  "));

    #ifdef BACKLIGHT_ENABLE
            for (int32_t i = 0; i < BatteryStats::NUM_SCREEN_BRIGHTNESS_BINS; i++) {
                pr->println(String::format("*** Screen brightness # %d :", i));
                (*mScreenBrightnessTimer)[i]->logState(pr, new String("  "));
            }
    #endif  // BACKLIGHT_ENABLE

            pr->println(String::format("*** Input event counter:"));
            mInputEventCounter->logState(pr, new String("  "));

            pr->println(String::format("*** Phone timer:"));
            mPhoneOnTimer->logState(pr, new String("  "));

            for (int32_t i = 0; i < SignalStrength::NUM_SIGNAL_STRENGTH_BINS; i++) {
                pr->println(String::format("*** Signal strength # %d:", i));
                (*mPhoneSignalStrengthsTimer)[i]->logState(pr, new String("  "));
            }

            pr->println(String::format("*** Signal scanning :"));
            mPhoneSignalScanningTimer->logState(pr, new String("  "));

            for (int32_t i = 0; i < NUM_DATA_CONNECTION_TYPES; i++) {
                pr->println(String::format("*** Data connection type # %d :", i));
                mPhoneDataConnectionsTimer[i]->logState(pr, new String("  "));
            }

            pr->println(String::format("*** Wifi timer:"));
    #ifdef WIFI_ENABLE
            mWifiOnTimer->logState(pr, new String("  "));
    #endif  // WIFI_ENABLE
            pr->println(String::format("*** WifiRunning timer:"));
    #ifdef WIFI_ENABLE
            mGlobalWifiRunningTimer->logState(pr, new String("  "));
    #endif  // WIFI_ENABLE
            pr->println(String::format("*** Bluetooth timer:"));
            mBluetoothOnTimer->logState(pr, new String("  "));
            pr->println(String::format("*** Mobile ifaces:"));
            pr->println(mMobileIfaces->toString());
        }

        if (sw != NULL && sw->toString() != NULL) {
            write(fd, sw->toString()->string(), sw->toString()->size());
            sw = new StringWriter();
            pw_ = new PrintWriter(sp<Writer>(sw.get()));
        }

        BatteryStats::dumpLocked(fd, pw_);

        // Customize +++
        if (1) {  // if (ProfileConfig.getProfileDebugWakelock()) {
                int64_t now = System::currentTimeMillis();

                pw_->println("=====================================================================================");
                pw_->println("Charging history");
                int32_t size = mBatteryStatusList->size();

                for (int32_t i = 0 ; i < size ; ++i) {
                    pw_->print("    ");
                    if (!mBatteryStatusList->get(i))
                        pw_->print("Plugged   : ");
                    else
                        pw_->print("Unplugged : ");

                    // TODO need Date
                    // pw_->println(new Date(mBatteryStatusTimeList->get(i)).toLocaleString());
                }

                pw_->println();
                pw_->println();
                pw_->println("Wake lock history");
                pw_->println();

                if (sw != NULL && sw->toString() != NULL) {
                    write(fd, sw->toString()->string(), sw->toString()->size());
                    sw = new StringWriter();
                    pw_ = new PrintWriter(sp<Writer>(sw.get()));
                }

                sp<StringBuilder> sb = new StringBuilder();

                sp<GArrayList<WakelockHistory> > mHistories = new GArrayList<WakelockHistory>();

                // synchronized (mWakelockHistory) {
                {
                    GLOGAUTOMUTEX(_l, mWakelockHistoryLock);

                    sp<GSet<String> > keySet = mWakelockHistory->keySet();
                    sp<GIterator<String> > it = keySet->iterator();

                    while (it != NULL && it->hasNext()) {
                        sp<String> name = it->next();
                        sp<WakelockHistory> wh = mWakelockHistory->get(name);

                        if (wh->mAcquireTimeList.size() == 0) {
                            continue;
                        }

                        wh->mTempSum = wh->mSum;

                        for (uint32_t i = 0 ; i < wh->mReleaseTimeList.size() ; ++i) {
                            int64_t diff = wh->mReleaseTimeList.itemAt(i) - wh->mAcquireTimeList.itemAt(i);
                            if (diff < 0)
                                diff = 0;
                            wh->mTempSum += diff;
                        }

                        if (wh->mAcquireTimeList.size() > wh->mReleaseTimeList.size()) {
                            int64_t diff = now - wh->mAcquireTimeList.itemAt(wh->mAcquireTimeList.size() - 1);
                            if (diff < 0)
                                diff = 0;
                            wh->mTempSum += diff;
                        }

                        mHistories->add(wh);
                    }

                    // TODO GCollections
                    // Collections->sort(mHistories, sWakelockHistoryComparator);

                    for (int32_t k = 0; k < mHistories->size(); ++k) {
                        sp<BatteryStatsImpl::WakelockHistory> wh = mHistories->get(k);

                        pw_->print(wh->mName);
                        sb->setLength(0);
                        pw_->print(" (");

                        for (int32_t i = 0 ; i < wh->mLockTypes->size() ; ++i) {
                            int32_t type = wh->mLockTypes->get(i)->intValue();
                            switch (type) {
                            case WAKE_TYPE_PARTIAL:
                                pw_->print("PARTIAL");
                                break;
                            case WAKE_TYPE_FULL:
                                pw_->print("FULL");
                                break;
                            case WAKE_TYPE_WINDOW:
                                pw_->print("WINDOW");
                                break;
                            default:
                                pw_->print("UNKNOWN");
                            }

                            if (i != wh->mLockTypes->size() - 1)
                                pw_->print(", ");
                        }

                        pw_->print(") : ");
                        sb->setLength(0);
                        BatteryStats::formatTimeMs(sb, wh->mTempSum);
                        pw_->println(sb->toString());

                        for (uint32_t i = 0 ; i < wh->mReleaseTimeList.size() ; ++i) {
                            int64_t diff = wh->mReleaseTimeList.itemAt(i) - wh->mAcquireTimeList.itemAt(i);
                            sb->setLength(0);
                            BatteryStats::formatTimeMs(sb, diff);

                            #if 0
                            pw_->println("    " + sb->toString() + "(" +
                                    new Date(wh->mAcquireTimeList->get(i))->toLocaleString() + " ~ " +
                                    new Date(wh->mReleaseTimeList->get(i))->toLocaleString() + ")");
                            #endif  // TODO date
                        }

                        if (wh->mAcquireTimeList.size() > wh->mReleaseTimeList.size()) {
                            int64_t diff = now - wh->mAcquireTimeList.itemAt(wh->mAcquireTimeList.size() - 1);
                            sb->setLength(0);
                            BatteryStats::formatTimeMs(sb, diff);

                            #if 0
                            pw_->println("    " + sb->toString() + "(" +
                                    new Date(wh->mAcquireTimeList->get(wh->mAcquireTimeList->size() - 1))->toLocaleString() +
                                    " ~ current" + ")");
                            #endif  // TODO date
                        }

                        pw_->println();

                        if ((k % 10) == 0) {
                            if (sw != NULL && sw->toString() != NULL) {
                                write(fd, sw->toString()->string(), sw->toString()->size());
                                sw = new StringWriter();
                                pw_ = new PrintWriter(sp<Writer>(sw.get()));
                            }
                        }
                    }
                }
                pw_->println("=====================================================================================");
            }

            if (sw != NULL && sw->toString() != NULL) {
                write(fd, sw->toString()->string(), sw->toString()->size());
                sw = new StringWriter();
                pw_ = new PrintWriter(sp<Writer>(sw.get()));
            }
        // Customize ---
    } else {
        if (DEBUG) {
            sp<Printer> pr = new PrintWriterPrinter(pw);
            pr->println(new String("*** Screen timer:"));
            mScreenOnTimer->logState(pr, new String("  "));

    #ifdef BACKLIGHT_ENABLE
            for (int32_t i = 0; i < BatteryStats::NUM_SCREEN_BRIGHTNESS_BINS; i++) {
                pr->println(String::format("*** Screen brightness # %d :", i));
                (*mScreenBrightnessTimer)[i]->logState(pr, new String("  "));
            }
    #endif  // BACKLIGHT_ENABLE

            pr->println(String::format("*** Input event counter:"));
            mInputEventCounter->logState(pr, new String("  "));

            pr->println(String::format("*** Phone timer:"));
            mPhoneOnTimer->logState(pr, new String("  "));

            for (int32_t i = 0; i < SignalStrength::NUM_SIGNAL_STRENGTH_BINS; i++) {
                pr->println(String::format("*** Signal strength # %d:", i));
                (*mPhoneSignalStrengthsTimer)[i]->logState(pr, new String("  "));
            }

            pr->println(String::format("*** Signal scanning :"));
            mPhoneSignalScanningTimer->logState(pr, new String("  "));

            for (int32_t i = 0; i < NUM_DATA_CONNECTION_TYPES; i++) {
                pr->println(String::format("*** Data connection type # %d :", i));
                mPhoneDataConnectionsTimer[i]->logState(pr, new String("  "));
            }

            pr->println(String::format("*** Wifi timer:"));
    #ifdef WIFI_ENABLE
            mWifiOnTimer->logState(pr, new String("  "));
    #endif  // WIFI_ENABLE
            pr->println(String::format("*** WifiRunning timer:"));
    #ifdef WIFI_ENABLE
            mGlobalWifiRunningTimer->logState(pr, new String("  "));
    #endif  // WIFI_ENABLE
            pr->println(String::format("*** Bluetooth timer:"));
            mBluetoothOnTimer->logState(pr, new String("  "));
            pr->println(String::format("*** Mobile ifaces:"));
            pr->println(mMobileIfaces->toString());
        }

        BatteryStats::dumpLocked(fd, pw);

        // Customize +++
        if (1) {  // if (ProfileConfig.getProfileDebugWakelock()) {
                int64_t now = System::currentTimeMillis();

                pw->println("=====================================================================================");
                pw->println("Charging history");
                int32_t size = mBatteryStatusList->size();

                for (int32_t i = 0 ; i < size ; ++i) {
                    pw->print("    ");
                    if (!mBatteryStatusList->get(i))
                        pw->print("Plugged   : ");
                    else
                        pw->print("Unplugged : ");

                    // TODO need Date
                    // pw->println(new Date(mBatteryStatusTimeList->get(i)).toLocaleString());
                }

                pw->println();
                pw->println();
                pw->println("Wake lock history");
                pw->println();
                sp<StringBuilder> sb = new StringBuilder();

                sp<GArrayList<WakelockHistory> > mHistories = new GArrayList<WakelockHistory>();

                // synchronized (mWakelockHistory) {
                {
                    GLOGAUTOMUTEX(_l, mWakelockHistoryLock);

                    sp<GSet<String> > keySet = mWakelockHistory->keySet();
                    sp<GIterator<String> > it = keySet->iterator();

                    while (it != NULL && it->hasNext()) {
                        sp<String> name = it->next();
                        sp<WakelockHistory> wh = mWakelockHistory->get(name);

                        if (wh->mAcquireTimeList.size() == 0) {
                            continue;
                        }

                        wh->mTempSum = wh->mSum;

                        for (uint32_t i = 0 ; i < wh->mReleaseTimeList.size() ; ++i) {
                            int64_t diff = wh->mReleaseTimeList.itemAt(i) - wh->mAcquireTimeList.itemAt(i);
                            if (diff < 0)
                                diff = 0;
                            wh->mTempSum += diff;
                        }

                        if (wh->mAcquireTimeList.size() > wh->mReleaseTimeList.size()) {
                            int64_t diff = now - wh->mAcquireTimeList.itemAt(wh->mAcquireTimeList.size() - 1);
                            if (diff < 0)
                                diff = 0;
                            wh->mTempSum += diff;
                        }

                        mHistories->add(wh);
                    }

                    // TODO GCollections
                    // Collections->sort(mHistories, sWakelockHistoryComparator);

                    for (int32_t i = 0; i < mHistories->size(); ++i) {
                        sp<BatteryStatsImpl::WakelockHistory> wh = mHistories->get(i);

                        pw->print(wh->mName);
                        sb->setLength(0);
                        pw->print(" (");

                        for (int32_t i = 0 ; i < wh->mLockTypes->size() ; ++i) {
                            int32_t type = wh->mLockTypes->get(i)->intValue();
                            switch (type) {
                            case WAKE_TYPE_PARTIAL:
                                pw->print("PARTIAL");
                                break;
                            case WAKE_TYPE_FULL:
                                pw->print("FULL");
                                break;
                            case WAKE_TYPE_WINDOW:
                                pw->print("WINDOW");
                                break;
                            default:
                                pw->print("UNKNOWN");
                            }

                            if (i != wh->mLockTypes->size() - 1)
                                pw->print(", ");
                        }

                        pw->print(") : ");
                        sb->setLength(0);
                        BatteryStats::formatTimeMs(sb, wh->mTempSum);
                        pw->println(sb->toString());

                        for (uint32_t i = 0 ; i < wh->mReleaseTimeList.size() ; ++i) {
                            int64_t diff = wh->mReleaseTimeList.itemAt(i) - wh->mAcquireTimeList.itemAt(i);
                            sb->setLength(0);
                            BatteryStats::formatTimeMs(sb, diff);

                            #if 0
                            pw->println("    " + sb->toString() + "(" +
                                    new Date(wh->mAcquireTimeList->get(i))->toLocaleString() + " ~ " +
                                    new Date(wh->mReleaseTimeList->get(i))->toLocaleString() + ")");
                            #endif  // TODO date
                        }

                        if (wh->mAcquireTimeList.size() > wh->mReleaseTimeList.size()) {
                            int64_t diff = now - wh->mAcquireTimeList.itemAt(wh->mAcquireTimeList.size() - 1);
                            sb->setLength(0);
                            BatteryStats::formatTimeMs(sb, diff);

                            #if 0
                            pw->println("    " + sb->toString() + "(" +
                                    new Date(wh->mAcquireTimeList->get(wh->mAcquireTimeList->size() - 1))->toLocaleString() +
                                    " ~ current" + ")");
                            #endif  // TODO date
                        }

                        pw->println();
                    }
                }
                pw->println("=====================================================================================");
            }
        // Customize ---
    }
}

void BatteryStatsImpl::Clear_Containers() {
    GLOGENTRY();

    if (mUnpluggables != NULL) {
        mUnpluggables->clear();
    }

    mKernelWakelockStats.clear();
    if (mPartialTimers != NULL) {
        mPartialTimers->clear();
    }
    if (mFullTimers != NULL) {
        mFullTimers->clear();
    }
    if (mWindowTimers != NULL) {
        mWindowTimers->clear();
    }
    if (mWifiRunningTimers != NULL) {
        mWifiRunningTimers->clear();
    }
    if (mFullWifiLockTimers != NULL) {
        mFullWifiLockTimers->clear();
    }
    if (mScanWifiLockTimers != NULL) {
        mScanWifiLockTimers->clear();
    }
    if (mWifiMulticastTimers != NULL) {
        mWifiMulticastTimers->clear();
    }

    {
        GLOGAUTOMUTEX(_l, mThisLock);
        mUidStats.clear();
    }
}

sp<NetworkStats> BatteryStatsImpl::getNetworkStatsSummary() {
    GLOGENTRY();

    // NOTE: calls from BatteryStatsService already hold this lock
    // synchronized(this)
    {
        GLOGAUTOMUTEX(_l, mThisLock);

        // Customize +++
        int64_t currentTime = elapsedRealtime();
        bool needUpdate = false;

        if (((currentTime - mLastGetNetworkSummaryTime) > MAX_NETWORK_DELAY_MSTIME) ||(currentTime < mLastGetNetworkSummaryTime)) {
            needUpdate = true;
        }
        // Customize ---

        // Customize +++
        // change cache timeout form 1 second to 5 second to prevent the watchdog timeout
        if (mNetworkSummaryCache == NULL || needUpdate) {
            // mNetworkSummaryCache->getElapsedRealtimeAge() > DateUtils::SECOND_IN_MILLIS) {
        // Customize ---
            mNetworkSummaryCache = NULL;

            sp<String> str_PROP_QTAGUID_ENABLED = new String("net.qtaguid_enabled");
            if (SystemProperties::getBoolean(str_PROP_QTAGUID_ENABLED, false)) {
                long long start = elapsedRealtime();
                mNetworkSummaryCache = mNetworkStatsFactory->readNetworkStatsSummaryDev();
                long long end = elapsedRealtime();
                GLOGI("NetworkStatsFactory::readNetworkStatsSummaryDev start=%lld end=%lld duration=%lld",start, end, (end - start));
            }

            if (mNetworkSummaryCache == NULL) {
                mNetworkSummaryCache = new NetworkStats(elapsedRealtime(), 0);
            }
            // Customize +++
            mLastGetNetworkSummaryTime = elapsedRealtime();
            // Customize ---
        }
        return mNetworkSummaryCache;
    }
}

sp<NetworkStats> BatteryStatsImpl::getNetworkStatsDetailGroupedByUid() {
    GLOGENTRY();

    long long start = elapsedRealtime();
    // NOTE: calls from BatteryStatsService already hold this lock
    // synchronized(this)
    {
        // Customize +++
        int64_t currentTime = elapsedRealtime();
        bool needUpdate = false;

        {
            GLOGAUTOMUTEX(_l, mThisLock);
            if (((currentTime - mLastGetNetworkDetailTime) > MAX_NETWORK_DELAY_MSTIME) ||(currentTime < mLastGetNetworkDetailTime)) {
                needUpdate = true;
            }
        }

        // change cache timeout form 1 second to 5 second to prevent the watchdog timeout
        if (mNetworkDetailCache == NULL || needUpdate) {
            // mNetworkDetailCache->getElapsedRealtimeAge() > DateUtils::SECOND_IN_MILLIS) {
        // Customize ---
            mNetworkDetailCache = NULL;

            sp<String> str_PROP_QTAGUID_ENABLED = new String("net.qtaguid_enabled");

            if (SystemProperties::getBoolean(str_PROP_QTAGUID_ENABLED, false)) {
                long long start1 = elapsedRealtime();
                sp<NetworkStats> networkStats = mNetworkStatsFactory->readNetworkStatsDetail();
                long long end1 = elapsedRealtime();
                GLOGI("NetworkStatsFactory::readNetworkStatsDetail start=%lld end=%lld duration=%lld",start1, end1, (end1 - start1));

                if (networkStats != NULL) {
                    mNetworkDetailCache = networkStats->groupedByUid();
                } else {
                    LOGE("problem reading network stats");
                }
            }

            if (mNetworkDetailCache == NULL) {
                mNetworkDetailCache = new NetworkStats(elapsedRealtime(), 0);
            }

            {
                GLOGAUTOMUTEX(_l, mThisLock);
                // Customize +++
                mLastGetNetworkDetailTime = elapsedRealtime();
                // Customize ---
            }
        }
        long long end = elapsedRealtime();
        GLOGI("BatteryStatsImpl::getNetworkStatsDetailGroupedByUid start=%lld end=%lld duration=%lld",start, end, (end - start));
        return mNetworkDetailCache;
    }
}

// Customize +++
bool BatteryStatsImpl::useNewMethodToResetUidData() {
    GLOGENTRY();

    bool value = false;

    // synchronized (sLockObject) {
    {
        GLOGAUTOMUTEX(_l, mObjectLock);

        if (mUidStatsDel.size() <= 0) {
            value = true;
        }
    }

    if (DEBUG_ON) {
        GLOGD("use new method = %d", value);
    }

    return value;
}
// Customize ---

// Customize +++
void BatteryStatsImpl::handleBatteryStatsData() {
    GLOGENTRY();

    if (DEBUG_ON) {
        GLOGD("handleBatteryStatsData start");
    }

    sp<HandleBatteryStatsDataThread> thread = new HandleBatteryStatsDataThread(this);
    thread->run();

    if (DEBUG_ON) {
        GLOGD("handleBatteryStatsData end");
    }
}
// Customize ---

// Customize +++
void BatteryStatsImpl::addBatteryStatsDataToUnpluggables() {
    GLOGENTRY();

        int32_t  size = 0;

        if (DEBUG_ON) {
            GLOGD("addBatteryStatsDataToUnpluggables start");
        }

        #if 0
        if (DEBUG_ON) {
            size = mUnpluggables.size();
            GLOGD("before mUnpluggables.size() = %d", size);
            for (int32_t i = 0; i < size; i++) {
                GLGOD("before data= ", mUnpluggables.get(i));  // TODO
            }
        }
        #endif  // TODO

        mUnpluggables->clear();

        if (DEBUG_ON) {
            size = mUnpluggables->size();
            GLOGD("middle mUnpluggables.size() = %d", size);
        }

        mScreenOnTimer->attach();
        for (int32_t  i = 0; i < NUM_SCREEN_BRIGHTNESS_BINS; i++) {
            mScreenBrightnessTimer[i]->attach();
        }
        mInputEventCounter->attach();
        mPhoneOnTimer->attach();
        mAudioOnTimer->attach();
        mVideoOnTimer->attach();
        for (int32_t i = 0; i < SignalStrength::NUM_SIGNAL_STRENGTH_BINS; i++) {
            mPhoneSignalStrengthsTimer[i]->attach();
        }
        mPhoneSignalScanningTimer->attach();
        for (int32_t i = 0; i < NUM_DATA_CONNECTION_TYPES; i++) {
            mPhoneDataConnectionsTimer[i]->attach();
        }
        mWifiOnTimer->attach();
        mGlobalWifiRunningTimer->attach();
        mBluetoothOnTimer->attach();

        {
            GLOGAUTOMUTEX(_l, mThisLock);
            for (int32_t i = 0; i < static_cast<int32_t>(mUidStats.size()); i++) {
                mUidStats.valueAt(i)->attach();
            }
        }

        #if 0
        if (DEBUG_ON) {
            size = mUnpluggables.size();
            GLOGD("after mUnpluggables.size() = %d", size);
            for (int32_t i = 0; i < size; i++) {
                Slog.d(TAG, "after data= "+mUnpluggables.get(i));  // TODO
            }
        }
        #endif  // TODO

        if (DEBUG_ON) {
           GLOGD("addBatteryStatsDataToUnpluggables end");
        }
}
// Customize ---

BatteryStatsImpl::MyHandler::MyHandler(const android::wp<BatteryStatsImpl>& parent)
    : PREINIT_DYNAMIC(),
    mCtorSafe(new ctor_safe(this)),
    mBS(parent) {
    GLOGENTRY();
    delete mCtorSafe;
}

void BatteryStatsImpl::MyHandler::handleMessage(const sp<Message>& msg) {
    GLOGENTRY();

    sp<BatteryStatsImpl> mbs = mBS.promote();
    NULL_RTN(mbs, "Promotion to sp<BatteryStatsImpl> fails");

    sp<BatteryCallback> cb = mbs->mCallback;

    // Customize +++
    if (DEBUG_ON) {
        mbs->mCallbackCount++;

        if (mbs->mCallbackCount > 50) {
            mbs->mCallbackCount = 0;
            GLOGD("callback index = %d", msg->what);
            if (cb == NULL) {
                GLOGD("callback is null");
            }
        }
    }
    // Customize ---

    switch (msg->what) {
    case MSG_UPDATE_WAKELOCKS:
          if (cb != NULL) {
              cb->batteryNeedsCpuUpdate();
          }
          break;
      case MSG_REPORT_POWER_CHANGE:
          if (cb != NULL) {
              cb->batteryPowerChanged(msg->arg1 != 0);
          }
          break;
    }
}

BatteryStatsImpl::Counter::Counter(const wp<BatteryStatsImpl>& parent, sp<GArrayList<Unpluggable> >& unpluggables, const Parcel& in, int32_t value)
    : Unpluggable(value),
    PREINIT_DYNAMIC(),
    mCtorSafe(new ctor_safe(this)),
    mBS(parent) {
    GLOGENTRY();

    sp<BatteryStatsImpl> mbs = mBS.promote();
    NULL_RTN(mbs, "Promotion to sp<BatteryStatsImpl> fails");

    mCount = new AtomicInteger();

    mUnpluggables = unpluggables;
    mPluggedCount = in.readInt32();
    mCount->set(mPluggedCount);
    mLoadedCount = in.readInt32();
    mLastCount = 0;
    mUnpluggedCount = in.readInt32();

    // Customize +++
    // synchronized (sLockPlug)
    // Customize ---
    {
        GLOGAUTOMUTEX(_l, mbs->mPlugLock);
        unpluggables->add(this);
    }
    delete mCtorSafe;
}

BatteryStatsImpl::Counter::Counter(const wp<BatteryStatsImpl>& parent, sp<GArrayList<Unpluggable> >& unpluggables, int32_t value)
    : Unpluggable(value),
    PREINIT_DYNAMIC(),
    mCtorSafe(new ctor_safe(this)),
    mBS(parent) {
    GLOGENTRY();

    sp<BatteryStatsImpl> mbs = mBS.promote();
    NULL_RTN(mbs, "Promotion to sp<BatteryStatsImpl> fails");

    mCount = new AtomicInteger();
    mLoadedCount = 0;
    mLastCount = 0;
    mUnpluggedCount = 0;
    mPluggedCount = 0;

    mUnpluggables = unpluggables;

    // Customize +++
    // synchronized (sLockPlug)
    // Customize ---
    {
        GLOGAUTOMUTEX(_l, mbs->mPlugLock);

        unpluggables->add(this);
    }

    delete mCtorSafe;
}

void BatteryStatsImpl::Counter::writeToParcel(Parcel* out) {  // NOLINT
    GLOGENTRY();

    out->writeInt32(mCount->get());
    out->writeInt32(mLoadedCount);
    out->writeInt32(mUnpluggedCount);
}

void BatteryStatsImpl::Counter::unplug(int64_t batteryUptime, int64_t batteryRealtime) {
    GLOGENTRY();

    mUnpluggedCount = mPluggedCount;
    mCount->set(mPluggedCount);
}

void BatteryStatsImpl::Counter::plug(int64_t batteryUptime, int64_t batteryRealtime) {
    GLOGENTRY();

    mPluggedCount = mCount->get();
}

void BatteryStatsImpl::Counter::writeCounterToParcel(Parcel* out, sp<Counter>& counter) {  // NOLINT
    GLOGENTRY();

    if (counter == NULL) {
        out->writeInt32(0);  // indicates null
        return;
    }

    out->writeInt32(1);  // indicates non-null
    counter->writeToParcel(out);
}

int32_t BatteryStatsImpl::Counter::getCountLocked(int32_t which) {
    GLOGENTRY();

    int32_t val;

    if (which == STATS_LAST) {
        val = mLastCount;
    } else {
        val = mCount->get();

        if (which == STATS_SINCE_UNPLUGGED) {
            val -= mUnpluggedCount;
        } else if (which != STATS_SINCE_CHARGED) {
            val -= mLoadedCount;
        }
    }

    return val;
}

void BatteryStatsImpl::Counter::logState(const sp<Printer>& pw, const sp<String>& prefix) {
    GLOGENTRY();

    pw->println(String::format("mLoadedCount=%d mLoadedCount=%d  mUnpluggedCount=%d mPluggedCount=%d",
                mLoadedCount, mLastCount, mUnpluggedCount, mPluggedCount));
}

void BatteryStatsImpl::Counter::stepAtomic() {
    GLOGENTRY();

    mCount->incrementAndGet();
}

void BatteryStatsImpl::Counter::reset(bool detachIfReset) {
    GLOGENTRY();

    mCount->set(0);
    mLoadedCount = mLastCount = mPluggedCount = mUnpluggedCount = 0;

    if (detachIfReset) {
        detach();
    }
}

// Customize +++
void BatteryStatsImpl::Counter::attach() {
    GLOGENTRY();

    sp<BatteryStatsImpl> mbs = mBS.promote();
    NULL_RTN(mbs, "Promotion to sp<BatteryStatsImpl> fails");

    if (DEBUG_SECURITY) {
        GLOGD("Counter attach");
    }

    // synchronized (sLockPlug) {
    {
        GLOGAUTOMUTEX(_l, mbs->mPlugLock);

        mUnpluggables->add(this);
    }
}
// Customize ---

void BatteryStatsImpl::Counter::detach() {
    GLOGENTRY();

    sp<BatteryStatsImpl> mbs = mBS.promote();
    NULL_RTN(mbs, "Promotion to sp<BatteryStatsImpl> fails");

    // Customize +++
    // synchronized (sLockPlug)
    // Customize ---
    {
        GLOGAUTOMUTEX(_l, mbs->mPlugLock);
        mbs->mUnpluggables->remove(this);
    }
}

void BatteryStatsImpl::Counter::writeSummaryFromParcelLocked(Parcel* out) {  // NOLINT
    GLOGENTRY();

    int32_t count = mCount->get();
    out->writeInt32(count);
}

void BatteryStatsImpl::Counter::readSummaryFromParcelLocked(Parcel& in) {  // NOLINT
    GLOGENTRY();

    mLoadedCount = in.readInt32();
    mCount->set(mLoadedCount);
    mLastCount = 0;
    mUnpluggedCount = mPluggedCount = mLoadedCount;
}

BatteryStatsImpl::SamplingCounter::SamplingCounter(const wp<BatteryStatsImpl>& parent,
                                                   sp<GArrayList<Unpluggable> >& unpluggables,  // NOLINT
                                                   const Parcel& in,
                                                   int32_t value)
    :Counter(parent, unpluggables, in, value),
    PREINIT_DYNAMIC(),
    mCtorSafe(new ctor_safe(this)),
    mBS(parent) {
    GLOGENTRY();
    mDelFlags = value;
    delete mCtorSafe;
}

BatteryStatsImpl::SamplingCounter::SamplingCounter(const wp<BatteryStatsImpl>& parent,
                                                   sp<GArrayList<Unpluggable> >& unpluggables,  // NOLINT
                                                   int32_t value)
    :Counter(parent, unpluggables, value),
    PREINIT_DYNAMIC(),
    mCtorSafe(new ctor_safe(this)),
    mBS(parent) {
    GLOGENTRY();
    mDelFlags = value;
    delete mCtorSafe;
}


void BatteryStatsImpl::SamplingCounter::addCountAtomic(int64_t count) {
    GLOGENTRY();

    mCount->addAndGet(static_cast<int32_t>(count));
}

BatteryStatsImpl::Timer::Timer(const wp<BatteryStatsImpl>& parent,
                               int32_t type,
                               const wp<GArrayList<Unpluggable> >& unpluggables,
                               const Parcel &in)  // NOLINT
    : Unpluggable(0),
    PREINIT_DYNAMIC(),
    mCtorSafe(new ctor_safe(this)),
    mBS(parent),
    mType(type) {
    GLOGENTRY();

    sp<BatteryStatsImpl> mbs = mBS.promote();
    NULL_RTN(mbs, "Promotion to sp<BatteryStatsImpl> fails");

    sp<GArrayList<Unpluggable> > tmp_unpluggables = unpluggables.promote();
    NULL_RTN(tmp_unpluggables, "Promotion to sp<Unpluggable> fails");

    mUnpluggables = tmp_unpluggables;
    mCount = in.readInt32();
    mLoadedCount = in.readInt32();
    mLastCount = 0;
    mUnpluggedCount = in.readInt32();
    mTotalTime = in.readInt64();
    mLoadedTime = in.readInt64();
    mLastTime = 0;
    mUnpluggedTime = in.readInt64();

    // Customize +++
    // synchronized (sLockPlug)
    // Customize ---
    {
        GLOGAUTOMUTEX(_l, mbs->mPlugLock);
        tmp_unpluggables->add(this);
    }
    delete mCtorSafe;
}

BatteryStatsImpl::Timer::Timer(const wp<BatteryStatsImpl>& parent,
                               int32_t type,
                               const wp<GArrayList<Unpluggable> >& unpluggables) // NOLINT
    : Unpluggable(0),
    PREINIT_DYNAMIC(),
    mCtorSafe(new ctor_safe(this)),
    mBS(parent),
    mType(type) {
    GLOGENTRY();

    mCount = 0;
    mLoadedCount = 0;
    mLastCount = 0;
    mUnpluggedCount = 0;
    mTotalTime = 0;
    mLoadedTime = 0;
    mLastTime = 0;
    mUnpluggedTime = 0;

    sp<BatteryStatsImpl> mbs = mBS.promote();
    NULL_RTN(mbs, "Promotion to sp<BatteryStatsImpl> fails");

    sp<GArrayList<Unpluggable> > tmp_unpluggables = unpluggables.promote();
    NULL_RTN(tmp_unpluggables, "Promotion to sp<Unpluggable> fails");

    mUnpluggables = tmp_unpluggables;

    // Customize +++
    // synchronized (sLockPlug)
    // Customize ---
    {
        GLOGAUTOMUTEX(_l, mbs->mPlugLock);
        tmp_unpluggables->add(this);
    }
    delete mCtorSafe;
}

bool BatteryStatsImpl::Timer::reset(const sp<BatteryStatsImpl>& stats, bool detachIfReset) {
    GLOGENTRY();

    mTotalTime = mLoadedTime = mLastTime = 0;
    mCount = mLoadedCount = mLastCount = 0;

    if (detachIfReset) {
        detach();
    }

    return true;
}

// Customize +++
void BatteryStatsImpl::Timer::attach() {
    GLOGENTRY();

    sp<BatteryStatsImpl> mbs = mBS.promote();
    NULL_RTN(mbs, "Promotion to sp<BatteryStatsImpl> fails");

    sp<GArrayList<Unpluggable> > tmp_mUnpluggables = mUnpluggables.promote();
    NULL_RTN(tmp_mUnpluggables, "Promotion to sp<Unpluggable> fails");

    if (DEBUG_SECURITY) {
        GLOGD("Timer attach");
    }

    // synchronized (sLockPlug) {
    {
        GLOGAUTOMUTEX(_l, mbs->mPlugLock);

        tmp_mUnpluggables->add(this);
    }
}
// Customize ---

void BatteryStatsImpl::Timer::detach() {
    GLOGENTRY();

    sp<BatteryStatsImpl> mbs = mBS.promote();
    NULL_RTN(mbs, "Promotion to sp<BatteryStatsImpl> fails");

    sp<GArrayList<Unpluggable> > tmp_unpluggables = mUnpluggables.promote();
    NULL_RTN(tmp_unpluggables, "Promotion to sp<Unpluggable> fails");

    // Customize +++
    // synchronized (sLockPlug)
    // Customize ---
    {
        GLOGAUTOMUTEX(_l, mbs->mPlugLock);
        tmp_unpluggables->remove(this);
    }
}

void BatteryStatsImpl::Timer::writeToParcel(Parcel* out, int64_t batteryRealtime) {  // NOLINT
    GLOGENTRY();

    out->writeInt32(mCount);
    out->writeInt32(mLoadedCount);
    out->writeInt32(mUnpluggedCount);
    out->writeInt64(computeRunTimeLocked(batteryRealtime));
    out->writeInt64(mLoadedTime);
    out->writeInt64(mUnpluggedTime);
}

void BatteryStatsImpl::Timer::unplug(int64_t batteryUptime, int64_t batteryRealtime) {
    GLOGENTRY();

    if (DEBUG && mType < 0) {
        GLOGV("%s, unplug #%d : realtime=%lld old mUnpluggedTime=%lld old mUnpluggedCount= %d", LOG_TAG,
                                                                                                mType,
                                                                                                batteryRealtime,
                                                                                                mUnpluggedTime,
                                                                                                mUnpluggedCount);
    }

    mUnpluggedTime = computeRunTimeLocked(batteryRealtime);
    mUnpluggedCount = mCount;

    if (DEBUG && mType < 0) {
        GLOGV("%s, unplug #%d: new mUnpluggedTime=%lld new mUnpluggedCount=%d", LOG_TAG,
                                                                                mType,
                                                                                mUnpluggedTime,
                                                                                mUnpluggedCount);
    }
}

void BatteryStatsImpl::Timer::plug(int64_t batteryUptime, int64_t batteryRealtime) {
    GLOGENTRY();

    if (DEBUG && mType < 0) {
        GLOGV("%s, plug#%d  : realtime=%lld  old mTotalTime=%lld", LOG_TAG,
                                                                   mType,
                                                                   batteryRealtime,
                                                                   mTotalTime);
    }

    mTotalTime = computeRunTimeLocked(batteryRealtime);
    mCount = computeCurrentCountLocked();

    if (DEBUG && mType < 0) {
        GLOGV("%s, plug#%d : new mTotalTime=%lld", LOG_TAG, mType, mTotalTime);
    }
}

void BatteryStatsImpl::Timer::writeTimerToParcel(Parcel* out, const sp<Timer>& timer, int64_t batteryRealtime) {
    GLOGENTRY();

    if (timer == NULL) {
        out->writeInt32(0);  // indicates null
        return;
    }

    out->writeInt32(1);  // indicates non-null
    timer->writeToParcel(out, batteryRealtime);
}

int64_t BatteryStatsImpl::Timer::getTotalTimeLocked(int64_t batteryRealtime, int32_t which) {
    GLOGENTRY();

    int64_t val;

    if (which == STATS_LAST) {
        val = mLastTime;
    } else {
        val = computeRunTimeLocked(batteryRealtime);

        if (which == STATS_SINCE_UNPLUGGED) {
            val -= mUnpluggedTime;
        } else if (which != STATS_SINCE_CHARGED) {
            val -= mLoadedTime;
        }
    }

    return val;
}

int32_t BatteryStatsImpl::Timer::getCountLocked(int32_t which) {
    GLOGENTRY();

    int32_t val;

    if (which == STATS_LAST) {
        val = mLastCount;
    } else {
        val = computeCurrentCountLocked();

        if (which == STATS_SINCE_UNPLUGGED) {
            val -= mUnpluggedCount;
        } else if (which != STATS_SINCE_CHARGED) {
            val -= mLoadedCount;
        }
    }

    return val;
}

void BatteryStatsImpl::Timer::logState(const sp<Printer>& pw, const sp<String>& prefix) {
    GLOGENTRY();

    pw->println(String::format("mCount=%d mLoadedCount=%d  mLastCount=%d mUnpluggedCount=%d", mCount,
                                                                                              mLoadedCount,
                                                                                              mLastCount,
                                                                                              mUnpluggedCount));

    pw->println(String::format("mTotalTime=%lld mLoadedTime=%lld", mTotalTime, mLoadedTime));
    pw->println(String::format("mLastTime=%lld mUnpluggedTime=%lld", mLastTime, mUnpluggedTime));
}

void BatteryStatsImpl::Timer::writeSummaryFromParcelLocked(Parcel* out, int64_t batteryRealtime) {  // NOLINT
    GLOGENTRY();

    int64_t runTime = computeRunTimeLocked(batteryRealtime);
    // Divide by 1000 for backwards compatibility
    out->writeInt64((runTime + 500) / 1000);
    out->writeInt32(mCount);
}

void BatteryStatsImpl::Timer::readSummaryFromParcelLocked(Parcel& in) {  // NOLINT
    GLOGENTRY();

    // Multiply by 1000 for backwards compatibility
    mTotalTime = mLoadedTime = in.readInt64() * 1000;
    mLastTime = 0;
    mUnpluggedTime = mTotalTime;
    mCount = mLoadedCount = in.readInt32();
    mLastCount = 0;
    mUnpluggedCount = mCount;
}

BatteryStatsImpl::SamplingTimer::SamplingTimer(const wp<BatteryStatsImpl>& parent,
                                               const sp<GArrayList<Unpluggable> >& unpluggables,  // NOLINT
                                               bool inDischarge,
                                               const Parcel& in)
    :Timer(parent, 0, unpluggables, in),
    PREINIT_DYNAMIC(),
    mCtorSafe(new ctor_safe(this)),
    mBS(parent) {
    GLOGENTRY();

    mUpdateVersion = 0;

    mCurrentReportedCount = in.readInt32();
    mUnpluggedReportedCount = in.readInt32();
    mCurrentReportedTotalTime = in.readInt64();
    mUnpluggedReportedTotalTime = in.readInt64();
    mTrackingReportedValues = in.readInt32() == 1;
    mInDischarge = inDischarge;
    delete mCtorSafe;
}

BatteryStatsImpl::SamplingTimer::SamplingTimer(const wp<BatteryStatsImpl>& parent,
                                               const sp<GArrayList<Unpluggable> >& unpluggables,  // NOLINT
                                               bool inDischarge,
                                               bool trackReportedValues)
    :Timer(parent, 0, unpluggables),
    PREINIT_DYNAMIC(),
    mCtorSafe(new ctor_safe(this)),
    mBS(parent) {
    GLOGENTRY();

    mCurrentReportedCount = 0;
    mUnpluggedReportedCount = 0;
    mCurrentReportedTotalTime = 0;
    mUnpluggedReportedTotalTime = 0;
    mUpdateVersion = 0;

    mTrackingReportedValues = trackReportedValues;
    mInDischarge = inDischarge;
    delete mCtorSafe;
}

void BatteryStatsImpl::SamplingTimer::setStale() {
    GLOGENTRY();

    mTrackingReportedValues = false;
    mUnpluggedReportedTotalTime = 0;
    mUnpluggedReportedCount = 0;
}

void BatteryStatsImpl::SamplingTimer::setUpdateVersion(int32_t version) {
    GLOGENTRY();

    mUpdateVersion = version;
}

int32_t BatteryStatsImpl::SamplingTimer::getUpdateVersion() {
    GLOGENTRY();

    return mUpdateVersion;
}

void BatteryStatsImpl::SamplingTimer::updateCurrentReportedCount(int32_t count) {
    GLOGENTRY();

    if (mInDischarge && mUnpluggedReportedCount == 0) {
        // Updating the reported value for the first time.
        mUnpluggedReportedCount = count;
        // If we are receiving an update update mTrackingReportedValues;
        mTrackingReportedValues = true;
    }

    mCurrentReportedCount = count;
}

void BatteryStatsImpl::SamplingTimer::updateCurrentReportedTotalTime(int64_t totalTime) {
    GLOGENTRY();

    if (mInDischarge && mUnpluggedReportedTotalTime == 0) {
        // Updating the reported value for the first time.
        mUnpluggedReportedTotalTime = totalTime;
        // If we are receiving an update update mTrackingReportedValues;
        mTrackingReportedValues = true;
    }

    mCurrentReportedTotalTime = totalTime;
}

void BatteryStatsImpl::SamplingTimer::unplug(int64_t batteryUptime, int64_t batteryRealtime) {
    GLOGENTRY();

    Timer::unplug(batteryUptime, batteryRealtime);

    if (mTrackingReportedValues) {
        mUnpluggedReportedTotalTime = mCurrentReportedTotalTime;
        mUnpluggedReportedCount = mCurrentReportedCount;
    }

    mInDischarge = true;
}

void BatteryStatsImpl::SamplingTimer::plug(int64_t batteryUptime, int64_t batteryRealtime) {
    GLOGENTRY();

    Timer::plug(batteryUptime, batteryRealtime);
    mInDischarge = false;
}

void BatteryStatsImpl::SamplingTimer::logState(sp<Printer>& pw, sp<String>& prefix) {  // NOLINT
    GLOGENTRY();

    Timer::logState(pw, prefix);
    pw->println(String::format("mCurrentReportedCount=%d mUnpluggedReportedCount=%d  mCurrentReportedTotalTime=%lld mUnpluggedReportedTotalTime=%lld", mCurrentReportedCount,
                                                                                                                                                       mUnpluggedReportedCount,
                                                                                                                                                       mCurrentReportedTotalTime,
                                                                                                                                                       mUnpluggedReportedTotalTime));
}

int64_t BatteryStatsImpl::SamplingTimer::computeRunTimeLocked(int64_t curBatteryRealtime) {
    GLOGENTRY();

    return mTotalTime + (mInDischarge && mTrackingReportedValues
                         ? mCurrentReportedTotalTime - mUnpluggedReportedTotalTime : 0);
}

int32_t BatteryStatsImpl::SamplingTimer::computeCurrentCountLocked() {
    GLOGENTRY();

    return mCount + (mInDischarge && mTrackingReportedValues
                     ? mCurrentReportedCount - mUnpluggedReportedCount : 0);
}

void BatteryStatsImpl::SamplingTimer::writeToParcel(Parcel* out, int64_t batteryRealtime) {  // NOLINT
    GLOGENTRY();

    Timer::writeToParcel(out, batteryRealtime);
    out->writeInt32(mCurrentReportedCount);
    out->writeInt32(mUnpluggedReportedCount);
    out->writeInt64(mCurrentReportedTotalTime);
    out->writeInt64(mUnpluggedReportedTotalTime);
    out->writeInt32(mTrackingReportedValues ? 1 : 0);
}

bool BatteryStatsImpl::SamplingTimer::reset(const sp<BatteryStatsImpl>& stats, bool detachIfReset) {
    GLOGENTRY();

    Timer::reset(stats, detachIfReset);
    setStale();
    return true;
}

void BatteryStatsImpl::SamplingTimer::writeSummaryFromParcelLocked(Parcel* out, int64_t batteryRealtime) {  // NOLINT
    GLOGENTRY();

    Timer::writeSummaryFromParcelLocked(out, batteryRealtime);
    out->writeInt64(mCurrentReportedTotalTime);
    out->writeInt32(mCurrentReportedCount);
    out->writeInt32(mTrackingReportedValues ? 1 : 0);
}

void BatteryStatsImpl::SamplingTimer::readSummaryFromParcelLocked(Parcel& in) {  // NOLINT
    GLOGENTRY();

    Timer::readSummaryFromParcelLocked(in);
    mUnpluggedReportedTotalTime = mCurrentReportedTotalTime = in.readInt64();
    mUnpluggedReportedCount = mCurrentReportedCount = in.readInt32();
    mTrackingReportedValues = in.readInt32() == 1;
}

BatteryStatsImpl::StopwatchTimer::StopwatchTimer(const wp<BatteryStatsImpl>& parent,
                                                 const wp<Uid>& uid,
                                                 int32_t type,
                                                 const wp<GArrayList<StopwatchTimer> >& timerPool,
                                                 const wp<GArrayList<Unpluggable> >& unpluggables,
                                                 const Parcel& in)
    : Timer(parent, type, unpluggables, in),
    PREINIT_DYNAMIC(),
    mCtorSafe(new ctor_safe(this)),
    mBS(parent),
    mTimerPool(timerPool) {
    GLOGENTRY();

    mNesting = 0;
    mAcquireTime = 0;
    mTimeout = 0;
    mInList = 0;

    mUid = uid;
    mUpdateTime = in.readInt64();
    delete mCtorSafe;
}

BatteryStatsImpl::StopwatchTimer::StopwatchTimer(const wp<BatteryStatsImpl>& parent,
                                                 const wp<Uid>& uid,
                                                 int32_t type,
                                                 const wp<GArrayList<StopwatchTimer> >& timerPool,
                                                 const wp<GArrayList<Unpluggable> >& unpluggables)
    : Timer(parent, type, unpluggables),
    PREINIT_DYNAMIC(),
    mCtorSafe(new ctor_safe(this)),
    mBS(parent),
    mTimerPool(timerPool) {
    GLOGENTRY();

    mNesting = 0;
    mUpdateTime = 0;
    mAcquireTime = 0;
    mTimeout = 0;
    mInList = 0;
    mUid = uid;
    delete mCtorSafe;
}

void BatteryStatsImpl::StopwatchTimer::setTimeout(int64_t timeout) {
    GLOGENTRY();

    mTimeout = timeout;
}

void BatteryStatsImpl::StopwatchTimer::writeToParcel(Parcel* out, int64_t batteryRealtime) {  // NOLINT
    GLOGENTRY();

    Timer::writeToParcel(out, batteryRealtime);
    out->writeInt64(mUpdateTime);
}

void BatteryStatsImpl::StopwatchTimer::plug(int64_t batteryUptime, int64_t batteryRealtime) {
    GLOGENTRY();

    if (mNesting > 0) {
        if (mNesting > 0) {
            if (DEBUG && mType < 0) {
                LOGV("%s, old mUpdateTime=%lld" , TAG, mUpdateTime);
            }

            Timer::plug(batteryUptime, batteryRealtime);
            mUpdateTime = batteryRealtime;

            if (DEBUG && mType < 0) {
                LOGV("%s, new mUpdateTime=%lld" , TAG, mUpdateTime);
            }
        }
    }
}

void BatteryStatsImpl::StopwatchTimer::logState(const sp<Printer>& pw, const sp<String>& prefix) {
    GLOGENTRY();

    Timer::logState(pw, prefix);
    pw->println(prefix + "mNesting=" + mNesting + "mUpdateTime=" + mUpdateTime
                + " mAcquireTime=" + mAcquireTime);
}

void BatteryStatsImpl::StopwatchTimer::startRunningLocked(const sp<BatteryStatsImpl> & stats) {
    GLOGENTRY();

    if (mNesting++ == 0) {
        mUpdateTime = stats->getBatteryRealtimeLocked(elapsedRealtime() * 1000);

        sp<GArrayList<StopwatchTimer> > tmp_mTimerPool = mTimerPool.promote();

        if (tmp_mTimerPool != NULL) {
            // Accumulate time to all currently active timers before adding
            // this new one to the pool.
            refreshTimersLocked(stats, mTimerPool);
            // Add this timer to the active pool
            tmp_mTimerPool->add(this);
        }

        // Increment the count
        mCount++;
        mAcquireTime = mTotalTime;

        if (DEBUG && mType < 0) {
            LOGV("%s, start #%d: mUpdateTime=%d mTotalTime=%d mCount=%d mAcquireTime=%d", TAG,
                                                                                          mType,
                                                                                          mUpdateTime,
                                                                                          mTotalTime,
                                                                                          mCount,
                                                                                          mAcquireTime);
        }
    }
}

bool BatteryStatsImpl::StopwatchTimer::isRunningLocked() {
    GLOGENTRY();

    return mNesting > 0;
}

void BatteryStatsImpl::StopwatchTimer::stopRunningLocked(const sp<BatteryStatsImpl>& stats) {
    GLOGENTRY();

    // Ignore attempt to stop a timer that isn't running
    if (mNesting == 0) {
        return;
    }

    if (--mNesting == 0) {
        sp<GArrayList<StopwatchTimer> > tmp_mTimerPool = mTimerPool.promote();

        if (tmp_mTimerPool != NULL) {
            // Accumulate time to all active counters, scaled by the total
            // active in the pool, before taking this one out of the pool.
            refreshTimersLocked(stats, mTimerPool);

            // Remove this timer from the active pool
            tmp_mTimerPool->remove(this);
        } else {
            const int64_t realtime = elapsedRealtime() * 1000;
            const int64_t batteryRealtime = stats->getBatteryRealtimeLocked(realtime);
            mNesting = 1;
            mTotalTime = computeRunTimeLocked(batteryRealtime);
            mNesting = 0;
        }

        if (DEBUG && mType < 0) {
            LOGV("%s, stop #%d: mUpdateTime=%d mTotalTime=%d mCount=%d mAcquireTime=%d", TAG,
                                                                                         mType,
                                                                                         mUpdateTime,
                                                                                         mTotalTime,
                                                                                         mCount,
                                                                                         mAcquireTime);
        }

        if (mTotalTime == mAcquireTime) {
            // If there was no change in the time, then discard this
            // count.  A somewhat cheezy strategy, but hey.
            mCount--;
        }
    }
}

void BatteryStatsImpl::StopwatchTimer::refreshTimersLocked(const sp<BatteryStatsImpl>& stats, wp<GArrayList<StopwatchTimer> >& pool) {
    GLOGENTRY();

    sp<GArrayList<StopwatchTimer> > tmp_pool = pool.promote();
    NULL_RTN(tmp_pool, "Promotion to sp<StopwatchTimer> fails");
    const int64_t realtime = elapsedRealtime() * 1000;
    const int64_t batteryRealtime = stats->getBatteryRealtimeLocked(realtime);
    const int32_t N = tmp_pool->size();

    for (int32_t i = N-1; i>= 0; i--) {
        sp<StopwatchTimer> t = tmp_pool->get(i);
        int64_t heldTime = batteryRealtime - t->mUpdateTime;
        if (heldTime > 0) {
            t->mTotalTime += heldTime / N;
        }
        t->mUpdateTime = batteryRealtime;
    }
}

// Customize +++
int32_t BatteryStatsImpl::StopwatchTimer::checkTimerPoolSize() {
    GLOGENTRY();

    sp<GArrayList<StopwatchTimer> > tmp_mTimerPool = mTimerPool.promote();

    int32_t tmpSize = 1;

    if (tmp_mTimerPool != NULL) {
        tmpSize = tmp_mTimerPool->size();

        if (tmpSize <= 0) {
            GLOGW("Timer Pool size <= 0");
            tmpSize = 1;
        }
    }
    return tmpSize;
}
// Customize ---

int64_t BatteryStatsImpl::StopwatchTimer::computeRunTimeLocked(int64_t curBatteryRealtime) {
    GLOGENTRY();

    sp<GArrayList<StopwatchTimer> > tmp_mTimerPool = mTimerPool.promote();

    if (mTimeout > 0 && curBatteryRealtime > mUpdateTime + mTimeout) {
        curBatteryRealtime = mUpdateTime + mTimeout;
    }

    // Customize +++
    return mTotalTime + (mNesting > 0 ? 
                        (curBatteryRealtime - mUpdateTime) / ((tmp_mTimerPool != NULL) ? checkTimerPoolSize() : 1) 
                        : 0);
    // Customize ---
}

int32_t BatteryStatsImpl::StopwatchTimer::computeCurrentCountLocked() {
    GLOGENTRY();

    return mCount;
}

// Customize +++
void BatteryStatsImpl::StopwatchTimer::attach() {
    GLOGENTRY();

    if (DEBUG_SECURITY) {
        GLOGD("Stopwatch attach");
    }
    Timer::attach();
}
// Customize ---

bool BatteryStatsImpl::StopwatchTimer::reset(const sp<BatteryStatsImpl>& stats, bool detachIfReset) {
    GLOGENTRY();

    bool canDetach = mNesting <= 0;
    Timer::reset(stats, canDetach && detachIfReset);

    if (mNesting > 0 && stats != NULL) {
        mUpdateTime = stats->getBatteryRealtimeLocked(elapsedRealtime() * 1000);
    }

    mAcquireTime = mTotalTime;
    return canDetach;
}

void BatteryStatsImpl::StopwatchTimer::detach() {
    GLOGENTRY();

    Timer::detach();

    sp<GArrayList<StopwatchTimer> > tmp_mTimerPool = mTimerPool.promote();

    if (tmp_mTimerPool != NULL) {
        tmp_mTimerPool->remove(this);
    }
}

void BatteryStatsImpl::StopwatchTimer::readSummaryFromParcelLocked(Parcel& in) {  // NOLINT
    GLOGENTRY();

    Timer::readSummaryFromParcelLocked(in);
    mNesting = 0;
}

// Customize +++
BatteryStatsImpl::GpuTimer::GpuTimer(const android::wp<BatteryStatsImpl>& parent,
                                     const android::wp<Uid>& uid,
                                     int32_t type,
                                     const android::wp<GArrayList<GpuTimer> >& timerPool,
                                     const android::wp<GArrayList<Unpluggable> >& unpluggables,
                                     const android::Parcel& in)
    : Timer(parent, type, unpluggables, in),
    PREINIT_DYNAMIC(),
    mCtorSafe(new ctor_safe(this)),
    mBS(parent),
    mUid(uid),
    mTimerPool(timerPool) {
    GLOGENTRY();

    sp<Uid> tmpUid = uid.promote();
    NULL_RTN(tmpUid, "Promotion to sp<Uid> fails");

    mNesting = 0;
    mUpdateTime = 0;
    mAcquireTime = 0;
    mTimeout = 0;

    mUnplugGpuSpeedTimes = NULL;
    mGpuSpeedTimes = NULL;
    mRelGpuSpeedTimes = NULL;
    mTotalGpuSpeedTimes = NULL; 

    if (DEBUG_GPU) {
        GLOGD("GpuTimer Contructure11 uid = %d : type = %d", tmpUid->getUid(), type);
    }

    mUpdateTime = in.readInt64();

    int32_t bins = in.readInt32();
    int32_t steps = getGpuSpeedSteps();

    if (DEBUG_GPU) {
        GLOGD("GpuTimer Contructure11 bins = %d : steps = %d", bins, steps);
    }

    if ((bins < 0) || (bins > MAX_GPU_SPEED_NUM)) {
        bins = 0;
    }

    mTotalGpuSpeedTimes = new Blob<int64_t>(bins >= steps ? bins : steps);

    for (int32_t i = 0; i < bins; i++) {
        mTotalGpuSpeedTimes[i] = in.readInt64();
    }

    if ( bins < steps ) {
        for (int32_t j = bins; j < steps; j++) {
            mTotalGpuSpeedTimes[j] = 0;  // Initialize
        }
    }

    delete mCtorSafe;
}
// Customize ---

// Customize +++
BatteryStatsImpl::GpuTimer::GpuTimer(const android::wp<BatteryStatsImpl>& parent,
                                     const android::wp<Uid>& uid,
                                     int32_t type,
                                     const android::wp<GArrayList<GpuTimer> >& timerPool,
                                     const android::wp<GArrayList<Unpluggable> >& unpluggables)
    : Timer(parent, type, unpluggables),
    PREINIT_DYNAMIC(),
    mCtorSafe(new ctor_safe(this)),
    mBS(parent),
    mUid(uid),
    mTimerPool(timerPool) {
    GLOGENTRY();

    mNesting = 0;
    mUpdateTime = 0;
    mAcquireTime = 0;
    mTimeout = 0;

    mUnplugGpuSpeedTimes = NULL;
    mGpuSpeedTimes = NULL;
    mRelGpuSpeedTimes = NULL;
    mTotalGpuSpeedTimes = NULL; 

    if (DEBUG_GPU) {
        sp<Uid> tmp = uid.promote();
        NULL_RTN(tmp, "Promotion to sp<Uid> fails");
        GLOGD("GpuTimer Contructure22 uid = %d : tyep = %d", tmp->getUid(), type);
    }

    int32_t steps = getGpuSpeedSteps();

    mTotalGpuSpeedTimes = new Blob<int64_t>(steps);

    for (int i = 0; i < steps; i++) {
        mTotalGpuSpeedTimes[i] = 0;  // Initialize
    }

    delete mCtorSafe;
}
// Customize ---

// Customize +++
void BatteryStatsImpl::GpuTimer::setTimeout(int64_t timeout) {
    GLOGENTRY();

    if (DEBUG_GPU) {
        GLOGD("GpuTimer setTimeout timeout= %lld ", timeout);
    }

    mTimeout = timeout;
}
// Customize ---

// Customize +++
void BatteryStatsImpl::GpuTimer::writeToParcel(android::Parcel* out, int64_t batteryRealtime) {
    GLOGENTRY();

    if (DEBUG_GPU) {
        GLOGD("GpuTimer writeToParcel");
    }

    Timer::writeToParcel(out, batteryRealtime);
    out->writeLong(mUpdateTime);

    int32_t bins = sizeof(mTotalGpuSpeedTimes) / sizeof(mTotalGpuSpeedTimes[0]);

    if ((bins < 0) || (bins > MAX_GPU_SPEED_NUM)) {
        bins = 0;
    }

    out->writeInt32(bins);

    for (int32_t i = 0; i < bins; i++) {
        out->writeInt64(mTotalGpuSpeedTimes[i]);
    }
}
// Customize ---

// Customize +++
void BatteryStatsImpl::GpuTimer::unplug(int64_t batteryUptime, int64_t batteryRealtime) {
    GLOGENTRY();

    if (DEBUG_GPU) {
        GLOGD("GpuTimer unplug mNesting = %d, batteryUptime = %lld, batteryUptime = %lld", mNesting, batteryUptime, batteryRealtime);
    }

    Timer::unplug(batteryUptime, batteryRealtime);

    // Gpu doesn't need to handle this
    /*
    if (mUnplugGpuSpeedTimes == null)
    {
        mUnplugGpuSpeedTimes = ProcessStats.getGpuSpeedTimes(null);
    }
    else
    {
        ProcessStats.getGpuSpeedTimes(mUnplugGpuSpeedTimes);
    }

    if (DEBUG_GPU)
    {
        for (int i = 0; i < mUnplugGpuSpeedTimes.length; i++)
        {
            GLOGD("unplug mUnplugGpuSpeedTimes[" + i + "]=" + mUnplugGpuSpeedTimes[i]);
        }
    }
    */

    if (mNesting > 0) {
        if (DEBUG_GPU) {
            GLOGD("GpuTimer unplug old mUpdateTime = %lld", mUpdateTime);
        }

        // Get current Gpu timer
        if (mGpuSpeedTimes == 0) {
            mGpuSpeedTimes = ProcessStats::getGpuSpeedTimes(NULL);
        } else {
            ProcessStats::getGpuSpeedTimes(mGpuSpeedTimes);
        }

        if (DEBUG_GPU) {
            for (int32_t i = 0; i < static_cast<int32_t>(sizeof(mGpuSpeedTimes) / sizeof(mGpuSpeedTimes[0])); i++) {
                GLOGD("unplug mGpuSpeedTimes i = %lld", mGpuSpeedTimes[i]);
            }
        }
    }
}
// Customize ---

// Customize +++
void BatteryStatsImpl::GpuTimer::plug(int64_t batteryUptime, int64_t batteryRealtime) {
    GLOGENTRY();

    if (DEBUG_GPU) {
        GLOGD("GpuTimer plug mNesting = %d, batteryUptime = %lld, batteryUptime = %lld", mNesting, batteryUptime, batteryRealtime);
    }

    if (mNesting > 0) {
        if (DEBUG_GPU) {
            GLOGD("GpuTimer plug old mUpdateTime = %lld", mUpdateTime);
        }

        Timer::plug(batteryUptime, batteryRealtime);

        updateGpuSpeedStepTimes();

        mUpdateTime = batteryRealtime;

        if (DEBUG_GPU) {
            GLOGD("GpuTimer plug new mUpdateTime = %lld", mUpdateTime);
        }
    }
}
// Customize ---

// Customize +++
void BatteryStatsImpl::GpuTimer::logState(const android::sp<Printer>& pw, const android::sp<String>& prefix) {
    GLOGENTRY();

    Timer::logState(pw, prefix);
    pw->println(prefix + "mNesting=" + mNesting + "mUpdateTime=" + mUpdateTime + " mAcquireTime=" + mAcquireTime);
}
// Customize ---

// Customize +++
void BatteryStatsImpl::GpuTimer::startRunningLocked(const android::sp<BatteryStatsImpl>& stats) {
    GLOGENTRY();

    if (DEBUG_GPU) {
        GLOGD("GpuTimer startRunningLocked mNesting = %d : mUpdateTime = %lld ,mTotalTime = %lld, mCount = %d, mAcquireTime = %ldd", mNesting, mUpdateTime, mTotalTime, mCount, mAcquireTime);
    }

    if (mNesting++ == 0) {
        mUpdateTime = stats->getBatteryRealtimeLocked(elapsedRealtime() * 1000);

        sp<GArrayList<GpuTimer> > tmp_mTimerPool = mTimerPool.promote();
        NULL_RTN(tmp_mTimerPool, "Promotion to sp<GpuTimer> fails");

        if (tmp_mTimerPool != NULL) {
            // Accumulate time to all currently active timers before adding
            // this new one to the pool.
            refreshTimersLocked(stats, mTimerPool);
            // Add this timer to the active pool
            tmp_mTimerPool->add(this);
        }

        // get GPU time from kernel
        if (mGpuSpeedTimes == NULL) {
            mGpuSpeedTimes = ProcessStats::getGpuSpeedTimes(NULL);
            mRelGpuSpeedTimes = new Blob<int64_t>(sizeof(mGpuSpeedTimes) / sizeof(mGpuSpeedTimes[0]));

            for (int32_t i = 0; i < static_cast<int32_t>(sizeof(mGpuSpeedTimes) / sizeof(mGpuSpeedTimes[0])); i++) {
                mRelGpuSpeedTimes[i] = 0;  // Initialize
            }
        } else {
            ProcessStats::getGpuSpeedTimes(mGpuSpeedTimes);
        }

        // Increment the count
        mCount++;
        mAcquireTime = mTotalTime;

        if (DEBUG_GPU) {
            for (int32_t i = 0; i < static_cast<int32_t>(sizeof(mGpuSpeedTimes) / sizeof(mGpuSpeedTimes[0])); i++) {
                GLOGD("start mGpuSpeedTimes  i  = %lld", mGpuSpeedTimes[i]);
            }

            GLOGD("GpuTimer Start After # %d : mUpdateTime = %lld, mTotalTime = %lld, mCount = %d, mAcquireTime = %lld", mType, mUpdateTime, mTotalTime, mCount, mAcquireTime);
        }
    }
}
// Customize ---

// Customize +++
bool BatteryStatsImpl::GpuTimer::isRunningLocked() {
    GLOGENTRY();
    return mNesting > 0;
}
// Customize ---

// Customize +++
void BatteryStatsImpl::GpuTimer::stopRunningLocked(const android::sp<BatteryStatsImpl>& stats) {
    GLOGENTRY();

    if (DEBUG_GPU) {
        GLOGD("GpuTimer stopRunningLocked mNesting = %d : mUpdateTime = %lld ,mTotalTime = %lld, mCount = %d, mAcquireTime = %ldd", mNesting, mUpdateTime, mTotalTime, mCount, mAcquireTime);
    }

    // Ignore attempt to stop a timer that isn't running
    if (mNesting == 0) {
        return;
    }

    if (--mNesting == 0) {
        int64_t realtime = elapsedRealtime() * 1000;
        int64_t batteryRealtime = stats->getBatteryRealtimeLocked(realtime);

        if (DEBUG_GPU) {
            GLOGD("batteryRealtime = %lld", batteryRealtime);
        }
        sp<GArrayList<GpuTimer> > tmp_mTimerPool = mTimerPool.promote();
        NULL_RTN(tmp_mTimerPool, "Promotion to sp<GpuTimer> fails");

        if (tmp_mTimerPool != NULL) {
            // Accumulate time to all active counters, scaled by the total
            // active in the pool, before taking this one out of the pool.
            refreshTimersLocked(stats, mTimerPool);
            // Remove this timer from the active pool
            tmp_mTimerPool->remove(this);
        } else {
            mNesting = 1;
            mTotalTime = computeRunTimeLocked(batteryRealtime);
            mNesting = 0;
        }

        if (DEBUG_GPU) {
            GLOGD("GpuTimer Stop After # %d : mUpdateTime = %lld, mTotalTime = %lld, mCount = %d, mAcquireTime = %lld", mType, mUpdateTime, mTotalTime, mCount, mAcquireTime);
        }

        if (mTotalTime == mAcquireTime) {
            // If there was no change in the time, then discard this
            // count.  A somewhat cheezy strategy, but hey.
            mCount--;
        }

        if (mUpdateTime != batteryRealtime) {
            updateGpuSpeedStepTimes();
        }
    }
}
// Customize ---

// Customize +++
void BatteryStatsImpl::GpuTimer::refreshTimersLocked(const android::sp<BatteryStatsImpl>& stats,
                                                android::wp<GArrayList<GpuTimer> >& pool) {
    GLOGENTRY();

    sp<GArrayList<GpuTimer> > tmp_pool = pool.promote();
    NULL_RTN(tmp_pool, "Promotion to sp<GpuTimer> fails");

    int64_t realtime = elapsedRealtime() * 1000;
    int64_t batteryRealtime = stats->getBatteryRealtimeLocked(realtime);
    int32_t N = tmp_pool->size();

    for (int32_t i = N-1; i >= 0; i--) {
        sp<GpuTimer> t = tmp_pool->get(i);
        int64_t heldTime = batteryRealtime - t->mUpdateTime;

        if (heldTime > 0) {
            t->mTotalTime += heldTime / N;
        }

        t->mUpdateTime = batteryRealtime;
    }
}
// Customize ---

// Customize +++
int32_t BatteryStatsImpl::GpuTimer::checkTimerPoolSize() {
    GLOGENTRY();

    sp<GArrayList<GpuTimer> > tmp_mTimerPool= mTimerPool.promote();

    int32_t tmpSize = 1;

    if (tmp_mTimerPool != NULL) {
        tmpSize = tmp_mTimerPool->size();

        if (tmpSize <= 0) {
            GLOGW("Timer Pool size <=0");
            tmpSize = 1;
        }
    }
    return tmpSize;
}
// Customize ---

// Customize +++
int64_t BatteryStatsImpl::GpuTimer::computeRunTimeLocked(int64_t curBatteryRealtime) {
    GLOGENTRY();

    sp<GArrayList<GpuTimer> > tmp_mTimerPool = mTimerPool.promote();

    if (DEBUG_GPU) {
        GLOGD("GpuTimer computeRunTimeLocked curBatteryRealtime = %lld : mTimeout = %lld, mUpdateTime = %lld, mTotalTime = %lld, mNesting = %d", curBatteryRealtime, mTimeout, mUpdateTime, mTotalTime, mNesting);
    }

    if (mTimeout > 0 && curBatteryRealtime > mUpdateTime + mTimeout) {
        curBatteryRealtime = mUpdateTime + mTimeout;
    }

    return mTotalTime + (mNesting > 0 ?
            (curBatteryRealtime - mUpdateTime) / ((tmp_mTimerPool != NULL) ? checkTimerPoolSize() : 1) 
            : 0);
}
// Customize ---

// Customize +++
int32_t BatteryStatsImpl::GpuTimer::computeCurrentCountLocked() {
    GLOGENTRY();

    if (DEBUG_GPU) {
        GLOGD("GpuTimer computeCurrentCountLocked mCount = %d", mCount);
    }

    return mCount;
}
// Customize ---

// Customize +++
bool BatteryStatsImpl::GpuTimer::reset(const android::sp<BatteryStatsImpl>& stats, bool detachIfReset) {
    GLOGENTRY();

    if (DEBUG_GPU) {
        GLOGD("GpuTimer reset detachIfReset = %d", detachIfReset);
    }

    bool canDetach = mNesting <= 0;
    Timer::reset(stats, canDetach && detachIfReset);

    if (mNesting > 0) {
        mUpdateTime = stats->getBatteryRealtimeLocked(elapsedRealtime() * 1000);

        if (mGpuSpeedTimes == 0) {
            mGpuSpeedTimes = ProcessStats::getGpuSpeedTimes(NULL);
            mRelGpuSpeedTimes = new Blob<int64_t>(sizeof(mGpuSpeedTimes) / sizeof(mGpuSpeedTimes[0]));

            for (int32_t i = 0; i < static_cast<int32_t>(sizeof(mGpuSpeedTimes) / sizeof(mGpuSpeedTimes[0])); i++) {
                mRelGpuSpeedTimes[i] = 0;  // Initialize
            }
        } else {
            ProcessStats::getGpuSpeedTimes(mGpuSpeedTimes);
        }

        if (DEBUG_GPU) {
            for (int32_t i = 0; i < static_cast<int32_t>(sizeof(mGpuSpeedTimes) / sizeof(mGpuSpeedTimes[0])); i++) {
                GLOGD("reset mGpuSpeedTimes i = %lld", mGpuSpeedTimes[i]);
            }
        }
    }

    mAcquireTime = mTotalTime;

    if (mTotalGpuSpeedTimes != NULL) {
        for (int32_t i = 0; i < static_cast<int32_t>(sizeof(mTotalGpuSpeedTimes) / sizeof(mTotalGpuSpeedTimes[0])); i++) {
            mTotalGpuSpeedTimes[i] = 0;  // Initialize
        }
    }

    return canDetach;
}
// Customize ---

// Customize +++
void BatteryStatsImpl::GpuTimer::attach() {
    GLOGENTRY();

    if (DEBUG_SECURITY) {
        GLOGD("Gpu attach");
    }

    Timer::attach();
}
// Customize ---

// Customize +++
void BatteryStatsImpl::GpuTimer::detach() {
    GLOGENTRY();

    sp<GArrayList<GpuTimer> > tmp_mTimerPool= mTimerPool.promote();

    if (DEBUG_GPU) {
        GLOGD("GpuTimer detach");
    }

    Timer::detach();

    if (tmp_mTimerPool != NULL) {
        tmp_mTimerPool->remove(this);
    }
}
// Customize ---

// Customize +++
void BatteryStatsImpl::GpuTimer::readSummaryFromParcelLocked(android::Parcel& in) {  // NOLINT
    GLOGENTRY();

    Timer::readSummaryFromParcelLocked(in);
    mNesting = 0;

    int32_t bins = in.readInt32();
    int32_t steps = getGpuSpeedSteps();

    if (DEBUG_GPU) {
        GLOGD("GpuTimer readSummaryFromParcelLocked bins = %d : steps = %d", bins, steps);
    }

    if ((bins < 0) || (bins > MAX_GPU_SPEED_NUM)) {
        bins = 0;
    }

    mTotalGpuSpeedTimes = new Blob<int64_t>(bins >= steps ? bins : steps);

    for (int32_t i = 0; i < bins; i++) {
        mTotalGpuSpeedTimes[i] = in.readInt64();
    }

    if (bins < steps) {
        for (int32_t j = bins; j < steps; j++) {
            mTotalGpuSpeedTimes[j] = 0;  // Initialize
        }
    }
}
// Customize ---

// Customize +++
void BatteryStatsImpl::GpuTimer::writeSummaryFromParcelLocked(android::Parcel* out, int64_t batteryRealtime) {
    GLOGENTRY();

    Timer::writeSummaryFromParcelLocked(out, batteryRealtime);

    int32_t bins = sizeof(mTotalGpuSpeedTimes) / sizeof(mTotalGpuSpeedTimes[0]);

    if (DEBUG_GPU) {
        GLOGD("GpuTimer writeSummaryFromParcelLocked bins = %d", bins);
    }

    if ((bins < 0) || (bins > MAX_GPU_SPEED_NUM)) {
        bins = 0;
    }

    out->writeInt32(bins);

    for (int32_t i = 0; i < bins; i++) {
        out->writeInt64(mTotalGpuSpeedTimes[i]);
    }
}
// Customize ---

// Customize +++
void BatteryStatsImpl::GpuTimer::addGpuSpeedStepTimes(android::sp<Blob<int64_t> >& values) {  // NOLINT
    GLOGENTRY();

    if (DEBUG_GPU) {
        GLOGD("GpuTimer addGpuSpeedStepTimes");
    }

    if (values == 0) {
        return;
    }

    int32_t totalLen = 0;

    if (mTotalGpuSpeedTimes == 0) {
        totalLen = getGpuSpeedSteps();

        mTotalGpuSpeedTimes = new Blob<int64_t>(totalLen);

        for (int32_t i = 0; i < totalLen; i++) {
            mTotalGpuSpeedTimes[i] = 0;  // Initialize
        }

    } else {
        totalLen = sizeof(mTotalGpuSpeedTimes) / sizeof(mTotalGpuSpeedTimes[0]);
    }

    int64_t amt = 0;
    int32_t valueLen = sizeof(values) / sizeof(values[0]);

    for (int32_t i = 0; i < totalLen && i < valueLen; i++) {
        amt = values[i];

        if (amt > 0) {
            mTotalGpuSpeedTimes[i] += amt;
        }
    }
}
// Customize ---

// Customize +++
void BatteryStatsImpl::GpuTimer::updateGpuSpeedStepTimes() {
    GLOGENTRY();

    if (mRelGpuSpeedTimes == 0) {
        mRelGpuSpeedTimes = ProcessStats::getGpuSpeedTimes(NULL);
    } else {
        ProcessStats::getGpuSpeedTimes(mRelGpuSpeedTimes);
    }

    if (DEBUG_GPU) {
        for (int32_t i = 0; i < static_cast<int32_t>(sizeof(mRelGpuSpeedTimes) / sizeof(mRelGpuSpeedTimes[0])); i++) {
            GLOGD("update1 mRelGpuSpeedTimes i = %lld", mRelGpuSpeedTimes[i]);
            GLOGD("update1 mGpuSpeedTimes i = %lld", mGpuSpeedTimes[i]);
        }
    }

    int64_t temp = 0;
    int32_t mGpuSpeedLen = sizeof(mGpuSpeedTimes) / sizeof(mGpuSpeedTimes[0]);
    int32_t mRelGpuSpeedLen = sizeof(mRelGpuSpeedTimes) / sizeof(mRelGpuSpeedTimes[0]);
    int32_t count = 0;

    for (int32_t i = 0; i < mGpuSpeedLen && i < mRelGpuSpeedLen; i++) {
        temp = mRelGpuSpeedTimes[i];

        if (temp < mGpuSpeedTimes[i]) {
            // TODO
            // Log::e(TAG, "Error current Gpu time (" + temp + ") < last Gpu time (" + mGpuSpeedTimes[i] + ")");
        }

        mRelGpuSpeedTimes[i] -= mGpuSpeedTimes[i];
        mGpuSpeedTimes[i] = temp;
        count++;
    }

    if (DEBUG_GPU) {
        for (int32_t i = 0; i < mGpuSpeedLen && i < mRelGpuSpeedLen; i++) {
            GLOGD("update2 mGpuSpeedTimes i = %lld ", mGpuSpeedTimes[i]);
            GLOGD("update2 mRelGpuSpeedTimes i = %lld", mRelGpuSpeedTimes[i]);
        }
    }

    for (int32_t j = count; j < mRelGpuSpeedLen; j++) {
        mRelGpuSpeedTimes[j] = 0;
    }

    addGpuSpeedStepTimes(mRelGpuSpeedTimes);
}
// Customize ---

// Customize +++
int64_t BatteryStatsImpl::GpuTimer::getTimeAtGpuSpeedStep(int32_t speedStep, int32_t which) {
    GLOGENTRY();

    if (DEBUG_GPU) {
        GLOGD("GpuTimer getTimeAtGpuSpeedStep speedStep = %d, which = %d", speedStep, which);
    }

    if (mTotalGpuSpeedTimes == 0) {
        return 0;
    }

    if (speedStep < 0) {
        return 0;
    }

    if (speedStep < static_cast<int32_t>(sizeof(mTotalGpuSpeedTimes) / sizeof(mTotalGpuSpeedTimes[0]))) {
        if (DEBUG_GPU) {
            GLOGD("GpuTimer getTimeAtGpuSpeedStep speedStep = %d  ,which = %d ,value = %lld", speedStep ,which ,mTotalGpuSpeedTimes[speedStep]);
        }

        return mTotalGpuSpeedTimes[speedStep];
    } else {
        return 0;
    }
}
// Customize ---

BatteryStatsImpl::KernelWakelockStats::KernelWakelockStats(int32_t count, int64_t totalTime, int32_t version)
    :PREINIT_DYNAMIC() {
    GLOGENTRY();
    android::CTOR_SAFE;

    mCount = count;
    mTotalTime = totalTime;
    mVersion = version;
}

BatteryStatsImpl::Uid::Uid(const wp<BatteryStatsImpl>& parent, int32_t uid)
    :PREINIT_DYNAMIC(),
    mCtorSafe(new ctor_safe(this)),
    mUid(uid),
    mStartedTcpBytesReceived(-1),
    mStartedTcpBytesSent(-1),
    mBS(parent) {
    GLOGENTRY();

    sp<BatteryStatsImpl> mbs = mBS.promote();
    NULL_RTN(mbs, "Promotion to sp<BatteryStatsImpl> fails");

    //  Customize +++
    if (DEBUG) {
        GLOGV("Uid constructor uid = %d", uid);
    }
    //  Customize ---

#ifdef WIFI_ENABLE
    mWifiRunningTimer = NULL;
    mFullWifiLockTimer = NULL;
    mScanWifiLockTimer = NULL;
    mWifiMulticastTimer = NULL;
#endif  // WIFI_ENABLE

    mAudioTurnedOnTimer = NULL;
    mVideoTurnedOnTimer = NULL;

    //  Customize +++
    mGpuTurnedOn = false;
    mGpuTurnedOnTimer = NULL;
    mCurrentBrightnesslevel = 0;
    mDisplayTurnedOn = false;
    mDisplayTurnedOnTimer = NULL;
    mDisplayBrightnessTimer = new Blob<sp<StopwatchTimer> >(BatteryStats::NUM_SCREEN_BRIGHTNESS_BINS);

    mGpuTurnedOnTimer = new GpuTimer(mBS,
                                     wp<Uid>(this),
                                     BatteryStats::GPU_TURNED_ON,
                                     NULL,
                                     mbs->mUnpluggables);

    mDisplayTurnedOnTimer = new StopwatchTimer(mBS,
                                               wp<Uid>(this),
                                               BatteryStats::DISPLAY_TURNED_ON,
                                               NULL,
                                               mbs->mUnpluggables);

    for (int32_t i = 0; i < BatteryStats::NUM_SCREEN_BRIGHTNESS_BINS; i++) {
        mDisplayBrightnessTimer[i] = new StopwatchTimer(mBS,
                                                        wp<Uid>(this),
                                                        1000+i,
                                                        NULL,
                                                        mbs->mUnpluggables);
    }
    //  Customize ---

    mUserActivityCounters = NULL;

    mLoadedTcpBytesReceived = 0;
    mLoadedTcpBytesSent = 0;
    mCurrentTcpBytesReceived = 0;
    mCurrentTcpBytesSent = 0;
    mTcpBytesReceivedAtLastUnplug = 0;
    mTcpBytesSentAtLastUnplug = 0;

#ifdef WIFI_ENABLE
    mWifiRunning = 0;
    mFullWifiLockOut = 0;
    mScanWifiLockOut = 0;
    mWifiMulticastEnabled = 0;
#endif  // WIFI_ENABLE

    mAudioTurnedOn = 0;
    mVideoTurnedOn = 0;

#ifdef WIFI_ENABLE
    mWifiRunningTimer = new StopwatchTimer(mBS, wp<Uid>(this), BatteryStats::WIFI_RUNNING, mbs->mWifiRunningTimers, mbs->mUnpluggables);
    mFullWifiLockTimer = new StopwatchTimer(mBS, wp<Uid>(this), BatteryStats::FULL_WIFI_LOCK, mbs->mFullWifiLockTimers, mbs->mUnpluggables);
    mScanWifiLockTimer = new StopwatchTimer(mBS, wp<Uid>(this), BatteryStats::SCAN_WIFI_LOCK, mbs->mScanWifiLockTimers, mbs->mUnpluggables);
    mWifiMulticastTimer = new StopwatchTimer(mBS, wp<Uid>(this), BatteryStats::WIFI_MULTICAST_ENABLED, mbs->mWifiMulticastTimers, mbs->mUnpluggables);
#endif  // WIFI_ENABLE

    mAudioTurnedOnTimer = new StopwatchTimer(mBS, wp<Uid>(this), BatteryStats::AUDIO_TURNED_ON, NULL, mbs->mUnpluggables);
    mVideoTurnedOnTimer = new StopwatchTimer(mBS, wp<Uid>(this), BatteryStats::VIDEO_TURNED_ON, NULL, mbs->mUnpluggables);
    delete mCtorSafe;
}

KeyedVector<sp<String> , sp<BatteryStats::Uid::Wakelock> >& BatteryStatsImpl::Uid::getWakelockStats() {
    GLOGENTRY();

    return mWakelockStats;
}

#ifdef SENSOR_ENABLE
KeyedVector<int32_t, sp<BatteryStats::Uid::Sensor> >& BatteryStatsImpl::Uid::getSensorStats() {
    GLOGENTRY();

    return mSensorStats;
}
#endif  // SENSOR_ENABLE

KeyedVector<sp<String> , sp<BatteryStats::Uid::Proc> >& BatteryStatsImpl::Uid::getProcessStats() {
    GLOGENTRY();

    return mProcessStats;
}

KeyedVector<sp<String> , sp<BatteryStats::Uid::Pkg> >& BatteryStatsImpl::Uid::getPackageStats() {
    GLOGENTRY();

    return mPackageStats;
}

int32_t BatteryStatsImpl::Uid::getUid() {
    GLOGENTRY();

    return mUid;
}

int64_t BatteryStatsImpl::Uid::getTcpBytesReceived(int32_t which) {
    GLOGENTRY();

    if (which == STATS_LAST) {
        return mLoadedTcpBytesReceived;
    } else {
        int64_t current = computeCurrentTcpBytesReceived();

        if (which == STATS_SINCE_UNPLUGGED) {
            current -= mTcpBytesReceivedAtLastUnplug;
        } else if (which == STATS_SINCE_CHARGED) {
            current += mLoadedTcpBytesReceived;
        }

        return current;
    }
}

int64_t BatteryStatsImpl::Uid::computeCurrentTcpBytesReceived() {
    GLOGENTRY();

    sp<BatteryStatsImpl> mbs = mBS.promote();
    NULL_RTN_VAL(mbs, 0, "Promotion to sp<BatteryStatsImpl> fails");
    const int64_t uidRxBytes = mbs->getNetworkStatsDetailGroupedByUid()->getTotal(NULL, mUid)->mrxBytes;
    return mCurrentTcpBytesReceived + (mStartedTcpBytesReceived >= 0
                                       ? (uidRxBytes - mStartedTcpBytesReceived) : 0);
}

int64_t BatteryStatsImpl::Uid::getTcpBytesSent(int32_t which) {
    GLOGENTRY();

    if (which == STATS_LAST) {
        return mLoadedTcpBytesSent;
    } else {
        int64_t current = computeCurrentTcpBytesSent();

        if (which == STATS_SINCE_UNPLUGGED) {
            current -= mTcpBytesSentAtLastUnplug;
        } else if (which == STATS_SINCE_CHARGED) {
            current += mLoadedTcpBytesSent;
        }

        return current;
    }
}

#ifdef WIFI_ENABLE
void BatteryStatsImpl::Uid::noteWifiRunningLocked() {
    GLOGENTRY();

    sp<BatteryStatsImpl> mbs = mBS.promote();
    NULL_RTN(mbs, "Promotion to sp<BatteryStatsImpl> fails");

    if (!mWifiRunning) {
        mWifiRunning = true;

        if (mWifiRunningTimer == NULL) {
            mWifiRunningTimer = new StopwatchTimer(mBS, wp<Uid>(this), BatteryStats::WIFI_RUNNING,
                                                   mbs->mWifiRunningTimers, mbs->mUnpluggables);
        }

        mWifiRunningTimer->startRunningLocked(mbs);
    }
}

void BatteryStatsImpl::Uid::noteWifiStoppedLocked() {
    GLOGENTRY();

    sp<BatteryStatsImpl> mbs = mBS.promote();
    NULL_RTN(mbs, "Promotion to sp<BatteryStatsImpl> fails");

    if (mWifiRunning) {
        mWifiRunning = false;
        mWifiRunningTimer->stopRunningLocked(mbs);
    }
}

void BatteryStatsImpl::Uid::noteFullWifiLockAcquiredLocked() {
    GLOGENTRY();

    sp<BatteryStatsImpl> mbs = mBS.promote();
    NULL_RTN(mbs, "Promotion to sp<BatteryStatsImpl> fails");

    if (!mFullWifiLockOut) {
        mFullWifiLockOut = true;

        if (mFullWifiLockTimer == NULL) {
             mFullWifiLockTimer = new StopwatchTimer(mBS, wp<Uid>(this), BatteryStats::FULL_WIFI_LOCK,
             mbs->mFullWifiLockTimers, mbs->mUnpluggables);
        }

        mFullWifiLockTimer->startRunningLocked(mbs);
    }
}

void BatteryStatsImpl::Uid::noteFullWifiLockReleasedLocked() {
    GLOGENTRY();

    sp<BatteryStatsImpl> mbs = mBS.promote();
    NULL_RTN(mbs, "Promotion to sp<BatteryStatsImpl> fails");

    if (mFullWifiLockOut) {
        mFullWifiLockOut = false;
        mFullWifiLockTimer->stopRunningLocked(mbs);
    }
}

void BatteryStatsImpl::Uid::noteScanWifiLockAcquiredLocked() {
    GLOGENTRY();

    sp<BatteryStatsImpl> mbs = mBS.promote();
    NULL_RTN(mbs, "Promotion to sp<BatteryStatsImpl> fails");

    if (!mScanWifiLockOut) {
        mScanWifiLockOut = true;

        if (mScanWifiLockTimer == NULL) {
            mScanWifiLockTimer = new StopwatchTimer(mBS, wp<Uid>(this), BatteryStats::SCAN_WIFI_LOCK,
            mbs->mScanWifiLockTimers, mbs->mUnpluggables);
        }

        mScanWifiLockTimer->startRunningLocked(mbs);
    }
}

void BatteryStatsImpl::Uid::noteScanWifiLockReleasedLocked() {
    GLOGENTRY();

    sp<BatteryStatsImpl> mbs = mBS.promote();
    NULL_RTN(mbs, "Promotion to sp<BatteryStatsImpl> fails");

    if (mScanWifiLockOut) {
        mScanWifiLockOut = false;
        mScanWifiLockTimer->stopRunningLocked(mbs);
    }
}

void BatteryStatsImpl::Uid::noteWifiMulticastEnabledLocked() {
    GLOGENTRY();

    sp<BatteryStatsImpl> mbs = mBS.promote();
    NULL_RTN(mbs, "Promotion to sp<BatteryStatsImpl> fails");

    if (!mWifiMulticastEnabled) {
        mWifiMulticastEnabled = true;

        if (mWifiMulticastTimer == NULL) {
            mWifiMulticastTimer = new StopwatchTimer(mBS, wp<Uid>(this), BatteryStats::WIFI_MULTICAST_ENABLED,
            mbs->mWifiMulticastTimers, mbs->mUnpluggables);
        }

        mWifiMulticastTimer->startRunningLocked(mbs);
    }
}

void BatteryStatsImpl::Uid::noteWifiMulticastDisabledLocked() {
    GLOGENTRY();

    sp<BatteryStatsImpl> mbs = mBS.promote();
    NULL_RTN(mbs, "Promotion to sp<BatteryStatsImpl> fails");

    if (mWifiMulticastEnabled) {
        mWifiMulticastEnabled = false;
        mWifiMulticastTimer->stopRunningLocked(mbs);
    }
}
#endif  // WIFI_ENABLE

void BatteryStatsImpl::Uid::noteAudioTurnedOnLocked() {
    GLOGENTRY();

    sp<BatteryStatsImpl> mbs = mBS.promote();
    NULL_RTN(mbs, "Promotion to sp<BatteryStatsImpl> fails");

    if (!mAudioTurnedOn) {
        mAudioTurnedOn = true;

        if (mAudioTurnedOnTimer == NULL) {
            mAudioTurnedOnTimer = new StopwatchTimer(mBS, wp<Uid>(this), BatteryStats::AUDIO_TURNED_ON,
            NULL, mbs->mUnpluggables);
        }

         mAudioTurnedOnTimer->startRunningLocked(mbs);
    }
}

void BatteryStatsImpl::Uid::noteAudioTurnedOffLocked() {
    GLOGENTRY();

    sp<BatteryStatsImpl> mbs = mBS.promote();
    NULL_RTN(mbs, "Promotion to sp<BatteryStatsImpl> fails");

    if (mAudioTurnedOn) {
        mAudioTurnedOn = false;
        mAudioTurnedOnTimer->stopRunningLocked(mbs);
    }
}

void BatteryStatsImpl::Uid::noteVideoTurnedOnLocked() {
    GLOGENTRY();

    sp<BatteryStatsImpl> mbs = mBS.promote();
    NULL_RTN(mbs, "Promotion to sp<BatteryStatsImpl> fails");

    if (!mVideoTurnedOn) {
        mVideoTurnedOn = true;

        if (mVideoTurnedOnTimer == NULL) {
            mVideoTurnedOnTimer = new StopwatchTimer(mBS,
                                                     wp<Uid>(this),
                                                     BatteryStats::VIDEO_TURNED_ON,
                                                     NULL,
                                                     mbs->mUnpluggables);
        }

        mVideoTurnedOnTimer->startRunningLocked(mbs);
    }
}

void BatteryStatsImpl::Uid::noteVideoTurnedOffLocked() {
    GLOGENTRY();

    sp<BatteryStatsImpl> mbs = mBS.promote();
    NULL_RTN(mbs, "Promotion to sp<BatteryStatsImpl> fails");

    if (mVideoTurnedOn) {
        mVideoTurnedOn = false;
        mVideoTurnedOnTimer->stopRunningLocked(mbs);
    }
}

// Customize +++
void BatteryStatsImpl::Uid::noteGpuTurnedOnLocked() {
    GLOGENTRY();

    sp<BatteryStatsImpl> mbs = mBS.promote();
    NULL_RTN(mbs, "Promotion to sp<BatteryStatsImpl> fails");

    if (DEBUG_GPU) {
        GLOGD("Uid noteGpuTurnedOnLocked mGpuTurnedOn = %d", mGpuTurnedOn);
    }

    if (!mGpuTurnedOn) {
        mGpuTurnedOn = true;

        if (mGpuTurnedOnTimer == NULL) {
            mGpuTurnedOnTimer = new GpuTimer(mBS,
                                             wp<Uid>(this),
                                             BatteryStats::GPU_TURNED_ON,
                                             NULL,
                                             mbs->mUnpluggables);
        }
        mGpuTurnedOnTimer->startRunningLocked(mbs);
    }
}
// Customize ---

// Customize +++
void BatteryStatsImpl::Uid::noteGpuTurnedOffLocked() {
    GLOGENTRY();

    sp<BatteryStatsImpl> mbs = mBS.promote();
    NULL_RTN(mbs, "Promotion to sp<BatteryStatsImpl> fails");

    if (DEBUG_GPU) {
        GLOGD("Uid noteGpuTurnedOffLocked mGpuTurnedOn = %d", mGpuTurnedOn);
    }

    if (mGpuTurnedOn) {
        mGpuTurnedOn = false;
        mGpuTurnedOnTimer->stopRunningLocked(mbs);
    }
}
// Customize ---

// Customize +++
void BatteryStatsImpl::Uid::noteDisplayTurnedOnLocked() {
    GLOGENTRY();

    sp<BatteryStatsImpl> mbs = mBS.promote();
    NULL_RTN(mbs, "Promotion to sp<BatteryStatsImpl> fails");

    if (!mbs->mScreenOn) {
        return;
    }

    if (!mDisplayTurnedOn) {
        mDisplayTurnedOn = true;
        if (mDisplayTurnedOnTimer == NULL) {
            mDisplayTurnedOnTimer = new StopwatchTimer(mBS,
                                                       wp<Uid>(this),
                                                       BatteryStats::DISPLAY_TURNED_ON,
                                                       NULL,
                                                       mbs->mUnpluggables);
        }
        mDisplayTurnedOnTimer->startRunningLocked(mbs);

        mCurrentBrightnesslevel = mbs->mScreenBrightnessBin;

        if ((mCurrentBrightnesslevel >= 0) && (mCurrentBrightnesslevel < NUM_SCREEN_BRIGHTNESS_BINS)) {
            if (mDisplayBrightnessTimer[mCurrentBrightnesslevel] == NULL) {
                mDisplayBrightnessTimer[mCurrentBrightnesslevel] = new StopwatchTimer(mBS,
                                                                                      wp<Uid>(this),
                                                                                      1000 + mCurrentBrightnesslevel,
                                                                                      NULL,
                                                                                      mbs->mUnpluggables);
            }
            mDisplayBrightnessTimer[mCurrentBrightnesslevel]->startRunningLocked(mbs);
        }
    }
}
// Customize ---

// Customize +++
void BatteryStatsImpl::Uid::noteDisplayTurnedOffLocked() {
    GLOGENTRY();

    sp<BatteryStatsImpl> mbs = mBS.promote();
    NULL_RTN(mbs, "Promotion to sp<BatteryStatsImpl> fails");

    if (mDisplayTurnedOn) {
        mDisplayTurnedOn = false;
        mDisplayTurnedOnTimer->stopRunningLocked(mbs);

        if (mDisplayBrightnessTimer != NULL) {
            int32_t bin = static_cast<int32_t>(sizeof(mDisplayBrightnessTimer) / sizeof(mDisplayBrightnessTimer[0]));
            for (int32_t i = 0; i < bin; i++) {
                if (mDisplayBrightnessTimer[i] != NULL) {
                    mDisplayBrightnessTimer[i]->stopRunningLocked(mbs);
                }
            }
        }

        /*
        if ((mScreenBrightnessBin >= 0)&&(mScreenBrightnessBin < NUM_SCREEN_BRIGHTNESS_BINS)) {
            if (mDisplayBrightnessTimer[mScreenBrightnessBin] != null) {
                mDisplayBrightnessTimer[mScreenBrightnessBin].stopRunningLocked(BatteryStatsImpl.this);
            }
        }
        */
    }
}
// Customize ---

// Customize +++
void BatteryStatsImpl::Uid::noteDisplayBrightnessLocked(int32_t bin) {
    GLOGENTRY();

    sp<BatteryStatsImpl> mbs = mBS.promote();
    NULL_RTN(mbs, "Promotion to sp<BatteryStatsImpl> fails");

    if (bin < 0) {
        bin = 0;
    } else if (bin >= NUM_SCREEN_BRIGHTNESS_BINS) {
        bin = NUM_SCREEN_BRIGHTNESS_BINS-1;
    }

    if (mDisplayTurnedOn) {
        if ((mCurrentBrightnesslevel >= 0)&&(mCurrentBrightnesslevel < NUM_SCREEN_BRIGHTNESS_BINS)) {
            if (mDisplayBrightnessTimer[mCurrentBrightnesslevel] != NULL) {
                mDisplayBrightnessTimer[mCurrentBrightnesslevel]->stopRunningLocked(mbs);
            }
        }

        if (mDisplayBrightnessTimer[bin] == NULL) {
            mDisplayBrightnessTimer[bin] = new StopwatchTimer(mBS,
                                                              wp<Uid>(this),
                                                              1000 + bin,
                                                              NULL,
                                                              mbs->mUnpluggables);
        }

        mDisplayBrightnessTimer[bin]->startRunningLocked(mbs);
        mCurrentBrightnesslevel = bin;
    }
}
// Customize ---

#ifdef WIFI_ENABLE
int64_t BatteryStatsImpl::Uid::getWifiRunningTime(int64_t batteryRealtime, int32_t which) {
    GLOGENTRY();

    if (mWifiRunningTimer == NULL) {
        return 0;
    }

    return mWifiRunningTimer->getTotalTimeLocked(batteryRealtime, which);
}

int64_t BatteryStatsImpl::Uid::getFullWifiLockTime(int64_t batteryRealtime, int32_t which) {
    GLOGENTRY();

    if (mFullWifiLockTimer == NULL) {
        return 0;
    }

    return mFullWifiLockTimer->getTotalTimeLocked(batteryRealtime, which);
}

int64_t BatteryStatsImpl::Uid::getScanWifiLockTime(int64_t batteryRealtime, int32_t which) {
    GLOGENTRY();

    if (mScanWifiLockTimer == NULL) {
        return 0;
    }

    return mScanWifiLockTimer->getTotalTimeLocked(batteryRealtime, which);
}

int64_t BatteryStatsImpl::Uid::getWifiMulticastTime(int64_t batteryRealtime, int32_t which) {
    GLOGENTRY();

    if (mWifiMulticastTimer == NULL) {
        return 0;
    }

    return mWifiMulticastTimer->getTotalTimeLocked(batteryRealtime, which);
}
#endif  // WIFI_ENABLE

int64_t BatteryStatsImpl::Uid::getAudioTurnedOnTime(int64_t batteryRealtime, int32_t which) {
    GLOGENTRY();

    if (mAudioTurnedOnTimer == NULL) {
        return 0;
    }

    return mAudioTurnedOnTimer->getTotalTimeLocked(batteryRealtime, which);
}

int64_t BatteryStatsImpl::Uid::getVideoTurnedOnTime(int64_t batteryRealtime, int32_t which) {
    GLOGENTRY();

    if (mVideoTurnedOnTimer == NULL) {
        return 0;
    }

    return mVideoTurnedOnTimer->getTotalTimeLocked(batteryRealtime, which);
}

// Customize +++
int64_t BatteryStatsImpl::Uid::getTimeAtGpuSpeedStep(int32_t speedStep, int32_t which) {
    GLOGENTRY();

    if (mGpuTurnedOnTimer == NULL) {
        if (DEBUG_ON) {
            GLOGD("getTimeAtGpuSpeedStep no Timer and Uid = %d", mUid);
        }

        return 0;
    }

    if (DEBUG_GPU) {
        GLOGD("getTimeAtGpuSpeedStep Uid = %d", mUid);
    }

    return mGpuTurnedOnTimer->getTimeAtGpuSpeedStep(speedStep, which);
}
// Customize ---

// Customize +++
int64_t BatteryStatsImpl::Uid::getDisplayTurnedOnTime(int64_t batteryRealtime, int32_t which) {
    GLOGENTRY();

    if (mDisplayTurnedOnTimer == NULL) {
        if (DEBUG_ON) {
            GLOGD("getDisplayTurnedOnTime no Timer and Uid = %d", mUid);
        }

        return 0;
    }

    if (DEBUG_GPU) {
        GLOGD("getDisplayTurnedOnTime Uid = %d", mUid);
    }

    return mDisplayTurnedOnTimer->getTotalTimeLocked(batteryRealtime, which);
}
// Customize ---

// Customize +++
int64_t BatteryStatsImpl::Uid::getDisplayBrightnessTime(int32_t brightnessBin, int64_t batteryRealtime, int32_t which) {
    GLOGENTRY();

    int32_t bin = brightnessBin;
    if (brightnessBin < 0) {
        bin = 0;
    } else if (brightnessBin >= NUM_SCREEN_BRIGHTNESS_BINS) {
        bin = NUM_SCREEN_BRIGHTNESS_BINS-1;
    }

    if (mDisplayBrightnessTimer[bin] == NULL) {
        if (DEBUG_ON) {
            GLOGD("getDisplayBrightnessTime no Timer and Uid = %d", mUid);
        }

        return 0;
    }

    if (DEBUG_GPU) {
        GLOGD("getDisplayBrightnessTime Uid = %d, bin = %d", mUid, bin);
    }

    return mDisplayBrightnessTimer[bin]->getTotalTimeLocked(batteryRealtime, which);
}
// Customize ---

void BatteryStatsImpl::Uid::noteUserActivityLocked(int32_t type) {
    GLOGENTRY();

    if (mUserActivityCounters == NULL) {
        initUserActivityLocked();
    }

    if (type < 0) {
        type = 0;
    } else if (type >= BatteryStats::Uid::NUM_USER_ACTIVITY_TYPES) {
        type = BatteryStats::Uid::NUM_USER_ACTIVITY_TYPES - 1;
    }

    if (mUserActivityCounters != NULL) {
        (*mUserActivityCounters)[type]->stepAtomic();
    }
}

bool BatteryStatsImpl::Uid::hasUserActivity() {
    GLOGENTRY();

    return mUserActivityCounters != NULL;
}

int32_t BatteryStatsImpl::Uid::getUserActivityCount(int32_t type, int32_t which) {
    GLOGENTRY();

    if (mUserActivityCounters == NULL) {
        return 0;
    }
    return (*mUserActivityCounters)[type]->getCountLocked(which);
}

void BatteryStatsImpl::Uid::initUserActivityLocked() {
    GLOGENTRY();

    sp<BatteryStatsImpl> mbs = mBS.promote();
    NULL_RTN(mbs, "Promotion to sp<BatteryStatsImpl> fails");

    mUserActivityCounters = new Blob<sp<Counter> >(BatteryStats::Uid::NUM_USER_ACTIVITY_TYPES);

    for (int32_t i = 0; i < BatteryStats::Uid::NUM_USER_ACTIVITY_TYPES; i++) {
        (*mUserActivityCounters)[i] = new Counter(mBS, mbs->mUnpluggables, 0);
    }
}

int64_t BatteryStatsImpl::Uid::computeCurrentTcpBytesSent() {
    GLOGENTRY();

    sp<BatteryStatsImpl> mbs = mBS.promote();
    NULL_RTN_VAL(mbs, 0, "Promotion to sp<BatteryStatsImpl> fails");

    const int64_t uidTxBytes = mbs->getNetworkStatsDetailGroupedByUid()->getTotal(NULL, mUid)->mtxBytes;
    return mCurrentTcpBytesSent + (mStartedTcpBytesSent >= 0
                                   ? (uidTxBytes - mStartedTcpBytesSent) : 0);
}

// Customize +++
bool BatteryStatsImpl::Uid::isRunning() {
    GLOGENTRY();

    bool active = false;
    int64_t startTimeMillis = 0;
    int64_t tempTime = 0;
    int32_t wakelockStatsSize = 0;
    int32_t sensorStatsSize = 0;
    int32_t processStatsSize = 0;
    int32_t pidSize = 0;
    int32_t packageStatsSize = 0;

    if (DEBUG_SECURITY) {
        startTimeMillis = uptimeMillis();
    }

    if (mWifiRunningTimer != NULL) {
        active |= mWifiRunningTimer->isRunningLocked();
        active |= mWifiRunning;

        if (active) {
            return true;
        }
    }

    if (mFullWifiLockTimer != NULL) {
        active |= mFullWifiLockTimer->isRunningLocked();
        active |= mFullWifiLockOut;

        if (active) {
            return true;
        }
    }

    if (mScanWifiLockTimer != NULL) {
        active |= mScanWifiLockTimer->isRunningLocked();
        active |= mScanWifiLockOut;

        if (active) {
            return true;
        }
    }

    if (mWifiMulticastTimer != NULL) {
        active |= mWifiMulticastTimer->isRunningLocked();
        active |= mWifiMulticastEnabled;

        if (active) {
            return true;
        }
    }

    if (mAudioTurnedOnTimer != NULL) {
        active |= mAudioTurnedOnTimer->isRunningLocked();
        active |= mAudioTurnedOn;

        if (active) {
            return true;
        }
    }

    if (mVideoTurnedOnTimer != NULL) {
        active |= mVideoTurnedOnTimer->isRunningLocked();
        active |= mVideoTurnedOn;

        if (active) {
            return true;
        }
    }

    if (mGpuTurnedOnTimer != NULL) {
        active |= mGpuTurnedOnTimer->isRunningLocked();
        active |= mGpuTurnedOn;

        if (active) {
            return true;
        }
    }

    if (mDisplayTurnedOnTimer != NULL) {
        active |= mDisplayTurnedOnTimer->isRunningLocked();
        active |= mDisplayTurnedOn;

        if (active) {
            return true;
        }
    }

    for (int32_t i = 0; i < NUM_SCREEN_BRIGHTNESS_BINS; i++) {
        if (mDisplayBrightnessTimer[i] != NULL) {
            active |= mDisplayBrightnessTimer[i]->isRunningLocked();

            if (active) {
                return true;
            }
        }
    }

    if (DEBUG_SECURITY) {
        wakelockStatsSize = mWakelockStats.size();
        sensorStatsSize = mSensorStats.size();
        processStatsSize = mProcessStats.size();
        pidSize = mPids.size();
        packageStatsSize = mPackageStats.size();
        GLOGD("mWakelockStats size = %d", wakelockStatsSize);
        GLOGD("mSensorStats size = %d", sensorStatsSize);
        GLOGD("mProcessStats size = %d", processStatsSize);
        GLOGD("mPids size = %d", pidSize);
        GLOGD("mPackageStats size = %d", packageStatsSize);
    }

    if (mWakelockStats.size() > 0) {
        for (int32_t i = 0; i < static_cast<int32_t>(mWakelockStats.size()); i++) {
            sp<Uid::Wakelock> wl = safe_cast<BatteryStatsImpl::Uid::Wakelock*>(mWakelockStats.valueAt(i));
            if (wl->isRunning()) {
                return true;
            } else {
                // wl.detach();
            }
        }
    }

    if (mSensorStats.size() > 0) {
        for (int32_t i = 0; i < static_cast<int32_t>(mSensorStats.size()); i++) {
            sp<Uid::Sensor> s = safe_cast<BatteryStatsImpl::Uid::Sensor*>(mSensorStats.valueAt(i));
            if (s->isRunning()) {
                return true;
            } else {
                // s.detach();
            }
        }
    }

    if (mPids.size() > 0) {
        for (int32_t i = 0; !active && i < static_cast<int32_t>(mPids.size()); i++) {
            sp<Pid> pid = mPids.valueAt(i);
            if (pid->mWakeStart != 0) {
                return true;
            }
        }
    }

    mPids.clear();

    if (DEBUG_SECURITY) {
        tempTime = uptimeMillis() - startTimeMillis;
        if (tempTime >= BATTERY_MAX_RUN_TIME_30MS) {
            GLOGD("Uid isRunning check time = %lld,  mUid =  %d", tempTime, mUid);
            GLOGD("size = wakelockStatsSize = %d, sensorStatsSize = %d, processStatsSize = %d, pidSize = %d, packageStatsSize = %d",
                  wakelockStatsSize,
                  sensorStatsSize,
                  processStatsSize,
                  pidSize,
                  packageStatsSize);
        }
    }

    return active;
}
// Customize ---

// Customize +++
void BatteryStatsImpl::Uid::attach() {
    GLOGENTRY();

    int64_t startTimeMillis = 0;
    int64_t tempTime = 0;
    int32_t wakelockStatsSize = 0;
    int32_t sensorStatsSize = 0;
    int32_t processStatsSize = 0;
    int32_t pidSize = 0;
    int32_t packageStatsSize = 0;

    if (DEBUG_SECURITY) {
        startTimeMillis = uptimeMillis();
    }

    if (mWifiRunningTimer != NULL) {
        GLOGD("attach mWifiRunningTimer");
        mWifiRunningTimer->attach();
    }
    if (mFullWifiLockTimer != NULL) {
        GLOGD("attach mFullWifiLockTimer");
        mFullWifiLockTimer->attach();
    }
    if (mScanWifiLockTimer != NULL) {
        GLOGD("attach mScanWifiLockTimer");
        mScanWifiLockTimer->attach();
    }
    if (mWifiMulticastTimer != NULL) {
        GLOGD("attach mWifiMulticastTimer");
        mWifiMulticastTimer->attach();
    }
    if (mAudioTurnedOnTimer != NULL) {
        GLOGD("attach mAudioTurnedOnTimer");
        mAudioTurnedOnTimer->attach();
    }
    if (mVideoTurnedOnTimer != NULL) {
        GLOGD("attach mVideoTurnedOnTimer");
        mVideoTurnedOnTimer->attach();
    }
    if (mGpuTurnedOnTimer != NULL) {
        GLOGD("attach mGpuTurnedOnTimer");
        mGpuTurnedOnTimer->attach();
    }
    if (mDisplayTurnedOnTimer != NULL) {
        GLOGD("attach mDisplayTurnedOnTimer");
        mDisplayTurnedOnTimer->attach();
    }

    for (int32_t i = 0; i < NUM_SCREEN_BRIGHTNESS_BINS; i++) {
        if (mDisplayBrightnessTimer[i] != NULL) {
            GLOGD("attach mDisplayBrightnessTimer");
            mDisplayBrightnessTimer[i]->attach();
        }
    }

    if (mUserActivityCounters != NULL) {
        for (int32_t i = 0; i < NUM_USER_ACTIVITY_TYPES; i++) {
            GLOGD("attach mUserActivityCounters");
            mUserActivityCounters[i]->attach();
        }
    }

    if (DEBUG_SECURITY) {
        wakelockStatsSize = mWakelockStats.size();
        sensorStatsSize = mSensorStats.size();
        processStatsSize = mProcessStats.size();
        pidSize = mPids.size();
        packageStatsSize = mPackageStats.size();
        GLOGD("mWakelockStats size = %d", wakelockStatsSize);
        GLOGD("mSensorStats size = %d", sensorStatsSize);
        GLOGD("mProcessStats size = %d", processStatsSize);
        GLOGD("mPids size = %d", pidSize);
        GLOGD("mPackageStats size = %d", packageStatsSize);
    }

    if (mWakelockStats.size() > 0) {
        for (int32_t i = 0; i < static_cast<int32_t>(mWakelockStats.size()); i++) {
            sp<Uid::Wakelock> wl = safe_cast<BatteryStatsImpl::Uid::Wakelock*>(mWakelockStats.valueAt(i));
            GLOGD("attach Wakelock");
            wl->attach();
        }
    }

    if (mSensorStats.size() > 0) {
        for (int32_t i = 0; i < static_cast<int32_t>(mSensorStats.size()); i++) {
            sp<Uid::Sensor> s = safe_cast<BatteryStatsImpl::Uid::Sensor*>(mSensorStats.valueAt(i));
            GLOGD("attach Sensor");
            s->attach();
        }
    }

    if (mProcessStats.size() > 0) {
        for (int32_t i = 0; i < static_cast<int32_t>(mProcessStats.size()); i++) {
            sp<Uid::Proc> ps = safe_cast<BatteryStatsImpl::Uid::Proc*>(mProcessStats.valueAt(i));
            GLOGD("attach Sensor");
            ps->attach();
        }
    }

    if (mPackageStats.size() > 0) {
        for (int32_t i = 0; i <static_cast<int32_t>(mPackageStats.size()); i++) {
            sp<Uid::Pkg> p = safe_cast<BatteryStatsImpl::Uid::Pkg*>(mPackageStats.valueAt(i));
            GLOGD("attach Pkg");
            p->attach();

            if (DEBUG_SECURITY) {
                GLOGD("p.mServiceStats size = %d", p->mServiceStats.size());
            }

            if (p->mServiceStats.size() > 0) {
                for (int32_t i = 0; i < static_cast<int32_t>(p->mServiceStats.size()); i++) {
                    sp<Pkg::Serv> s = safe_cast<BatteryStatsImpl::Uid::Pkg::Serv*>(p->mServiceStats.valueAt(i));
                        GLOGD("attach Serv");
                    s->attach();
                }
            }
        }
    }

    if (DEBUG_SECURITY) {
        tempTime = uptimeMillis() - startTimeMillis;
        if (tempTime >= BATTERY_MAX_RUN_TIME_30MS) {
            GLOGD("Uid attach time = %lld, mUid = %d", tempTime, mUid);
            GLOGD("size = wakelockStatsSize = %d, sensorStatsSize = %d, processStatsSize = %d, pidSize = %d, packageStatsSize = %d",
                  wakelockStatsSize,
                  sensorStatsSize,
                  processStatsSize,
                  pidSize,
                  packageStatsSize);
        }
    }
}
// Customize ---

bool BatteryStatsImpl::Uid::reset() {
    GLOGENTRY();

    sp<BatteryStatsImpl> mbs = mBS.promote();
    NULL_RTN_VAL(mbs, false, "Promotion to sp<BatteryStatsImpl> fails");

    bool active = false;

    // Customize +++
    int64_t startTimeMillis = 0;
    int64_t tempTime = 0;
    int32_t wakelockStatsSize = 0;
    int32_t sensorStatsSize = 0;
    int32_t processStatsSize = 0;
    int32_t pidSize = 0;
    int32_t packageStatsSize = 0;

    if (1) {
        startTimeMillis = uptimeMillis();
    }
    // Customize ---

#ifdef WIFI_ENABLE
    if (mWifiRunningTimer != NULL) {
        active |= !mWifiRunningTimer->reset(mbs, false);
        active |= mWifiRunning;
    }

    if (mFullWifiLockTimer != NULL) {
        active |= !mFullWifiLockTimer->reset(mbs, false);
        active |= mFullWifiLockOut;
    }

    if (mScanWifiLockTimer != NULL) {
        active |= !mScanWifiLockTimer->reset(mbs, false);
        active |= mScanWifiLockOut;
    }

    if (mWifiMulticastTimer != NULL) {
        active |= !mWifiMulticastTimer->reset(mbs, false);
        active |= mWifiMulticastEnabled;
    }
#endif  // WIFI_ENABLE

    if (mAudioTurnedOnTimer != NULL) {
        active |= !mAudioTurnedOnTimer->reset(mbs, false);
        active |= mAudioTurnedOn;
    }

    if (mVideoTurnedOnTimer != NULL) {
        active |= !mVideoTurnedOnTimer->reset(mbs, false);
        active |= mVideoTurnedOn;
    }

    // Customize +++
    if (mGpuTurnedOnTimer != NULL) {
        active |= !mGpuTurnedOnTimer->reset(mbs, false);
        active |= mGpuTurnedOn;
    }

    if (mDisplayTurnedOnTimer != NULL) {
        active |= !mDisplayTurnedOnTimer->reset(mbs, false);
        active |= mDisplayTurnedOn;
    }

    for (int32_t i = 0; i < NUM_SCREEN_BRIGHTNESS_BINS; i++) {
        if (mDisplayBrightnessTimer[i] != NULL) {
            mDisplayBrightnessTimer[i]->reset(mbs, false);
        }
    }
    // Customize ---

    mLoadedTcpBytesReceived = mLoadedTcpBytesSent = 0;
    mCurrentTcpBytesReceived = mCurrentTcpBytesSent = 0;

    if (mUserActivityCounters != NULL) {
        for (int32_t i = 0; i < BatteryStats::Uid::NUM_USER_ACTIVITY_TYPES; i++) {
            (*mUserActivityCounters)[i]->reset(false);
        }
    }

    // Customize +++
    if (DEBUG_SECURITY) {
        wakelockStatsSize = mWakelockStats.size();
        sensorStatsSize = mSensorStats.size();
        processStatsSize = mProcessStats.size();
        pidSize = mPids.size();
        packageStatsSize = mPackageStats.size();
        GLOGD("mWakelockStats size = %d", wakelockStatsSize);
        GLOGD("mSensorStats size = %d", sensorStatsSize);
        GLOGD("mProcessStats size = %d", processStatsSize);
        GLOGD("mPids size = %d", pidSize);
        GLOGD("mPackageStats size = %d",  packageStatsSize);
    }
    // Customize ---

    if (mWakelockStats.size() > 0) {
        for (int32_t i = static_cast<int32_t>(mWakelockStats.size()) - 1; i >= 0; i--) {
            sp<Wakelock> wl = safe_cast<BatteryStatsImpl::Uid::Wakelock*>(mWakelockStats.valueAt(i));

            if (wl->reset()) {
                mWakelockStats.removeItem(mWakelockStats.keyAt(i));
            } else {
                active = true;
            }
        }
    }

    if (mSensorStats.size() > 0) {
        for (int32_t i = static_cast<int32_t>(mSensorStats.size()) - 1; i >= 0 ;i--) {
            sp<Sensor> s = safe_cast<BatteryStatsImpl::Uid::Sensor*>(mSensorStats.valueAt(i));

            if (s->reset()) {
                mSensorStats.removeItem(mSensorStats.keyAt(i));
            } else {
                active = true;
            }
        }
    }

    if (mProcessStats.size() > 0) {
        {
            GLOGAUTOMUTEX(_l, mbs->mPlugLock);

            sp<GArrayList<Unpluggable> > UnpluggablesTemp = new GArrayList<Unpluggable>();
            UnpluggablesTemp->clear();

            GLOGI("Uid::reset mUnpluggables [+] size=%d",mbs->mUnpluggables->size());
            for (int32_t i = 0; i < mbs->mUnpluggables->size(); ++i) {
                if (mbs->mUnpluggables->get(i)->mDelFlags != 1) {
                    UnpluggablesTemp->add(mbs->mUnpluggables->get(i));
                }
            }

            mbs->mUnpluggables->clear();

            for (int32_t i = 0; i < UnpluggablesTemp->size(); ++i) {
               mbs->mUnpluggables->add(UnpluggablesTemp->get(i));
            }

            UnpluggablesTemp->clear();
            GLOGI("Uid::reset mUnpluggables [-] size=%d",mbs->mUnpluggables->size());

            for (int32_t i = 0; i < static_cast<int32_t>(mProcessStats.size()); i++) {
                sp<Proc> p = safe_cast<BatteryStatsImpl::Uid::Proc*>(mProcessStats.valueAt(i));
                p->detach();
            }
        }

        mProcessStats.clear();
    }

    if (mPids.size() > 0) {
        for (int32_t i = 0; !active && i < static_cast<int32_t>(mPids.size()); i++) {
            sp<Pid> pid = mPids.valueAt(i);

            if (pid->mWakeStart != 0) {
                active = true;
            }
        }
    }

    if (mPackageStats.size() > 0) {
        for (int32_t i = 0; i < static_cast<int32_t>(mPackageStats.size()); i++) {
            sp<Pkg> p = safe_cast<BatteryStatsImpl::Uid::Pkg*>(mPackageStats.valueAt(i));
            p->detach();

            if (p->mServiceStats.size() > 0) {
                for (int32_t i = 0; i < static_cast<int32_t>(p->mServiceStats.size()); i++) {
                    sp<Pkg::Serv> s = safe_cast<BatteryStatsImpl::Uid::Pkg::Serv*>(p->mServiceStats.valueAt(i));
                    s->detach();
                }
            }
        }
        mPackageStats.clear();
    }

    mPids.clear();

    if (!active) {
#ifdef WIFI_ENABLE  // NOLINT
        if (mWifiRunningTimer != NULL) {
            mWifiRunningTimer->detach();
        }

        if (mFullWifiLockTimer != NULL) {
            mFullWifiLockTimer->detach();
        }

        if (mScanWifiLockTimer != NULL) {
            mScanWifiLockTimer->detach();
        }

        if (mWifiMulticastTimer != NULL) {
            mWifiMulticastTimer->detach();
        }
#endif  // WIFI_ENABLE

        if (mAudioTurnedOnTimer != NULL) {
            mAudioTurnedOnTimer->detach();
        }

        if (mVideoTurnedOnTimer != NULL) {
            mVideoTurnedOnTimer->detach();
        }

        // Customize +++
        if (mGpuTurnedOnTimer != NULL) {
            mGpuTurnedOnTimer->detach();
        }

        if (mDisplayTurnedOnTimer != NULL) {
            mDisplayTurnedOnTimer->detach();
        }

        if (mDisplayBrightnessTimer != NULL) {
            for (int32_t i = 0; i < NUM_SCREEN_BRIGHTNESS_BINS; i++) {
                if (mDisplayBrightnessTimer[i] != NULL) {
                    mDisplayBrightnessTimer[i]->detach();
                }
            }
        }
        // Customize ---

        if (mUserActivityCounters != NULL) {
            for (int32_t i = 0; i < BatteryStats::Uid::NUM_USER_ACTIVITY_TYPES; i++) {
                (*mUserActivityCounters)[i]->detach();
            }
        }
    }

    // Customize +++
    if (1) {
        tempTime = uptimeMillis() - startTimeMillis;
        if (tempTime >= BATTERY_MAX_RUN_TIME_100MS) {
            GLOGI("Uid reset time = %lld, mUid = %d", tempTime, mUid);
            GLOGI("size = wakelockStatsSize = %d, sensorStatsSize = %d, processStatsSize = %d, pidSize = %d, packageStatsSize = %d",
                  wakelockStatsSize,
                  sensorStatsSize,
                  processStatsSize,
                  pidSize,
                  packageStatsSize);
        }
    }
    // Customize ---

    return !active;
}

void BatteryStatsImpl::Uid::writeToParcelLocked(Parcel* out, int64_t batteryRealtime) {
    GLOGENTRY();

    // Customize +++
    if (DEBUG) {
        GLOGV("writeToParcelLocked Start uid = %d", mUid);
    }
    // Customize ---

    const int32_t NWS = mWakelockStats.size();

    out->writeInt32(NWS);
    for (int32_t i = 0; i < NWS; i++) {
        out->writeString(mWakelockStats.keyAt(i));
        sp<Wakelock> wakelock = safe_cast<BatteryStatsImpl::Uid::Wakelock*>(mWakelockStats.valueAt(i));

        wakelock->writeToParcelLocked(out, batteryRealtime);
    }

#ifdef SENSOR_ENABLE
    const int32_t NSS = mSensorStats.size();
    out->writeInt32(NSS);

    for (int32_t i = 0; i < NSS; i++) {
        out->writeInt32(mSensorStats.keyAt(i));
        sp<Sensor> sensor = safe_cast<BatteryStatsImpl::Uid::Sensor*>(mSensorStats.valueAt(i));

        sensor->writeToParcelLocked(out, batteryRealtime);
    }
#endif  // SENSOR_ENABLE

    const int32_t NPS = mProcessStats.size();
    out->writeInt32(NPS);

    for (int32_t i = 0; i < NPS; i++) {
        out->writeString(mProcessStats.keyAt(i));
        sp<Proc> proc = safe_cast<BatteryStatsImpl::Uid::Proc*>(mProcessStats.valueAt(i));

        proc->writeToParcelLocked(out);
    }

    const int32_t NPKS = mPackageStats.size();
    out->writeInt32(NPKS);

    for (int32_t i = 0; i < NPKS; i++) {
        out->writeString(mPackageStats.keyAt(i));
        sp<Pkg> pkg = safe_cast<BatteryStatsImpl::Uid::Pkg*>(mPackageStats.valueAt(i));

        pkg->writeToParcelLocked(out);
    }

    out->writeInt64(mLoadedTcpBytesReceived);
    out->writeInt64(mLoadedTcpBytesSent);
    out->writeInt64(computeCurrentTcpBytesReceived());
    out->writeInt64(computeCurrentTcpBytesSent());
    out->writeInt64(mTcpBytesReceivedAtLastUnplug);
    out->writeInt64(mTcpBytesSentAtLastUnplug);

#ifdef WIFI_ENABLE
    if (mWifiRunningTimer != NULL) {
        out->writeInt32(1);
        mWifiRunningTimer->writeToParcel(out, batteryRealtime);
    } else {
        out->writeInt32(0);
    }

    if (mFullWifiLockTimer != NULL) {
        out->writeInt32(1);
        mFullWifiLockTimer->writeToParcel(out, batteryRealtime);
    } else {
        out->writeInt32(0);
    }

    if (mScanWifiLockTimer != NULL) {
        out->writeInt32(1);
        mScanWifiLockTimer->writeToParcel(out, batteryRealtime);
    } else {
        out->writeInt32(0);
    }

    if (mWifiMulticastTimer != NULL) {
        out->writeInt32(1);
        mWifiMulticastTimer->writeToParcel(out, batteryRealtime);
    } else {
        out->writeInt32(0);
    }
#endif  // WIFI_ENABLE

    if (mAudioTurnedOnTimer != NULL) {
        out->writeInt32(1);
        mAudioTurnedOnTimer->writeToParcel(out, batteryRealtime);
    } else {
        out->writeInt32(0);
    }

    if (mVideoTurnedOnTimer != NULL) {
        out->writeInt32(1);
        mVideoTurnedOnTimer->writeToParcel(out, batteryRealtime);
    } else {
        out->writeInt32(0);
    }

    // Customize +++
    if (mGpuTurnedOnTimer != NULL) {
        out->writeInt32(1);
        mGpuTurnedOnTimer->writeToParcel(out, batteryRealtime);
    } else {
        out->writeInt32(0);
    }

    if (mDisplayTurnedOnTimer != NULL) {
        out->writeInt32(1);
        mDisplayTurnedOnTimer->writeToParcel(out, batteryRealtime);
    } else {
        out->writeInt32(0);
    }
    if (mDisplayBrightnessTimer != NULL) {
        out->writeInt32(1);
        for (int32_t i = 0; i < NUM_SCREEN_BRIGHTNESS_BINS; i++) {
            mDisplayBrightnessTimer[i]->writeToParcel(out, batteryRealtime);
        }
    } else {
        out->writeInt32(0);
    }
    // Customize ---

    if (mUserActivityCounters != NULL) {
        out->writeInt32(1);

        for (int32_t i = 0; i < NUM_USER_ACTIVITY_TYPES; i++) {
            (*mUserActivityCounters)[i]->writeToParcel(out);
        }
    } else {
        out->writeInt32(0);
    }

    // Customize +++
    if (DEBUG) {
        GLOGV("writeToParcelLocked End");
    }
    // Customize ---
}

void BatteryStatsImpl::Uid::readFromParcelLocked(sp<GArrayList<Unpluggable> >& unpluggables, const Parcel& in) {
    GLOGENTRY();

    // Customize +++
    if (DEBUG) {
        GLOGV("readFromParcelLocked Start uid = %d", mUid);
    }
    // Customize ---

    sp<BatteryStatsImpl> mbs = mBS.promote();
    NULL_RTN(mbs, "Promotion to sp<BatteryStatsImpl> fails");

    int32_t numWakelocks = in.readInt32();
    mWakelockStats.clear();

    for (int32_t j = 0; j < numWakelocks; j++) {
        sp<String> wakelockName = in.readString();
        sp<Uid::Wakelock> wakelock = new Wakelock(this, mBS);
        wakelock->readFromParcelLocked(unpluggables, in);
        // We will just drop some random set of wakelocks if
        // the previous run of the system was an older version
        // that didn't impose a limit.

        mWakelockStats.add(wakelockName, wakelock);
    }

#ifdef SENSOR_ENABLE
    int32_t numSensors = in.readInt32();
    mSensorStats.clear();

    for (int32_t k = 0; k < numSensors; k++) {
        int32_t sensorNumber = in.readInt32();
        sp<Uid::Sensor> sensor = new Sensor(this, mBS, sensorNumber);
        sensor->readFromParcelLocked(mbs->mUnpluggables, in);
        mSensorStats.add(sensorNumber, sensor);
    }
#endif  // SENSOR_ENABLE

    int32_t numProcs = in.readInt32();
    mProcessStats.clear();

    for (int32_t k = 0; k < numProcs; k++) {
        sp<String> processName = in.readString();
        sp<Uid::Proc> proc = new Proc(this, mBS, 0);
        proc->readFromParcelLocked(in);
        mProcessStats.add(processName, proc);
    }

    int32_t numPkgs = in.readInt32();
    mPackageStats.clear();

    for (int32_t l = 0; l < numPkgs; l++) {
        sp<String> packageName = in.readString();
        sp<Uid::Pkg> pkg = new Pkg(this, mBS);
        pkg->readFromParcelLocked(in);
        mPackageStats.add(packageName, pkg);
    }

    mLoadedTcpBytesReceived = in.readInt64();
    mLoadedTcpBytesSent = in.readInt64();
    mCurrentTcpBytesReceived = in.readInt64();
    mCurrentTcpBytesSent = in.readInt64();
    mTcpBytesReceivedAtLastUnplug = in.readInt64();
    mTcpBytesSentAtLastUnplug = in.readInt64();

#ifdef WIFI_ENABLE
    mWifiRunning = false;

    if (in.readInt32() != 0) {
        mWifiRunningTimer = new StopwatchTimer(mBS,
                                               wp<Uid>(this),
                                               BatteryStats::WIFI_RUNNING,
                                               mbs->mWifiRunningTimers,
                                               mbs->mUnpluggables,
                                               in);
    } else {
        mWifiRunningTimer = NULL;
    }

    mFullWifiLockOut = false;

    if (in.readInt32() != 0) {
        mFullWifiLockTimer = new StopwatchTimer(mBS,
                                                wp<Uid>(this),
                                                BatteryStats::FULL_WIFI_LOCK,
                                                mbs->mFullWifiLockTimers,
                                                mbs->mUnpluggables,
                                                in);
    } else {
        mFullWifiLockTimer = NULL;
    }

    mScanWifiLockOut = false;

    if (in.readInt32() != 0) {
        mScanWifiLockTimer = new StopwatchTimer(mBS,
                                                wp<Uid>(this),
                                                BatteryStats::SCAN_WIFI_LOCK,
                                                mbs->mScanWifiLockTimers,
                                                mbs->mUnpluggables,
                                                in);
    } else {
        mScanWifiLockTimer = NULL;
    }

    mWifiMulticastEnabled = false;

    if (in.readInt32() != 0) {
        mWifiMulticastTimer = new StopwatchTimer(mBS,
                                                 wp<Uid>(this),
                                                 BatteryStats::WIFI_MULTICAST_ENABLED,
                                                 mbs->mWifiMulticastTimers,
                                                 mbs->mUnpluggables,
                                                 in);
    } else {
        mWifiMulticastTimer = NULL;
    }
#endif  // WIFI_ENABLE

    mAudioTurnedOn = false;

    if (in.readInt32() != 0) {
        mAudioTurnedOnTimer = new StopwatchTimer(mBS,
                                                 wp<Uid>(this),
                                                 BatteryStats::AUDIO_TURNED_ON,
                                                 NULL,
                                                 mbs->mUnpluggables,
                                                 in);
    } else {
        mAudioTurnedOnTimer = NULL;
    }

    mVideoTurnedOn = false;

    if (in.readInt32() != 0) {
        mVideoTurnedOnTimer = new StopwatchTimer(mBS,
                                                 wp<Uid>(this),
                                                 BatteryStats::VIDEO_TURNED_ON,
                                                 NULL,
                                                 mbs->mUnpluggables,
                                                 in);
    } else {
        mVideoTurnedOnTimer = NULL;
    }

    mGpuTurnedOn = false;
    if (in.readInt32() != 0) {
        if (DEBUG) {
            GLOGV("Gpu have Timer");
        }

        mGpuTurnedOnTimer = new GpuTimer(mBS,
                                         wp<Uid>(this),
                                         GPU_TURNED_ON,
                                         NULL,
                                         mbs->mUnpluggables,
                                         in);
    } else {
        mGpuTurnedOnTimer = NULL;
    }

    mDisplayTurnedOn = false;
    if (in.readInt32() != 0) {
        mDisplayTurnedOnTimer = new StopwatchTimer(mBS,
                                                   wp<Uid>(this),
                                                   DISPLAY_TURNED_ON,
                                                   NULL,
                                                   mbs->mUnpluggables,
                                                   in);
    } else {
        mDisplayTurnedOnTimer = NULL;
    }

    if (in.readInt32() != 0) {
        for (int32_t i = 0; i < NUM_SCREEN_BRIGHTNESS_BINS; i++) {
            mDisplayBrightnessTimer[i] = new StopwatchTimer(mBS,
                                                            wp<Uid>(this),
                                                            1000+i,
                                                            NULL,
                                                            mbs->mUnpluggables,
                                                            in);
        }
    } else {
        for (int32_t i = 0; i < NUM_SCREEN_BRIGHTNESS_BINS; i++) {
            mDisplayBrightnessTimer[i] = new StopwatchTimer(mBS,
                                                            wp<Uid>(this),
                                                            1000+i,
                                                            NULL,
                                                            mbs->mUnpluggables);
        }
    }

    if (in.readInt32() != 0) {
        mUserActivityCounters = new Blob<sp<Counter> >(BatteryStats::Uid::NUM_USER_ACTIVITY_TYPES);

        for (int32_t i = 0; i < NUM_USER_ACTIVITY_TYPES; i++) {
            (*mUserActivityCounters)[i] = new Counter(mBS, mbs->mUnpluggables, in, 0);
        }
    } else {
        mUserActivityCounters = NULL;
    }

    if (DEBUG) {
        GLOGV("readFromParcelLocked End");
    }
}

sp<BatteryStats::Uid::Proc> BatteryStatsImpl::Uid::getProcessStatsLocked(const sp<String>& name) {
    GLOGENTRY();


    sp<BatteryStats::Uid::Proc> ps;
    int32_t index = mProcessStats.indexOfKey(name);

    if (index >= 0) {
        ps = mProcessStats.valueAt(index);
    }

    if (ps == NULL) {
        ps = new Proc(this, mBS, 0);
        mProcessStats.add(name, ps);
    }

    return ps;
}

KeyedVector<int32_t, sp<BatteryStats::Uid::Pid> >& BatteryStatsImpl::Uid::getPidStats() {
    GLOGENTRY();

    return mPids;
}

sp<BatteryStats::Uid::Pid> BatteryStatsImpl::Uid::getPidStatsLocked(int32_t pid) {
    GLOGENTRY();

    sp<BatteryStats::Uid::Pid> p;
    int32_t index = mPids.indexOfKey(pid);

    if (index >= 0) {
        p = mPids.valueAt(index);
    }

    if (p == NULL) {
        p = new BatteryStats::Uid::Pid();
        mPids.add(pid, p);
    }

    return p;
}

sp<BatteryStatsImpl::Uid::Pkg> BatteryStatsImpl::Uid::getPackageStatsLocked(const sp<String>& name) {
    GLOGENTRY();

    sp<BatteryStatsImpl::Uid::Pkg> ps;
    int32_t index = mPackageStats.indexOfKey(name);

    if (index >= 0) {
        ps = safe_cast<BatteryStatsImpl::Uid::Pkg*>(mPackageStats.valueAt(index));
    }

    if (ps == NULL) {
        ps = new Pkg(this, mBS);
        sp<BatteryStats::Uid::Pkg> ps1 = ps;  // upper cast, ok
        mPackageStats.add(name, ps1);
    }

    return ps;
}


sp<BatteryStatsImpl::Uid::Pkg::Serv> BatteryStatsImpl::Uid::getServiceStatsLocked(const sp<String>& pkg, const sp<String>& serv) {
    GLOGENTRY();

    sp<BatteryStatsImpl::Uid::Pkg> ps = getPackageStatsLocked(pkg);
    sp<BatteryStatsImpl::Uid::Pkg::Serv> ss;
    int32_t index = ps->mServiceStats.indexOfKey(serv);

    if (index >= 0) {
        ss = safe_cast<BatteryStatsImpl::Uid::Pkg::Serv*>(ps->mServiceStats.valueAt(index));
    }

    if (ss == NULL) {
        ss = ps->newServiceStatsLocked();
        sp<BatteryStats::Uid::Pkg::Serv> ss1 = ss;  // upper cast, ok
        ps->mServiceStats.add(serv, ss1);
    }

    return ss;
}

sp<BatteryStatsImpl::StopwatchTimer> BatteryStatsImpl::Uid::getWakeTimerLocked(const sp<String>& name, int32_t type) {
    GLOGENTRY();

    sp<BatteryStatsImpl> mbs = mBS.promote();
    NULL_RTN_VAL(mbs, NULL, "Promotion to sp<BatteryStatsImpl> fails");

    sp<Wakelock> wl = NULL;
    int32_t index = mWakelockStats.indexOfKey(name);

    if (index >= 0) {
        wl = safe_cast<BatteryStatsImpl::Uid::Wakelock*>(mWakelockStats.valueAt(index));
    }

    if (wl == NULL) {
        const int32_t N = mWakelockStats.size();

        if (N > MAX_WAKELOCKS_PER_UID && (mUid != Process::SYSTEM_UID
                                          || N > MAX_WAKELOCKS_PER_UID_IN_SYSTEM)) {
            const_cast<sp<String>& >(name) = BATCHED_WAKELOCK_NAME();

            index = mWakelockStats.indexOfKey(name);
            if (index >= 0) {
                wl = safe_cast<BatteryStatsImpl::Uid::Wakelock*>(mWakelockStats.valueAt(index));
            }
        }

        if (wl == NULL) {
            wl = new Wakelock(this, mBS);
            mWakelockStats.add(name, wl);
        }
    }

    sp<StopwatchTimer> t = NULL;

    switch (type) {
        case WAKE_TYPE_PARTIAL:
            t = wl->mTimerPartial;

            if (t == NULL) {
                t = new StopwatchTimer(mBS,
                                       wp<Uid>(this),
                                       WAKE_TYPE_PARTIAL,
                                       mbs->mPartialTimers,
                                       mbs->mUnpluggables);
                wl->mTimerPartial = t;
            }

            return t;
        case WAKE_TYPE_FULL:
            t = wl->mTimerFull;

            if (t == NULL) {
                t = new StopwatchTimer(mBS,
                                       wp<Uid>(this),
                                       WAKE_TYPE_FULL,
                                       mbs->mFullTimers,
                                       mbs->mUnpluggables);
                wl->mTimerFull = t;
            }

            return t;
        case WAKE_TYPE_WINDOW:
            t = wl->mTimerWindow;

            if (t == NULL) {
                t = new StopwatchTimer(mbs,
                                       wp<Uid>(this),
                                       WAKE_TYPE_WINDOW,
                                       mbs->mWindowTimers,
                                       mbs->mUnpluggables);
                wl->mTimerWindow = t;
            }

            return t;
        default:
            // throw new IllegalArgumentExcep
            GLOGE(LOG_TAG "IllegalArgumentException: type=%d", type);
            return t;
    }
}

#ifdef SENSOR_ENABLE
sp<BatteryStatsImpl::StopwatchTimer> BatteryStatsImpl::Uid::getSensorTimerLocked(int32_t sensor, bool create) {
    GLOGENTRY();

    sp<BatteryStatsImpl> mbs = mBS.promote();
    NULL_RTN_VAL(mbs, NULL, "Promotion to sp<BatteryStatsImpl> fails");

    sp<Sensor> se;

    int32_t index = mSensorStats.indexOfKey(sensor);
    if (index >= 0) {
        se = safe_cast<BatteryStatsImpl::Uid::Sensor*>(mSensorStats.valueAt(index));
    }

    if (se == NULL) {
        if (!create) {
            return 0;
        }

        se = new Sensor(this, mBS, sensor);
        mSensorStats.add(sensor, se);
    }

    sp<StopwatchTimer> t = se->mTimer;

    if (t != NULL) {
        return t;
    }

    sp<GArrayList<StopwatchTimer> >timers;
    index = mbs->mSensorTimers.indexOfKey(sensor);

    if (index >= 0) {
        timers = mbs->mSensorTimers.valueAt(index);
    }

    if (timers == NULL) {
        sp<GArrayList<StopwatchTimer> > timers = new GArrayList<StopwatchTimer>();
        mbs->mSensorTimers.add(sensor, timers);
    }

    t = new StopwatchTimer(mbs, wp<Uid>(this), BatteryStats::SENSOR, timers, mbs->mUnpluggables);
    se->mTimer = t;
    return t;
}
#endif  // SENSOR_ENABLE

void BatteryStatsImpl::Uid::noteStartWakeLocked(int32_t pid, const sp<String>& name, int32_t type) {
    GLOGENTRY();

    sp<BatteryStatsImpl> mbs = mBS.promote();
    NULL_RTN(mbs, "Promotion to sp<BatteryStatsImpl> fails");

    sp<StopwatchTimer> t = getWakeTimerLocked(name, type);

    if (t != NULL) {
        t->startRunningLocked(mbs);

        // Customize +++
        if (1) {  // if (ProfileConfig.getProfileDebugWakelock()) {
            // synchronized (mWakelockHistory) {
            {
                GLOGAUTOMUTEX(_l, mbs->mWakelockHistoryLock);
                sp<BatteryStatsImpl::WakelockHistory> wh = mbs->mWakelockHistory->get(name);

                if (wh == NULL) {
                    wh = new WakelockHistory();
                    wh->mName = name;
                    mbs->mWakelockHistory->put(name, wh);
                }

                if (!wh->mLockTypes->contains(new Integer(type))) {
                    wh->mLockTypes->add(new Integer(type));
                }

                // Ensure the last 50 records are kept.

                if (wh->mReleaseTimeList.size() > 100) {
                    for (int32_t i = 0 ; i < 50 ; ++i) {
                        int64_t diff = wh->mReleaseTimeList.itemAt(0) - wh->mAcquireTimeList.itemAt(0);
                        if (diff < 0) {
                            diff = 0;
                        }
                        wh->mSum += diff;
                        wh->mReleaseTimeList.removeAt(0);
                        wh->mAcquireTimeList.removeAt(0);
                    }
                }
                wh->mAcquireTimeList.add(System::currentTimeMillis());
            }
        }
        // Customize ---

        // Customize +++
        if (1) {  // if (ProfileConfig.getProfilePower())
            GLOGI(LOG_TAG "hold %s  wakelock, name = %s,  pid = %d", getWakeLockTypeString(type)->string(), name->string(), pid);
        }

        #if 0
        if (com.htc.utils.PerformanceLogUtil.Enabled) {
            com.htc.utils.PerformanceLogUtil.logNoteStartWakeLock(name, getWakeLockTypeString(type));
        }
        #endif  // TODO
        // Customize ---
    }

    if (pid >= 0 && type == BatteryStats::WAKE_TYPE_PARTIAL) {
        sp<Pid> p = getPidStatsLocked(pid);

        if (p->mWakeStart == 0) {
            p->mWakeStart = elapsedRealtime();
        }
    }
}

void BatteryStatsImpl::Uid::noteStopWakeLocked(int32_t pid, const sp<String>& name, int32_t type) {
    GLOGENTRY();

    sp<BatteryStatsImpl> mbs = mBS.promote();
    NULL_RTN(mbs, "Promotion to sp<BatteryStatsImpl> fails");

    sp<StopwatchTimer> t = getWakeTimerLocked(name, type);

    if (t != NULL) {
        t->stopRunningLocked(mbs);

        // Customize +++
        if (1) {  // if (ProfileConfig.getProfileDebugWakelock()) {
            // synchronized (mWakelockHistory) {
            {
                GLOGAUTOMUTEX(_l, mbs->mWakelockHistoryLock);
                sp<WakelockHistory> wh = mbs->mWakelockHistory->get(name);

                if (wh != NULL) {
                    if (wh->mReleaseTimeList.size() < wh->mAcquireTimeList.size())
                        wh->mReleaseTimeList.add(System::currentTimeMillis());
                }
            }
        }
        // Customize ---

        // Customize +++
        if (1) {  // if (ProfileConfig.getProfilePower())
            GLOGI(LOG_TAG "release %s  wakelock, name = %s,  pid = %d", getWakeLockTypeString(type)->string(), name->string(), pid);
        }

        #if 0
        if (com.htc.utils.PerformanceLogUtil.Enabled) {
            com.htc.utils.PerformanceLogUtil.logNoteStopWakeLock(name, getWakeLockTypeString(type));
        }
        #endif  // TODO
        // Customize ---
    }

    if (pid >= 0 && type == WAKE_TYPE_PARTIAL) {
        sp<Pid> p;
        int32_t index = mPids.indexOfKey(pid);
        if (index >= 0) {
            p = mPids.valueAt(index);
        }

        if (p != NULL && p->mWakeStart != 0) {
            p->mWakeSum += elapsedRealtime() - p->mWakeStart;
            p->mWakeStart = 0;
        }
    }
}

// Customize +++
sp<String> BatteryStatsImpl::Uid::getWakeLockTypeString(int32_t type) {
    GLOGENTRY();

    switch (type) {
    case BatteryStats::WAKE_TYPE_PARTIAL:
        return new String("partial");
    case BatteryStats::WAKE_TYPE_FULL:
        return new String("full");
    case BatteryStats::WAKE_TYPE_WINDOW:
        return new String("window");
    default:
        return new String("unknown");
    }
}
// Customize ---

void BatteryStatsImpl::Uid::reportExcessiveWakeLocked(const sp<String>& proc, int64_t overTime, int64_t usedTime) {
    GLOGENTRY();

    sp<Proc> p = safe_cast<BatteryStatsImpl::Uid::Proc*>(getProcessStatsLocked(proc).get());

    if (p != NULL) {
        p->addExcessiveWake(overTime, usedTime);
    }
}

void BatteryStatsImpl::Uid::reportExcessiveCpuLocked(const sp<String>& proc, int64_t overTime, int64_t usedTime) {
    GLOGENTRY();

    sp<Proc> p = safe_cast<BatteryStatsImpl::Uid::Proc*>(getProcessStatsLocked(proc).get());

    if (p != NULL) {
        p->addExcessiveCpu(overTime, usedTime);
    }
}

#ifdef SENSOR_ENABLE
void BatteryStatsImpl::Uid::noteStartSensor(int32_t sensor) {
    GLOGENTRY();

    sp<BatteryStatsImpl> mbs = mBS.promote();
    NULL_RTN(mbs, "Promotion to sp<BatteryStatsImpl> fails");

    sp<StopwatchTimer> t = getSensorTimerLocked(sensor, true);

    if (t != NULL) {
        t->startRunningLocked(mbs);
    }
}

void BatteryStatsImpl::Uid::noteStopSensor(int32_t sensor) {
    GLOGENTRY();

    sp<BatteryStatsImpl> mbs = mBS.promote();
    NULL_RTN(mbs, "Promotion to sp<BatteryStatsImpl> fails");

    sp<StopwatchTimer> t = getSensorTimerLocked(sensor, true);

    if (t != NULL) {
        t->startRunningLocked(mbs);
    }
}
#endif  // SENSOR_ENABLE

#ifdef GPS_ENABLE
void BatteryStatsImpl::Uid::noteStartGps() {
    GLOGENTRY();

    sp<BatteryStatsImpl> mbs = mBS.promote();
    NULL_RTN(mbs, "Promotion to sp<BatteryStatsImpl> fails");

#ifdef SENSOR_ENABLE
    sp<StopwatchTimer> t = getSensorTimerLocked(Sensor::GPS, true);

    if (t != NULL) {
        t->startRunningLocked(mbs);
    }
#endif  // SENSOR_ENABLE
}

void BatteryStatsImpl::Uid::noteStopGps() {
    GLOGENTRY();

    sp<BatteryStatsImpl> mbs = mBS.promote();
    NULL_RTN(mbs, "Promotion to sp<BatteryStatsImpl> fails");

#ifdef SENSOR_ENABLE
    sp<StopwatchTimer> t = getSensorTimerLocked(Sensor::GPS, false);

    if (t != NULL) {
        t->stopRunningLocked(mbs);
    }
#endif  // SENSOR_ENABLE
}
#endif  // GPS_ENABLE

sp<BatteryStatsImpl> BatteryStatsImpl::Uid::getBatteryStats() {
    GLOGENTRY();

    sp<BatteryStatsImpl> mbs = mBS.promote();
    NULL_RTN_VAL(mbs, NULL, "Promotion to sp<BatteryStatsImpl> fails");

    return mbs;
}

sp<BatteryStatsImpl::StopwatchTimer> BatteryStatsImpl::Uid::Wakelock::readTimerFromParcel(int32_t type, sp<GArrayList<StopwatchTimer> >& pool, sp<GArrayList<Unpluggable> >& unpluggables, const Parcel& in) {
    GLOGENTRY();

    if (in.readInt32() == 0) {
        return NULL;
    }

    return new StopwatchTimer(mPBS, mBS, type, pool, unpluggables, in);
}

// Customize +++
bool BatteryStatsImpl::Uid::Wakelock::isRunning() {
    GLOGENTRY();

    bool wlactive = false;

    if (mTimerFull != NULL) {
        wlactive |= mTimerFull->isRunningLocked();
    }

    if (mTimerPartial != NULL) {
        wlactive |= mTimerPartial->isRunningLocked();
    }

    if (mTimerWindow != NULL) {
        wlactive |= mTimerWindow->isRunningLocked();
    }

    return wlactive;
}
// Customize ---

// Customize +++
void BatteryStatsImpl::Uid::Wakelock::attach() {
    GLOGENTRY();

    if (DEBUG_SECURITY) {
        GLOGD("Wakelock attach");
    }
    if (mTimerFull != NULL) {
        GLOGD("mTimerFull attach");
        mTimerFull->attach();
    }

    if (mTimerPartial != NULL) {
        GLOGD("mTimerPartial attach");
        mTimerPartial->attach();
    }

    if (mTimerWindow != NULL) {
        GLOGD("mTimerWindow attach");
        mTimerWindow->attach();
    }
}
// Customize ---

// Customize +++
void BatteryStatsImpl::Uid::Wakelock::detach() {
    GLOGENTRY();

    if (mTimerFull != NULL) {
        mTimerFull->detach();
    }

    if (mTimerPartial != NULL) {
        mTimerPartial->detach();
    }

    if (mTimerWindow != NULL) {
        mTimerWindow->detach();
    }
}
// Customize ---

bool BatteryStatsImpl::Uid::Wakelock::reset() {
    GLOGENTRY();

    sp<BatteryStatsImpl> mpbs = mPBS.promote();
    NULL_RTN_VAL(mpbs, false, "Promotion to sp<BatteryStatsImpl> fails");

    bool wlactive = false;

    if (mTimerFull != NULL) {
        wlactive |= !mTimerFull->reset(mpbs, false);
    }

    if (mTimerPartial != NULL) {
        wlactive |= !mTimerPartial->reset(mpbs, false);
    }

    if (mTimerWindow != NULL) {
        wlactive |= !mTimerWindow->reset(mpbs, false);
    }

    if (!wlactive) {
        if (mTimerFull != NULL) {
            mTimerFull->detach();
            mTimerFull = NULL;
        }

        if (mTimerPartial != NULL) {
            mTimerPartial->detach();
            mTimerPartial = NULL;
        }

        if (mTimerWindow != NULL) {
            mTimerWindow->detach();
            mTimerWindow = NULL;
        }
    }

    return !wlactive;
}

void BatteryStatsImpl::Uid::Wakelock::readFromParcelLocked(sp<GArrayList<Unpluggable> >& unpluggables, const Parcel& in) {
    GLOGENTRY();

    sp<BatteryStatsImpl> mpbs = mPBS.promote();
    NULL_RTN(mpbs, "Promotion to sp<BatteryStatsImpl> fails");

    mTimerPartial = readTimerFromParcel(BatteryStats::WAKE_TYPE_PARTIAL, mpbs->mPartialTimers, unpluggables, in);
    mTimerFull = readTimerFromParcel(BatteryStats::WAKE_TYPE_FULL, mpbs->mFullTimers, unpluggables, in);
    mTimerWindow = readTimerFromParcel(BatteryStats::WAKE_TYPE_WINDOW, mpbs->mWindowTimers, unpluggables, in);
}

void BatteryStatsImpl::Uid::Wakelock::writeToParcelLocked(Parcel* out, int64_t batteryRealtime) {
    GLOGENTRY();

    Timer::writeTimerToParcel(out, mTimerPartial, batteryRealtime);
    Timer::writeTimerToParcel(out, mTimerFull, batteryRealtime);
    Timer::writeTimerToParcel(out, mTimerWindow, batteryRealtime);
}

sp<BatteryStats::Timer> BatteryStatsImpl::Uid::Wakelock::getWakeTime(int32_t type) {
    GLOGENTRY();

    switch (type) {
    case WAKE_TYPE_FULL:
        return mTimerFull;
    case WAKE_TYPE_PARTIAL:
        return mTimerPartial;
    case WAKE_TYPE_WINDOW:
        return mTimerWindow;
    default:
        sp<BatteryStats::Timer> temp;
        GLOG("Exception happens in getWakeTime!");
        return temp;
        // throw new IllegalArgumentException("type = " + type);
    }
}

#ifdef SENSOR_ENABLE
BatteryStatsImpl::Uid::Sensor::Sensor(const wp<BatteryStatsImpl::Uid>& parent, const wp<BatteryStatsImpl>& pparent, int32_t handle)
    : PREINIT_DYNAMIC(),
    mCtorSafe(new ctor_safe(this)),
    mHandle(handle),
    mTimer(NULL),
    mBS(parent),
    mPBS(pparent) {
    GLOGENTRY();
    delete mCtorSafe;
}

// Customize +++
bool BatteryStatsImpl::Uid::Sensor::isRunning() {
    GLOGENTRY();

    bool wlactive = false;

    if (mTimer != NULL) {
        wlactive |= mTimer->isRunningLocked();
    }

    return wlactive;
}
// Customize ---

// Customize +++
void BatteryStatsImpl::Uid::Sensor::attach() {
    GLOGENTRY();

    if (DEBUG_SECURITY) {
        GLOGD("Sensor attach");
    }

    if (mTimer != NULL) {
        GLOGD("mTimer attach");
        mTimer->attach();
    }
}
// Customize ---

// Customize +++
void BatteryStatsImpl::Uid::Sensor::detach() {
    GLOGENTRY();

    if (mTimer != NULL) {
        mTimer->detach();
    }
}
// Customize ---

sp<BatteryStatsImpl::StopwatchTimer> BatteryStatsImpl::Uid::Sensor::readTimerFromParcel(sp<GArrayList<Unpluggable> >& unpluggables, const Parcel& in) {
    GLOGENTRY();

    sp<BatteryStatsImpl> mpbs = mPBS.promote();
    NULL_RTN_VAL(mpbs, NULL, "Promotion to sp<BatteryStatsImpl> fails");

    if (in.readInt32() == 0) {
        return 0;
    }

    sp<GArrayList<StopwatchTimer> >pool;

    int32_t index = mpbs->mSensorTimers.indexOfKey(mHandle);

    if (index >= 0) {
        pool = mpbs->mSensorTimers.valueAt(index);
    }

    if (pool == NULL) {
        sp<GArrayList<StopwatchTimer> > pool = new GArrayList<StopwatchTimer>();
        mpbs->mSensorTimers.add(mHandle, pool);
    }

    return new StopwatchTimer(mPBS, mBS, 0, pool, unpluggables, in);
}

bool BatteryStatsImpl::Uid::Sensor::reset() {
    GLOGENTRY();

    sp<BatteryStatsImpl> mpbs = mPBS.promote();
    NULL_RTN_VAL(mpbs, NULL, "Promotion to sp<BatteryStatsImpl> fails");

    if (mTimer->reset(mpbs, true)) {
        mTimer = NULL;
        return true;
    }

    return false;
}

void BatteryStatsImpl::Uid::Sensor::readFromParcelLocked(sp<GArrayList<Unpluggable> >& unpluggables, const Parcel& in) {
    GLOGENTRY();

    mTimer = readTimerFromParcel(unpluggables, in);
}

void BatteryStatsImpl::Uid::Sensor::writeToParcelLocked(Parcel* out, int64_t batteryRealtime) {  // NOLINT
    GLOGENTRY();

    Timer::writeTimerToParcel(out, mTimer, batteryRealtime);
}

sp<BatteryStats::Timer> BatteryStatsImpl::Uid::Sensor::getSensorTime() {
    GLOGENTRY();

    return mTimer;
}

int32_t BatteryStatsImpl::Uid::Sensor::getHandle() {
    GLOGENTRY();

    return mHandle;
}
#endif  // SENSOR_ENABLE

BatteryStatsImpl::Uid::Proc::Proc(const wp<BatteryStatsImpl::Uid>& parent, const wp<BatteryStatsImpl>& pparent, int32_t value)
    : Unpluggable(value),
    PREINIT_DYNAMIC(),
    mCtorSafe(new ctor_safe(this)),
    mBS(parent),
    mPBS(pparent) {
    GLOGENTRY();

    sp<BatteryStatsImpl> mpbs = mPBS.promote();
    NULL_RTN(mpbs, "Promotion to sp<BatteryStatsImpl> fails");

    mUserTime = 0;
    mSystemTime = 0;
    mStarts = 0;
    mForegroundTime = 0;
    mLoadedUserTime = 0;
    mLoadedSystemTime = 0;
    mLoadedStarts = 0;
    mLoadedForegroundTime = 0;
    mLastUserTime = 0;
    mLastSystemTime = 0;
    mLastStarts = 0;
    mLastForegroundTime = 0;
    mUnpluggedUserTime = 0;
    mUnpluggedSystemTime = 0;
    mUnpluggedStarts = 0;
    mUnpluggedForegroundTime = 0;

    // Customize +++
    // synchronized (sLockPlug)
    // Customize ---
    {
        GLOGAUTOMUTEX(_l, mpbs->mPlugLock);
        mpbs->mUnpluggables->add(this);
    }
    mSpeedBins = new Blob<sp<SamplingCounter> >(mpbs->getCpuSpeedSteps());
    delete mCtorSafe;
}

void BatteryStatsImpl::Uid::Proc::unplug(int64_t batteryUptime, int64_t batteryRealtime) {
    GLOGENTRY();

    mUnpluggedUserTime = mUserTime;
    mUnpluggedSystemTime = mSystemTime;
    mUnpluggedStarts = mStarts;
    mUnpluggedForegroundTime = mForegroundTime;
}

void BatteryStatsImpl::Uid::Proc::plug(int64_t batteryUptime, int64_t batteryRealtime) {
    GLOGENTRY();
}

// Customize +++
void BatteryStatsImpl::Uid::Proc::attach() {
    GLOGENTRY();

    sp<BatteryStatsImpl> mpbs = mPBS.promote();
    NULL_RTN(mpbs, "Promotion to sp<BatteryStatsImpl> fails");

    if (DEBUG_SECURITY) {
        GLOGD("Proc attach");
    }

    // synchronized (sLockPlug)
    {
        GLOGAUTOMUTEX(_l, mpbs->mPlugLock);

        mpbs->mUnpluggables->add(this);

        for (uint32_t i = 0; i < mSpeedBins->length(); i++) {
            sp<SamplingCounter> c = mSpeedBins[i];
            if (c != NULL) {
                GLOGD("SamplingCounter attach");
                mpbs->mUnpluggables->add(c);
            }
        }
    }
}
// Customize ---

void BatteryStatsImpl::Uid::Proc::detach() {
    GLOGENTRY();

    sp<BatteryStatsImpl> mpbs = mPBS.promote();
    NULL_RTN(mpbs, "Promotion to sp<BatteryStatsImpl> fails");

    #if 1
    long long start = elapsedRealtime();

    mpbs->mUnpluggables->remove(this);

    for (uint32_t i = 0; i < mSpeedBins->length(); i++) {
        (*mSpeedBins)[i] = NULL;
    }

    long long end = elapsedRealtime();
    GLOGI("Uid::Proc::detach time start=%lld end=%lld duration=%lld", start, end, (end - start));
    #else
    // Customize +++
    // synchronized (sLockPlug)
    // Customize ---
    {
        
        long long start = elapsedRealtime();
        GLOGAUTOMUTEX(_l, mpbs->mPlugLock);
        long long end = elapsedRealtime();
        GLOGI("Uid::Proc::detach 1-1 time start=%lld end=%lld duration=%lld",start, end, (end - start));

        long long start1 = elapsedRealtime();
        mpbs->mUnpluggables->remove(this);
        GLOGI("Uid::Proc::detach mSpeedBins->length=%d",mSpeedBins->length());
        GLOGI("Uid::Proc::detach mpbs->mUnpluggables->size=%d", mpbs->mUnpluggables->size());

        for (uint32_t i = 0; i < mSpeedBins->length(); i++) {
            sp<SamplingCounter> c = (*mSpeedBins)[i];
            if (c != NULL) {
                mpbs->mUnpluggables->remove(c);
                (*mSpeedBins)[i] = NULL;
            }
        }
        GLOGI("Uid::Proc::detach mpbs->mUnpluggables->size=%d", mpbs->mUnpluggables->size());
        long long end1 = elapsedRealtime();
        GLOGI("Uid::Proc::detach 2-2 time start=%lld end=%lld duration=%lld",start1, end1, (end1 - start1));
    }
    #endif
}

int32_t BatteryStatsImpl::Uid::Proc::countExcessivePowers() {
    GLOGENTRY();

    return (mExcessivePower != NULL)? mExcessivePower->size() : 0;
}

sp<BatteryStats::Uid::Proc::ExcessivePower> BatteryStatsImpl::Uid::Proc::getExcessivePower(int32_t i) {
    GLOGENTRY();

    if (mExcessivePower != NULL) {
        return mExcessivePower->get(i);
    }

    return NULL;
}

void BatteryStatsImpl::Uid::Proc::addExcessiveWake(int64_t overTime, int64_t usedTime) {
    GLOGENTRY();

    if (mExcessivePower == NULL) {
        sp<GArrayList<ExcessivePower> > mExcessivePower = new GArrayList<ExcessivePower>();
    }

    sp<ExcessivePower> ew = new ExcessivePower();
    ew->type = ExcessivePower::TYPE_WAKE;
    ew->overTime = overTime;
    ew->usedTime = usedTime;
    mExcessivePower->add(ew);
}

void BatteryStatsImpl::Uid::Proc::addExcessiveCpu(int64_t overTime, int64_t usedTime) {
    GLOGENTRY();

    if (mExcessivePower == NULL) {
        sp<GArrayList<ExcessivePower> > mExcessivePower = new GArrayList<ExcessivePower>();
    }

    sp<ExcessivePower> ew = new ExcessivePower();
    ew->type = ExcessivePower::TYPE_CPU;
    ew->overTime = overTime;
    ew->usedTime = usedTime;
    mExcessivePower->add(ew);
}

void BatteryStatsImpl::Uid::Proc::writeExcessivePowerToParcelLocked(Parcel* out) {  // NOLINT
    GLOGENTRY();

    if (mExcessivePower == NULL) {
        out->writeInt32(0);
        return;
    }

    const int32_t N = mExcessivePower->size();
    out->writeInt32(N);

    for (int32_t i = 0; i < N; i++) {
        sp<ExcessivePower> ew = mExcessivePower->get(i);
        out->writeInt32(ew->type);
        out->writeInt64(ew->overTime);
        out->writeInt64(ew->usedTime);
    }
}

bool BatteryStatsImpl::Uid::Proc::readExcessivePowerFromParcelLocked(const Parcel& in) {  // NOLINT
    GLOGENTRY();

    const int32_t N = in.readInt32();

    if (N == 0) {
        mExcessivePower = NULL;
        return true;
    }

    if (N > 10000) {
        GLOGW("%s File corrupt: too many excessive power entries %d", LOG_TAG, N);
        return false;
    }

    sp<GArrayList<ExcessivePower> > mExcessivePower;
    for (int32_t i = 0; i < N; i++) {
        sp<ExcessivePower> ew = new ExcessivePower();
        ew->type = in.readInt32();
        ew->overTime = in.readInt64();
        ew->usedTime = in.readInt64();
        mExcessivePower->add(ew);
    }

    return true;
}

void BatteryStatsImpl::Uid::Proc::writeToParcelLocked(Parcel* out) {  // NOLINT
    GLOGENTRY();


    out->writeInt64(mUserTime);
    out->writeInt64(mSystemTime);
    out->writeInt64(mForegroundTime);
    out->writeInt32(mStarts);
    out->writeInt64(mLoadedUserTime);
    out->writeInt64(mLoadedSystemTime);
    out->writeInt64(mLoadedForegroundTime);
    out->writeInt32(mLoadedStarts);
    out->writeInt64(mUnpluggedUserTime);
    out->writeInt64(mUnpluggedSystemTime);
    out->writeInt64(mUnpluggedForegroundTime);
    out->writeInt32(mUnpluggedStarts);
    out->writeInt32(mSpeedBins->length());

    for (uint32_t i = 0; i < mSpeedBins->length(); i++) {
        sp<SamplingCounter> c = (*mSpeedBins)[i];

        if (c != NULL) {
            out->writeInt32(1);
            c->writeToParcel(out);
        } else {
            out->writeInt32(0);
        }
    }

    writeExcessivePowerToParcelLocked(out);
}

void BatteryStatsImpl::Uid::Proc::readFromParcelLocked(const Parcel& in) {  // NOLINT
    GLOGENTRY();
    sp<BatteryStatsImpl> mpbs = mPBS.promote();
    NULL_RTN(mpbs, "Promotion to sp<BatteryStatsImpl> fails");

    mUserTime = in.readInt64();
    mSystemTime = in.readInt64();
    mForegroundTime = in.readInt64();
    mStarts = in.readInt32();
    mLoadedUserTime = in.readInt64();
    mLoadedSystemTime = in.readInt64();
    mLoadedForegroundTime = in.readInt64();
    mLoadedStarts = in.readInt32();
    mLastUserTime = 0;
    mLastSystemTime = 0;
    mLastForegroundTime = 0;
    mLastStarts = 0;
    mUnpluggedUserTime = in.readInt64();
    mUnpluggedSystemTime = in.readInt64();
    mUnpluggedForegroundTime = in.readInt64();
    mUnpluggedStarts = in.readInt32();
    int32_t bins = in.readInt32();
    int32_t steps = mpbs->getCpuSpeedSteps();
    mSpeedBins = new Blob<sp<SamplingCounter> >(bins >= steps ? bins : steps);

    for (int32_t i = 0; i < bins; i++) {
        if (in.readInt32() != 0) {
            (*mSpeedBins)[i] = new SamplingCounter(mPBS, mpbs->mUnpluggables, in, 1);
        }
    }

    readExcessivePowerFromParcelLocked(in);
}

sp<BatteryStatsImpl> BatteryStatsImpl::Uid::Proc::getBatteryStats() {
    GLOGENTRY();

    sp<BatteryStatsImpl> mpbs = mPBS.promote();
    NULL_RTN_VAL(mpbs, NULL, "Promotion to sp<BatteryStatsImpl> fails");
    return mpbs;
}

void BatteryStatsImpl::Uid::Proc::addCpuTimeLocked(int32_t utime, int32_t stime) {
    GLOGENTRY();

    mUserTime += utime;
    mSystemTime += stime;
}

void BatteryStatsImpl::Uid::Proc::addForegroundTimeLocked(int64_t ttime) {
    GLOGENTRY();

    mForegroundTime += ttime;
}

void BatteryStatsImpl::Uid::Proc::incStartsLocked() {
    GLOGENTRY();

    mStarts++;
}

int64_t BatteryStatsImpl::Uid::Proc::getUserTime(int32_t which) {
    GLOGENTRY();

    int64_t val;

    if (which == STATS_LAST) {
        val = mLastUserTime;
    } else {
        val = mUserTime;

        if (which == STATS_CURRENT) {
            val -= mLoadedUserTime;
        } else if (which == STATS_SINCE_UNPLUGGED) {
            val -= mUnpluggedUserTime;
        }
    }

    return val;
}

int64_t BatteryStatsImpl::Uid::Proc::getSystemTime(int32_t which) {
    GLOGENTRY();

    int64_t val;

    if (which == STATS_LAST) {
        val = mLastSystemTime;
    } else {
        val = mSystemTime;

        if (which == STATS_CURRENT) {
            val -= mLoadedSystemTime;
        } else if (which == STATS_SINCE_UNPLUGGED) {
            val -= mUnpluggedSystemTime;
        }
    }

    return val;
}

int64_t BatteryStatsImpl::Uid::Proc::getForegroundTime(int32_t which) {
    GLOGENTRY();

    int64_t val;

    if (which == STATS_LAST) {
        val = mLastForegroundTime;
    } else {
        val = mForegroundTime;

        if (which == STATS_CURRENT) {
            val -= mLoadedForegroundTime;
        } else if (which == STATS_SINCE_UNPLUGGED) {
            val -= mUnpluggedForegroundTime;
        }
    }

    return val;
}

int32_t BatteryStatsImpl::Uid::Proc::getStarts(int32_t which) {
    GLOGENTRY();

    int32_t val;

    if (which == STATS_LAST) {
        val = mLastStarts;
    } else {
        val = mStarts;

        if (which == STATS_CURRENT) {
            val -= mLoadedStarts;
        } else if (which == STATS_SINCE_UNPLUGGED) {
            val -= mUnpluggedStarts;
        }
    }

    return val;
}

void BatteryStatsImpl::Uid::Proc::addSpeedStepTimes(Blob<int64_t> values) {
    GLOGENTRY();

    sp<BatteryStatsImpl> mpbs = mPBS.promote();
    NULL_RTN(mpbs, "Promotion to sp<BatteryStatsImpl> fails");

    for (uint32_t i = 0; i < mSpeedBins->length() && i < values.length(); i++) {
        int64_t amt = values[i];

        if (amt != 0) {
            sp<SamplingCounter> c = (*mSpeedBins)[i];

            if (c == NULL) {
                (*mSpeedBins)[i] = c = new SamplingCounter(mPBS, mpbs->mUnpluggables, 1);
            }
            c->addCountAtomic(values[i]);
        }
    }
}

int64_t BatteryStatsImpl::Uid::Proc::getTimeAtCpuSpeedStep(uint32_t speedStep, int32_t which) {
    GLOGENTRY();

    if (speedStep < mSpeedBins->length()) {
        sp<SamplingCounter> c = (*mSpeedBins)[speedStep];
        return c != NULL ? c->getCountLocked(which) : 0;
    } else {
        return 0;
    }
}

BatteryStatsImpl::Uid::Pkg::Pkg(const wp<BatteryStatsImpl::Uid>& parent, const wp<BatteryStatsImpl>& pparent)
     : Unpluggable(0),
     PREINIT_DYNAMIC(),
     mCtorSafe(new ctor_safe(this)),
     mBS(parent),
     mPBS(pparent) {
     GLOGENTRY();

     sp<BatteryStatsImpl> mpbs = mPBS.promote();
     NULL_RTN(mpbs, "Promotion to sp<BatteryStatsImpl> fails");

     mWakeups = 0;
     mLoadedWakeups = 0;
     mLastWakeups = 0;
     mUnpluggedWakeups = 0;

     // Customize +++
     // synchronized (sLockPlug)
     // Customize ---
     {
         GLOGAUTOMUTEX(_l, mpbs->mPlugLock);

         mpbs->mUnpluggables->add(this);
     }
     delete mCtorSafe;
}

void BatteryStatsImpl::Uid::Pkg::unplug(int64_t batteryUptime, int64_t batteryRealtime) {
    GLOGENTRY();

    mUnpluggedWakeups = mWakeups;
}

void BatteryStatsImpl::Uid::Pkg::plug(int64_t batteryUptime, int64_t batteryRealtime) {
    GLOGENTRY();
}

// Customize +++
void BatteryStatsImpl::Uid::Pkg::attach() {
    GLOGENTRY();

    sp<BatteryStatsImpl> mpbs = mPBS.promote();
    NULL_RTN(mpbs, "Promotion to sp<BatteryStatsImpl> fails");

    if (DEBUG_SECURITY) {
        GLOGD("Pkg attach");
    }

    // synchronized (sLockPlug)
    {
        GLOGAUTOMUTEX(_l, mpbs->mPlugLock);

        mpbs->mUnpluggables->add(this);
    }
}
// Customize ---

void BatteryStatsImpl::Uid::Pkg::detach() {
    GLOGENTRY();

    sp<BatteryStatsImpl> mpbs = mPBS.promote();
    NULL_RTN(mpbs, "Promotion to sp<BatteryStatsImpl> fails");

    // Customize +++
    // synchronized (sLockPlug)
    // Customize ---
    {
        GLOGAUTOMUTEX(_l, mpbs->mPlugLock);

        mpbs->mUnpluggables->remove(this);
    }
}

void BatteryStatsImpl::Uid::Pkg::readFromParcelLocked(const Parcel& in) {
    GLOGENTRY();

    mWakeups = in.readInt32();
    mLoadedWakeups = in.readInt32();
    mLastWakeups = 0;
    mUnpluggedWakeups = in.readInt32();
    int32_t numServs = in.readInt32();
    mServiceStats.clear();

    for (int32_t m = 0; m < numServs; m++) {
        sp<String> serviceName = in.readString();
        sp<Uid::Pkg::Serv> serv = new Serv(this, mPBS);
        mServiceStats.add(serviceName, serv);
        serv->readFromParcelLocked(in);
    }
}

void BatteryStatsImpl::Uid::Pkg::writeToParcelLocked(Parcel* out) {
    GLOGENTRY();

    out->writeInt32(mWakeups);
    out->writeInt32(mLoadedWakeups);
    out->writeInt32(mUnpluggedWakeups);

    const int32_t NSS = mServiceStats.size();
    out->writeInt32(NSS);
    for (int32_t i = 0; i < NSS; i++) {
        out->writeString(mServiceStats.keyAt(i));
        sp<Serv> serv = safe_cast<BatteryStatsImpl::Uid::Pkg::Serv*>(mServiceStats.valueAt(i));
        serv->writeToParcelLocked(out);
    }
}

KeyedVector<sp<String> , sp<BatteryStats::Uid::Pkg::Serv> >& BatteryStatsImpl::Uid::Pkg::getServiceStats() {
    GLOGENTRY();

    return mServiceStats;
}

int32_t BatteryStatsImpl::Uid::Pkg::getWakeups(int32_t which) {
    GLOGENTRY();

    int32_t val;

    if (which == STATS_LAST) {
        val = mLastWakeups;
    } else {
        val = mWakeups;

        if (which == STATS_CURRENT) {
            val -= mLoadedWakeups;
        } else if (which == STATS_SINCE_UNPLUGGED) {
            val -= mUnpluggedWakeups;
        }
    }

    return val;
}

sp<BatteryStatsImpl> BatteryStatsImpl::Uid::Pkg::getBatteryStats() {
    GLOGENTRY();

    sp<BatteryStatsImpl> mpbs = mPBS.promote();
    NULL_RTN_VAL(mpbs, NULL, "Promotion to sp<BatteryStatsImpl> fails");

    return mpbs;
}

void BatteryStatsImpl::Uid::Pkg::incWakeupsLocked() {
    GLOGENTRY();

    mWakeups++;
}

sp<BatteryStatsImpl::Uid::Pkg::Serv> BatteryStatsImpl::Uid::Pkg::newServiceStatsLocked() {
    GLOGENTRY();

    return new Serv(this, mPBS);
}

BatteryStatsImpl::Uid::Pkg::Serv::Serv(const wp<BatteryStatsImpl::Uid::Pkg>& parent, const wp<BatteryStatsImpl>& pparent)
    : Unpluggable(0),
    PREINIT_DYNAMIC(),
    mCtorSafe(new ctor_safe(this)),
    mBS(parent),
    mPBS(pparent) {
    GLOGENTRY();

    sp<BatteryStatsImpl> mpbs = mPBS.promote();
    NULL_RTN(mpbs, "Promotion to sp<BatteryStatsImpl> fails");

    mStartTime = 0;
    mRunningSince = 0;
    mRunning = 0;
    mStarts = 0;
    mLaunchedTime = 0;
    mLaunchedSince = 0;
    mLaunched = 0;
    mLaunches = 0;
    mLoadedStartTime = 0;
    mLoadedStarts = 0;
    mLoadedLaunches = 0;
    mLastStartTime = 0;
    mLastStarts = 0;
    mLastLaunches = 0;
    mUnpluggedStartTime = 0;
    mUnpluggedStarts = 0;
    mUnpluggedLaunches = 0;

    // Customize +++
    // synchronized (sLockPlug)
    // Customize ---
    {
        GLOGAUTOMUTEX(_l, mpbs->mPlugLock);

        mpbs->mUnpluggables->add(this);
    }
    delete mCtorSafe;
}

void BatteryStatsImpl::Uid::Pkg::Serv::unplug(int64_t batteryUptime, int64_t batteryRealtime) {
    GLOGENTRY();

    mUnpluggedStartTime = getStartTimeToNowLocked(batteryUptime);
    mUnpluggedStarts = mStarts;
    mUnpluggedLaunches = mLaunches;
}

void BatteryStatsImpl::Uid::Pkg::Serv::plug(int64_t batteryUptime, int64_t batteryRealtime) {
    GLOGENTRY();
}

// Customize +++
void BatteryStatsImpl::Uid::Pkg::Serv::attach() {
    GLOGENTRY();

    sp<BatteryStatsImpl> mpbs = mPBS.promote();
    NULL_RTN(mpbs, "Promotion to sp<BatteryStatsImpl> fails");

    if (DEBUG_SECURITY) {
        GLOGD("Serv attach");
    }

    // synchronized (sLockPlug)
    {
        GLOGAUTOMUTEX(_l, mpbs->mPlugLock);

        mpbs->mUnpluggables->add(this);
    }
}
// Customize ---


void BatteryStatsImpl::Uid::Pkg::Serv::detach() {
    GLOGENTRY();

    sp<BatteryStatsImpl> mpbs = mPBS.promote();
    NULL_RTN(mpbs, "Promotion to sp<BatteryStatsImpl> fails");

    // Customize +++
    // synchronized (sLockPlug)
    // Customize ---
    {
        GLOGAUTOMUTEX(_l, mpbs->mPlugLock);

        mpbs->mUnpluggables->remove(this);
    }
}

void BatteryStatsImpl::Uid::Pkg::Serv::readFromParcelLocked(const Parcel& in) {
    GLOGENTRY();

    mStartTime = in.readInt64();
    mRunningSince = in.readInt64();
    mRunning = in.readInt32() != 0;
    mStarts = in.readInt32();
    mLaunchedTime = in.readInt64();
    mLaunchedSince = in.readInt64();
    mLaunched = in.readInt32() != 0;
    mLaunches = in.readInt32();
    mLoadedStartTime = in.readInt64();
    mLoadedStarts = in.readInt32();
    mLoadedLaunches = in.readInt32();
    mLastStartTime = 0;
    mLastStarts = 0;
    mLastLaunches = 0;
    mUnpluggedStartTime = in.readInt64();
    mUnpluggedStarts = in.readInt32();
    mUnpluggedLaunches = in.readInt32();
}

void BatteryStatsImpl::Uid::Pkg::Serv::writeToParcelLocked(Parcel* out) {  // NOLINT
    GLOGENTRY();

    out->writeInt64(mStartTime);
    out->writeInt64(mRunningSince);
    out->writeInt32(mRunning ? 1 : 0);
    out->writeInt32(mStarts);
    out->writeInt64(mLaunchedTime);
    out->writeInt64(mLaunchedSince);
    out->writeInt32(mLaunched ? 1 : 0);
    out->writeInt32(mLaunches);
    out->writeInt64(mLoadedStartTime);
    out->writeInt32(mLoadedStarts);
    out->writeInt32(mLoadedLaunches);
    out->writeInt64(mUnpluggedStartTime);
    out->writeInt32(mUnpluggedStarts);
    out->writeInt32(mUnpluggedLaunches);
}

int64_t BatteryStatsImpl::Uid::Pkg::Serv::getLaunchTimeToNowLocked(int64_t batteryUptime) {
    GLOGENTRY();

    if (!mRunning) {
        return mStartTime;
    }

    return mStartTime + batteryUptime - mRunningSince;
}

int64_t BatteryStatsImpl::Uid::Pkg::Serv::getStartTimeToNowLocked(int64_t batteryUptime) {
    GLOGENTRY();

    if (!mRunning) {
        return mStartTime;
    }

    return mStartTime + batteryUptime - mRunningSince;
}

void BatteryStatsImpl::Uid::Pkg::Serv::startLaunchedLocked() {
    GLOGENTRY();

    sp<BatteryStatsImpl> mpbs = mPBS.promote();
    NULL_RTN(mpbs, "Promotion to sp<BatteryStatsImpl> fails");

    if (!mLaunched) {
        mLaunches++;
        mLaunchedSince = mpbs->getBatteryUptimeLocked();
        mLaunched = true;
    }
}

void BatteryStatsImpl::Uid::Pkg::Serv::stopLaunchedLocked() {
    GLOGENTRY();

    sp<BatteryStatsImpl> mpbs = mPBS.promote();
    NULL_RTN(mpbs, "Promotion to sp<BatteryStatsImpl> fails");

    if (mLaunched) {
        int64_t time = mpbs->getBatteryUptimeLocked() - mLaunchedSince;
        if (time > 0) {
            mLaunchedTime += time;
        } else {
            mLaunches--;
        }

        mLaunched = false;
    }
}

void BatteryStatsImpl::Uid::Pkg::Serv::startRunningLocked() {
    GLOGENTRY();

    sp<BatteryStatsImpl> mpbs = mPBS.promote();
    NULL_RTN(mpbs, "Promotion to sp<BatteryStatsImpl> fails");

    if (!mRunning) {
        mStarts++;
        mRunningSince = mpbs->getBatteryUptimeLocked();
        mRunning = true;
    }
}

void BatteryStatsImpl::Uid::Pkg::Serv::stopRunningLocked() {
    GLOGENTRY();

    sp<BatteryStatsImpl> mpbs = mPBS.promote();
    NULL_RTN(mpbs, "Promotion to sp<BatteryStatsImpl> fails");

    if (mRunning) {
        int64_t time = mpbs->getBatteryUptimeLocked() - mRunningSince;
        if (time > 0) {
            mStartTime += time;
        } else {
            mStarts--;
        }

        mRunning = false;
    }
}

sp<BatteryStatsImpl> BatteryStatsImpl::Uid::Pkg::Serv::getBatteryStats() {
    GLOGENTRY();

    sp<BatteryStatsImpl> mpbs = mPBS.promote();
    NULL_RTN_VAL(mpbs, NULL, "Promotion to sp<BatteryStatsImpl> fails");
    return mpbs;
}

int32_t BatteryStatsImpl::Uid::Pkg::Serv::getLaunches(int32_t which) {
    GLOGENTRY();

    int32_t val;

    if (which == STATS_LAST) {
        val = mLastLaunches;
    } else {
        val = mLaunches;

        if (which == STATS_CURRENT) {
            val -= mLoadedLaunches;
        } else if (which == STATS_SINCE_UNPLUGGED) {
            val -= mUnpluggedLaunches;
        }
    }

    return val;
}

int64_t BatteryStatsImpl::Uid::Pkg::Serv::getStartTime(int64_t now, int32_t which) {
    GLOGENTRY();

    int64_t val;

    if (which == STATS_LAST) {
        val = mLastStartTime;
    } else {
        val = getStartTimeToNowLocked(now);

        if (which == STATS_CURRENT) {
            val -= mLoadedStartTime;
        } else if (which == STATS_SINCE_UNPLUGGED) {
            val -= mUnpluggedStartTime;
        }
    }

    return val;
}

int32_t BatteryStatsImpl::Uid::Pkg::Serv::getStarts(int32_t which) {
    GLOGENTRY();

    int32_t val;

    if (which == STATS_LAST) {
        val = mLastStarts;
    } else {
        val = mStarts;

        if (which == STATS_CURRENT) {
            val -= mLoadedStarts;
        } else if (which == STATS_SINCE_UNPLUGGED) {
            val -= mUnpluggedStarts;
        }
    }

    return val;
}
