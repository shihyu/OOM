// Copyright for translation

#define DEBUG_LEVEL 0
#undef LOG_TAG
#define LOG_TAG "batterystats"
#include <glog.h>
#include <battery/hepenergystats/BatteryStats.h>
#include <gaiainternal/media/NullChecker.h>
#include <gaiainternal/io/PrintWriter.h>
#include <lang/StringBuilder.h>
#include <gaiainternal/text/format/DateUtils.h>
#include <battery/hepenergystats/BatteryManager.h>
#include <lang/Integer.h>
#include <utils/List.h>
#include <gaiainternal/io/StringWriter.h>
using namespace android;
USING_NAMESPACE(GAIA_NAMESPACE)

IMPLEMENT_DYNAMIC(BatteryStats, TYPEINFO(Parcelable))
IMPLEMENT_DYNAMIC(BatteryStats::Counter, TYPEINFO(Object))
IMPLEMENT_DYNAMIC(BatteryStats::Timer, TYPEINFO(Object))
IMPLEMENT_DYNAMIC(BatteryStats::Uid, TYPEINFO(Object))
IMPLEMENT_DYNAMIC(BatteryStats::Uid::Wakelock, TYPEINFO(Object))
#ifdef SENSOR_ENABLE
IMPLEMENT_DYNAMIC(BatteryStats::Uid::Sensor, TYPEINFO(Object))
#endif  // SENSOR_ENABLE
IMPLEMENT_DYNAMIC(BatteryStats::Uid::Pid, TYPEINFO(Object))
IMPLEMENT_DYNAMIC(BatteryStats::Uid::Proc, TYPEINFO(Object))
IMPLEMENT_DYNAMIC(BatteryStats::Uid::Proc::ExcessivePower, TYPEINFO(Object))
IMPLEMENT_DYNAMIC(BatteryStats::Uid::Pkg, TYPEINFO(Object))
IMPLEMENT_DYNAMIC(BatteryStats::Uid::Pkg::Serv, TYPEINFO(Object))
IMPLEMENT_DYNAMIC(BatteryStats::HistoryItem, TYPEINFO(Parcelable))
IMPLEMENT_DYNAMIC(BatteryStats::BitDescription, TYPEINFO(Object))
IMPLEMENT_DYNAMIC(BatteryStats::HistoryPrinter, TYPEINFO(Object))

const sp<Blob<sp<String> > >& BatteryStats::STAT_NAMES() {  // [4] = { String("t"), String("l"), String("c"), String("u") };
    static sp<Blob<sp<String> > > temp = initClassBlob(4,
                                         new String("t"), new String("l"), new String("c"), new String("u"));
    return temp;
}

const sp<String> BatteryStats::UID_DATA() {  // = String("uid");
    static sp<String> temp = new String("uid");
    return temp;
}


const sp<String> BatteryStats::APK_DATA() {  // = String("apk");
    static sp<String> temp = new String("apk");
    return temp;
}

const sp<String> BatteryStats::PROCESS_DATA() {  // = String("pr");
    static sp<String> temp = new String("pr");
    return temp;
}

const sp<String> BatteryStats::SENSOR_DATA() {  // = String("sr");
    static sp<String> temp = new String("sr");
    return temp;
}

const sp<String> BatteryStats::WAKELOCK_DATA() {  // = String("wl");
    static sp<String> temp = new String("wl");
    return temp;
}

const sp<String> BatteryStats::KERNEL_WAKELOCK_DATA() {  // = String("kwl");
    static sp<String> temp = new String("kwl");
    return temp;
}

const sp<String> BatteryStats::NETWORK_DATA() {  // = String("nt");
    static sp<String> temp = new String("nt");
    return temp;
}

const sp<String> BatteryStats::USER_ACTIVITY_DATA() {  // = String("ua");
    static sp<String> temp = new String("ua");
    return temp;
}

const sp<String> BatteryStats::BATTERY_DATA() {  // = String("bt");
    static sp<String> temp = new String("bt");
    return temp;
}

const sp<String> BatteryStats::BATTERY_DISCHARGE_DATA() {  // = String("dc");
    static sp<String> temp = new String("dc");
    return temp;
}

const sp<String> BatteryStats::BATTERY_LEVEL_DATA() {  // = String("lv");
    static sp<String> temp = new String("lv");
    return temp;
}

const sp<String> BatteryStats::WIFI_LOCK_DATA() {  // = String("wfl");
    static sp<String> temp = new String("wfl");
    return temp;
}

const sp<String> BatteryStats::MISC_DATA() {  // = String("m");
    static sp<String> temp = new String("m");
    return temp;
}

const sp<String> BatteryStats::SCREEN_BRIGHTNESS_DATA() {  // = String("br");
    static sp<String> temp = new String("br");
    return temp;
}

const sp<String> BatteryStats::SIGNAL_STRENGTH_TIME_DATA() {  // = String("sgt");
    static sp<String> temp = new String("sgt");
    return temp;
}

const sp<String> BatteryStats::SIGNAL_SCANNING_TIME_DATA() {  // = String("sst");
    static sp<String> temp = new String("sst");
    return temp;
}

const sp<String> BatteryStats::SIGNAL_STRENGTH_COUNT_DATA() {  // = String("sgc");
    static sp<String> temp = new String("sgc");
    return temp;
}

const sp<String> BatteryStats::DATA_CONNECTION_TIME_DATA() {  // = String("dct");
    static sp<String> temp = new String("dct");
    return temp;
}

const sp<String> BatteryStats::DATA_CONNECTION_COUNT_DATA() {  // = String("dcc");
    static sp<String> temp = new String("dcc");
    return temp;
}

const sp<Blob<sp<String> > >& BatteryStats::Uid::USER_ACTIVITY_TYPES() {
    static sp<Blob<sp<String> > > temp = initClassBlob(7 ,
                                         new String("other"), new String("cheek"), new String("touch"),
                                         new String("long_touch"), new String("touch_up"), new String("button"), new String("unknown"));
    return temp;
}

const sp<Blob<sp<String> > >& BatteryStats::SCREEN_BRIGHTNESS_NAMES() {
    static sp<Blob<sp<String> > > temp = initClassBlob(5 ,
                                         new String("dark"), new String("dim"), new String("medium"),
                                         new String("light"), new String("bright"));
    return temp;
}

const sp<Blob<sp<String> > >& BatteryStats::DATA_CONNECTION_NAMES() {
    static sp<Blob<sp<String> > > temp = initClassBlob(16,
                                         new String("none"), new String("gprs"), new String("edge"), new String("umts"),
                                         new String("cdma"), new String("evdo_0"), new String("evdo_A"), new String("1xrtt"),
                                         new String("hsdpa"), new String("hsupa"), new String("hspa"), new String("iden"),
                                         new String("evdo_b"), new String("lte"), new String("ehrpd"), new String("other"));
    return temp;
}

const sp<Blob<sp<String> > >& BatteryStats::SIGNAL_STRENGTH_NAMES() {
  static sp<Blob<sp<String> > > temp = initClassBlob(5 ,
          new String("none"), new String("poor"), new String("moderate"),
          new String("good"), new String("great"));
  return temp;
}

const sp<Blob<sp<BatteryStats::BitDescription> > >& BatteryStats::HISTORY_STATE_DESCRIPTIONS() {
    static sp<Blob<sp<BitDescription> > > temp = initClassBlob(19,
        new BitDescription(HistoryItem::STATE_BATTERY_PLUGGED_FLAG, new String("plugged")),
        new BitDescription(HistoryItem::STATE_SCREEN_ON_FLAG, new String("screen")),
        new BitDescription(HistoryItem::STATE_GPS_ON_FLAG, new String("gps")),
        new BitDescription(HistoryItem::STATE_PHONE_IN_CALL_FLAG, new String("phone_in_call")),
        new BitDescription(HistoryItem::STATE_PHONE_SCANNING_FLAG, new String("phone_scanning")),
        new BitDescription(HistoryItem::STATE_WIFI_ON_FLAG, new String("wifi")),
        new BitDescription(HistoryItem::STATE_WIFI_RUNNING_FLAG, new String("wifi_running")),
        new BitDescription(HistoryItem::STATE_WIFI_FULL_LOCK_FLAG, new String("wifi_full_lock")),
        new BitDescription(HistoryItem::STATE_WIFI_SCAN_LOCK_FLAG, new String("wifi_scan_lock")),
        new BitDescription(HistoryItem::STATE_WIFI_MULTICAST_ON_FLAG, new String("wifi_multicast")),
        new BitDescription(HistoryItem::STATE_BLUETOOTH_ON_FLAG, new String("bluetooth")),
        new BitDescription(HistoryItem::STATE_AUDIO_ON_FLAG, new String("audio")),
        new BitDescription(HistoryItem::STATE_VIDEO_ON_FLAG, new String("video")),
        new BitDescription(HistoryItem::STATE_WAKE_LOCK_FLAG, new String("wake_lock")),
        new BitDescription(HistoryItem::STATE_SENSOR_ON_FLAG, new String("sensor")),
        new BitDescription(HistoryItem::STATE_BRIGHTNESS_MASK,
                HistoryItem::STATE_BRIGHTNESS_SHIFT, new String("brightness"),
                SCREEN_BRIGHTNESS_NAMES()),

        new BitDescription(HistoryItem::STATE_SIGNAL_STRENGTH_MASK,
                HistoryItem::STATE_SIGNAL_STRENGTH_SHIFT, new String("signal_strength"),
                BatteryStats::SIGNAL_STRENGTH_NAMES()),
        new BitDescription(HistoryItem::STATE_PHONE_STATE_MASK,
                HistoryItem::STATE_PHONE_STATE_SHIFT, new String("phone_state"),
                initClassBlob<String>(4, new String("in"), new String("out"), new String("emergency"), new String("off"))),
        new BitDescription(HistoryItem::STATE_DATA_CONNECTION_MASK,
                HistoryItem::STATE_DATA_CONNECTION_SHIFT, new String("data_conn"),
                DATA_CONNECTION_NAMES()));

    return temp;
}

BatteryStats::Uid::Pid::Pid()
    :PREINIT_DYNAMIC(),
    mWakeSum(0),
    mWakeStart(0) {
    GLOGENTRY();
}

BatteryStats::Uid::Proc::ExcessivePower::ExcessivePower()
    :PREINIT_DYNAMIC(),
    type(0),
    overTime(0),
    usedTime(0) {
    GLOGENTRY();
}

int64_t BatteryStats::getRadioDataUptimeMs() {
    GLOGENTRY();

    return getRadioDataUptime() / 1000;
}

void BatteryStats::formatTimeRaw(const sp<StringBuilder>& out, int64_t seconds) {
    GLOGENTRY();

    IF_NULL_RETURN(out);
    int64_t days = seconds / (60 * 60 * 24);
    char buf[25];

    if (days != 0) {
        snprintf(buf, sizeof(buf), "%lld", days);
        out->append(buf);
        out->append(String("d "));
    }

    int64_t used = days * 60 * 60 * 24;
    int64_t hours = (seconds - used) / (60 * 60);

    if (hours != 0 || used != 0) {
        snprintf(buf, sizeof(buf), "%lld", hours);
        out->append(buf);
        out->append(String("h "));
    }

    used += hours * 60 * 60;
    int64_t mins = (seconds - used) / 60;

    if (mins != 0 || used != 0) {
        snprintf(buf, sizeof(buf), "%lld", mins);
        out->append(buf);
        out->append(String("m "));
    }

    used += mins * 60;

    if (seconds != 0 || used != 0) {
        snprintf(buf, sizeof(buf), "%lld", seconds - used);
        out->append(buf);
        out->append(String("s "));
    }
}

void BatteryStats::formatTime(const sp<StringBuilder>& sb, int64_t time) {
    GLOGENTRY();

    IF_NULL_RETURN(sb);
    int64_t sec = time / 100;
    char buf[25];
    formatTimeRaw(sb, sec);
    snprintf(buf, sizeof(buf), "%lld", (time - (sec * 100)) * 10);
    sb->append(buf);
    sb->append(String("ms "));
}

void BatteryStats::formatTimeMs(const sp<StringBuilder>& sb, int64_t time) {
    GLOGENTRY();

    IF_NULL_RETURN(sb);
    int64_t sec = time / 1000;
    char buf[25];
    formatTimeRaw(sb, sec);
    snprintf(buf, sizeof(buf), "%lld", time - (sec * 1000));
    sb->append(buf);
    sb->append(String("ms "));
}

sp<String> BatteryStats::formatRatioLocked(int64_t num, int64_t den) {
    GLOGENTRY();

    char buf[25];

    if (den == 0L) {
        return new String("---%");
    }

    float perc = (static_cast<float>(num)) / (static_cast<float>(den)) * 100;
    snprintf(buf, sizeof(buf), "%.1f%%", perc);
    return new String(buf);
}

sp<String> BatteryStats::formatBytesLocked(int64_t bytes) {
    GLOGENTRY();

    sp<StringBuilder> _result = new StringBuilder();
    char buf[30];

    if (_result != NULL) {
        if (bytes < BYTES_PER_KB) {
            snprintf(buf, sizeof(buf), "%lld", bytes);
            _result->append(buf);
            _result->append(String("B"));
        } else if (bytes < BYTES_PER_MB) {
            snprintf(buf, sizeof(buf), "%.2fKB", bytes / static_cast<double>(BYTES_PER_KB));
            _result->append(buf);
        } else if (bytes < BYTES_PER_GB) {
            snprintf(buf, sizeof(buf), "%.2fMB", bytes / static_cast<double>(BYTES_PER_MB));
            _result->append(buf);
        } else {
            snprintf(buf, sizeof(buf), "%.2fGB", bytes / static_cast<double>(BYTES_PER_GB));
            _result->append(buf);
        }

        return _result->toString();
    } else {
        return NULL;
    }
}

int64_t BatteryStats::computeWakeLock(const sp<Timer>& timer, int64_t batteryRealtime, int32_t which) {
    GLOGENTRY();

    if (timer != NULL) {
        // Convert from microseconds to milliseconds with rounding
        int64_t totalTimeMicros = timer->getTotalTimeLocked(batteryRealtime, which);
        int64_t totalTimeMillis = (totalTimeMicros + 500) / 1000;
        return totalTimeMillis;
    }
    return 0;
}

sp<String> BatteryStats::printWakeLock(const sp<StringBuilder>& sb,
                                       const sp<Timer> & timer,
                                       int64_t batteryRealtime,
                                       const sp<String>& name,
                                       int32_t which,
                                       const sp<String>& linePrefix) {
    GLOGENTRY();

    if (timer != NULL) {
        int64_t  totalTimeMillis = computeWakeLock(timer, batteryRealtime, which);
        int32_t count = timer->getCountLocked(which);
        char buf[25];

        if (totalTimeMillis != 0) {
            sb->append(linePrefix);
            formatTimeMs(sb, totalTimeMillis);

            if (name != NULL) {
                sb->append(name);
            }
            sb->append(String(" "));
            sb->append(String("("));
            snprintf(buf, sizeof(buf), "%d", count);
            sb->append(buf);
            sb->append(String(" times)"));
            return new String(", ");
        }
    }

    return linePrefix;
}

sp<String> BatteryStats::printWakeLockCheckin(const sp<StringBuilder>& sb,
                                              const sp<Timer> & timer,
                                              int64_t now,
                                              const sp<String>& name,
                                              int32_t which,
                                              const sp<String>& linePrefix) {
    GLOGENTRY();

    int64_t totalTimeMicros = 0;
    int32_t count = 0;
    char buf[25];

    if (timer != NULL) {
        totalTimeMicros = timer->getTotalTimeLocked(now, which);
        count = timer->getCountLocked(which);
    }

    if (sb != NULL) {
        sb->append(linePrefix);
        snprintf(buf, sizeof(buf), "%lld", (totalTimeMicros + 500) / 1000);  // microseconds to milliseconds with rounding
        sb->append(buf);
        sb->append(String(","));

        if (name != NULL)
            sb->append(name + new String(","));
        else
            sb->append(String(""));

        snprintf(buf, sizeof(buf), "%d", count);
        sb->append(buf);
    }

    return new String(",");
}

void BatteryStats::dumpLine(sp<PrintWriter> & pw,
                            int32_t uid,
                            const sp<String>& category,
                            const sp<String>& type,
                            const Vector<sp<String> >& args
                            ) {
    GLOGENTRY();

    IF_NULL_RETURN(pw);
    pw->print(BATTERY_STATS_CHECKIN_VERSION);
    pw->print(',');
    pw->print(uid);
    pw->print(',');
    pw->print(category);
    pw->print(',');
    pw->print(type);

    for (int32_t i = 0; i < static_cast<int32_t>(args.size()); ++i) {
        pw->print(',');
        pw->print(args[i]);
    }
    pw->print('\n');
}

void BatteryStats::dumpCheckinLocked(int32_t fd, sp<PrintWriter>& pw, int32_t which, int32_t reqUid) {  // NOLINT
    GLOGENTRY();

    if (fd != -1) {
        sp<StringWriter> sw = new StringWriter();
        sp<PrintWriter> pw_ = new PrintWriter(sp<Writer>(sw.get()));

        if (pw_ == NULL) {
            GLOGW("pw_ = NULL");
            return;
        }

        const int64_t rawUptime = uptimeMillis() * 1000;
        const int64_t rawRealtime = elapsedRealtime() * 1000;
        const int64_t batteryUptime = getBatteryUptime(rawUptime);
        const int64_t batteryRealtime = getBatteryRealtime(rawRealtime);
        const int64_t whichBatteryUptime = computeBatteryUptime(rawUptime, which);
        const int64_t whichBatteryRealtime = computeBatteryRealtime(rawRealtime, which);
        const int64_t totalRealtime = computeRealtime(rawRealtime, which);
        const int64_t totalUptime = computeUptime(rawUptime, which);
        const int64_t screenOnTime = getScreenOnTime(batteryRealtime, which);
    #ifdef PHONE_ENABLE
        const int64_t phoneOnTime = getPhoneOnTime(batteryRealtime, which);
    #endif  // PHONE_ENABLE
    #ifdef WIFI_ENABLE
        const int64_t wifiOnTime = getWifiOnTime(batteryRealtime, which);
        const int64_t wifiRunningTime = getGlobalWifiRunningTime(batteryRealtime, which);
    #else
        const int64_t wifiOnTime = 0;
        const int64_t wifiRunningTime = 0;
    #endif  // WIFI_ENABLE

    #ifndef BT_DISABLE
        const int64_t bluetoothOnTime = getBluetoothOnTime(batteryRealtime, which);
    #endif  // BT_DISABLE
        sp<StringBuilder> sb = new StringBuilder(128);
        KeyedVector<int32_t, sp<BatteryStats::Uid> > uidStats = getUidStats();
        const int32_t NU = uidStats.size();
        sp<String> category = (*STAT_NAMES())[which];
        // Dump "battery" stat

        Vector<sp<String> > tmp_string = Vector<sp<String> >();

        if (which == STATS_SINCE_CHARGED) {
            tmp_string.clear();
            tmp_string.push(String::valueOf(static_cast<int32_t>(getStartCount())));
            tmp_string.push(String::valueOf(static_cast<int64_t>(whichBatteryRealtime / 1000)));
            tmp_string.push(String::valueOf(static_cast<int64_t>(whichBatteryUptime / 1000)));
            tmp_string.push(String::valueOf(static_cast<int64_t>(totalRealtime / 1000)));
            tmp_string.push(String::valueOf(static_cast<int64_t>(totalUptime / 1000)));
            dumpLine(pw_, 0 /* uid */, category, BATTERY_DATA(), tmp_string);

            if (sw != NULL && sw->toString() != NULL) {
                write(fd, sw->toString()->string(), sw->toString()->size());
                sw = new StringWriter();
                pw_ = new PrintWriter(sp<Writer>(sw.get()));
            }
        } else {
            tmp_string.clear();
            tmp_string.push(new String("N/A"));
            tmp_string.push(String::valueOf(static_cast<int64_t>(whichBatteryRealtime / 1000)));
            tmp_string.push(String::valueOf(static_cast<int64_t>(whichBatteryUptime / 1000)));
            tmp_string.push(String::valueOf(static_cast<int64_t>(totalRealtime / 1000)));
            tmp_string.push(String::valueOf(static_cast<int64_t>(totalUptime / 1000)));
            dumpLine(pw_, 0 /* uid */, category, BATTERY_DATA(), tmp_string);

            if (sw != NULL && sw->toString() != NULL) {
                write(fd, sw->toString()->string(), sw->toString()->size());
                sw = new StringWriter();
                pw_ = new PrintWriter(sp<Writer>(sw.get()));
            }
        }

        // Calculate total network and wakelock times across all uids.
        int64_t rxTotal = 0;
        int64_t txTotal = 0;
        int64_t fullWakeLockTimeTotal = 0;
        int64_t partialWakeLockTimeTotal = 0;

        for (int32_t iu = 0; iu < NU; iu++) {
            sp<Uid> u = uidStats.valueAt(iu);
            rxTotal += u->getTcpBytesReceived(which);
            txTotal += u->getTcpBytesSent(which);
            KeyedVector<sp<String>, sp<BatteryStats::Uid::Wakelock> > wakelocks = u->getWakelockStats();

            const int32_t WL = wakelocks.size();
            if (WL > 0) {
                    for (int32_t i = 0; i < WL; i++) {
                    sp<Uid::Wakelock> wl = wakelocks.valueAt(i);
                    sp<Timer> fullWakeTimer = wl->getWakeTime(WAKE_TYPE_FULL);

                    if (fullWakeTimer != NULL) {
                        fullWakeLockTimeTotal += fullWakeTimer->getTotalTimeLocked(batteryRealtime, which);
                    }

                    sp<Timer> partialWakeTimer = wl->getWakeTime(WAKE_TYPE_PARTIAL);

                    if (partialWakeTimer != NULL) {
                        partialWakeLockTimeTotal += partialWakeTimer->getTotalTimeLocked(
                                                        batteryRealtime, which);
                    }
                }
            }
        }

        tmp_string.clear();
        tmp_string.push(new String("N/A"));
        tmp_string.push(String::valueOf(static_cast<int64_t>(screenOnTime / 1000)));
    #ifdef PHONE_ENABLE
        tmp_string.push(String::valueOf(static_cast<int64_t>(phoneOnTime / 1000)));
    #endif  // PHONE_ENABLE
    #ifdef WIFI_ENABLE
        tmp_string.push(String::valueOf(static_cast<int64_t>(wifiOnTime / 1000)));
        tmp_string.push(String::valueOf(static_cast<int64_t>(wifiRunningTime / 1000)));
    #endif  // WIFI_ENABLE
    #ifndef BT_DISABLE
        tmp_string.push(String::valueOf(static_cast<int64_t>(bluetoothOnTime / 1000)));
    #endif  // BT_DISABLE
        tmp_string.push(String::valueOf(static_cast<int64_t>(rxTotal)));
        tmp_string.push(String::valueOf(static_cast<int64_t>(txTotal)));
        tmp_string.push(String::valueOf(static_cast<int64_t>(fullWakeLockTimeTotal)));
        tmp_string.push(String::valueOf(static_cast<int64_t>(partialWakeLockTimeTotal)));
        tmp_string.push(String::valueOf(static_cast<int64_t>(getInputEventCount(which))));
        dumpLine(pw_, 0 /* uid */, category, MISC_DATA(), tmp_string);

        if (sw != NULL && sw->toString() != NULL) {
            write(fd, sw->toString()->string(), sw->toString()->size());
        }

        // Dump screen brightness stats
        // Object[] args = new Object[NUM_SCREEN_BRIGHTNESS_BINS];
        sp<Blob<int64_t> > args = new Blob<int64_t>(NUM_SCREEN_BRIGHTNESS_BINS);

    #ifdef BACKLIGHT_ENABLE
        for (int32_t i = 0; i < NUM_SCREEN_BRIGHTNESS_BINS; i++) {
            (*args)[i] = getScreenBrightnessTime(i, batteryRealtime, which) / 1000;
        }

        tmp_string.clear();

        for (int32_t i = 0; i < NUM_SCREEN_BRIGHTNESS_BINS; i++) {
            tmp_string.push(String::valueOf(static_cast<int64_t>(args[i])));
        }

        dumpLine(pw_, 0 /* uid */, category, SCREEN_BRIGHTNESS_DATA(), tmp_string);

        if (sw != NULL && sw->toString() != NULL) {
            write(fd, sw->toString()->string(), sw->toString()->size());
        }
    #endif   // BACKLIGHT_ENABLE

    #ifdef PHONE_ENABLE
        // Dump signal strength stats
        // args = new Object[SignalStrength::NUM_SIGNAL_STRENGTH_BINS];
        args = new Blob<int64_t>(SignalStrength::NUM_SIGNAL_STRENGTH_BINS);

        for (int32_t i = 0; i < SignalStrength::NUM_SIGNAL_STRENGTH_BINS; i++) {
            (*args)[i] = getPhoneSignalStrengthTime(i, batteryRealtime, which) / 1000;
        }

        tmp_string.clear();

        for (int32_t i = 0; i < SignalStrength::NUM_SIGNAL_STRENGTH_BINS; i++) {
            tmp_string.push(String::valueOf(static_cast<int64_t>(args[i])));
        }

        dumpLine(pw_, 0 /* uid */, category, SIGNAL_STRENGTH_TIME_DATA(), tmp_string);

        if (sw != NULL && sw->toString() != NULL) {
            write(fd, sw->toString()->string(), sw->toString()->size());
        }

        tmp_string.clear();
        tmp_string.push(String::valueOf(static_cast<int64_t>(getPhoneSignalScanningTime(batteryRealtime, which) / 1000)));
        dumpLine(pw_, 0 /* uid */, category, SIGNAL_SCANNING_TIME_DATA(), tmp_string);

        if (sw != NULL && sw->toString() != NULL) {
            write(fd, sw->toString()->string(), sw->toString()->size());
        }

        for (int32_t i = 0; i < SignalStrength::NUM_SIGNAL_STRENGTH_BINS; i++) {
            (*args)[i] = getPhoneSignalStrengthCount(i, which);
        }

        tmp_string.clear();

        for (int32_t i = 0; i < SignalStrength::NUM_SIGNAL_STRENGTH_BINS; i++) {
            tmp_string.push(String::valueOf(static_cast<int64_t>(args[i])));
        }

        dumpLine(pw_, 0 /* uid */, category, SIGNAL_STRENGTH_COUNT_DATA(), tmp_string);

        if (sw != NULL && sw->toString() != NULL) {
            write(fd, sw->toString()->string(), sw->toString()->size());
        }

        // Dump network type stats
        // args = new Object[NUM_DATA_CONNECTION_TYPES];
        args = new Blob<int64_t>(NUM_DATA_CONNECTION_TYPES);

        for (int32_t i = 0; i < NUM_DATA_CONNECTION_TYPES; i++) {
            (*args)[i] = getPhoneDataConnectionTime(i, batteryRealtime, which) / 1000;
        }

        tmp_string.clear();

        for (int32_t i = 0; i < NUM_DATA_CONNECTION_TYPES; i++) {
            tmp_string.push(String::valueOf(static_cast<int64_t>(args[i])));
        }

        dumpLine(pw_, 0 /* uid */, category, DATA_CONNECTION_TIME_DATA(), tmp_string);

        if (sw != NULL && sw->toString() != NULL) {
            write(fd, sw->toString()->string(), sw->toString()->size());
        }

        for (int32_t i = 0; i < NUM_DATA_CONNECTION_TYPES; i++) {
            (*args)[i] = getPhoneDataConnectionCount(i, which);
        }

        tmp_string.clear();

        for (int32_t i = 0; i < NUM_DATA_CONNECTION_TYPES; i++) {
            tmp_string.push(String::valueOf(static_cast<int64_t>(args[i])));
        }

        dumpLine(pw_, 0 /* uid */, category, DATA_CONNECTION_COUNT_DATA(), tmp_string);

        if (sw != NULL && sw->toString() != NULL) {
            write(fd, sw->toString()->string(), sw->toString()->size());
        }
    #endif  // PHONE_ENABLE
        if (which == STATS_SINCE_UNPLUGGED) {
            tmp_string.clear();
            tmp_string.push(String::valueOf(static_cast<int32_t>(getDischargeStartLevel())));
            tmp_string.push(String::valueOf(static_cast<int32_t>(getDischargeCurrentLevel())));

            dumpLine(pw_, 0 /* uid */, category, BATTERY_LEVEL_DATA(), tmp_string);

            if (sw != NULL && sw->toString() != NULL) {
                write(fd, sw->toString()->string(), sw->toString()->size());
            }
        }

        if (which == STATS_SINCE_UNPLUGGED) {
            tmp_string.clear();
            tmp_string.push(String::valueOf(static_cast<int32_t>(getDischargeStartLevel() - getDischargeCurrentLevel())));
            tmp_string.push(String::valueOf(static_cast<int32_t>(getDischargeStartLevel() - getDischargeCurrentLevel())));
            tmp_string.push(String::valueOf(static_cast<int32_t>(getDischargeAmountScreenOn())));
            tmp_string.push(String::valueOf(static_cast<int32_t>(getDischargeAmountScreenOff())));
            dumpLine(pw_, 0 /* uid */, category, BATTERY_DISCHARGE_DATA(), tmp_string);

            if (sw != NULL && sw->toString() != NULL) {
                write(fd, sw->toString()->string(), sw->toString()->size());
            }
        } else {
            tmp_string.clear();
            tmp_string.push(String::valueOf(static_cast<int32_t>(getLowDischargeAmountSinceCharge())));
            tmp_string.push(String::valueOf(static_cast<int32_t>(getHighDischargeAmountSinceCharge())));
            tmp_string.push(String::valueOf(static_cast<int32_t>(getDischargeAmountScreenOn())));
            tmp_string.push(String::valueOf(static_cast<int32_t>(getDischargeAmountScreenOff())));
            dumpLine(pw_, 0 /* uid */, category, BATTERY_DISCHARGE_DATA(), tmp_string);

            if (sw != NULL && sw->toString() != NULL) {
                write(fd, sw->toString()->string(), sw->toString()->size());
            }
        }

        if (reqUid < 0) {
            // Map < String, ? extends BatteryStats.Timer > kernelWakelocks = getKernelWakelockStats();
            KeyedVector<sp<String>, sp<BatteryStats::Timer> > kernelWakelocks = getKernelWakelockStats();

            if (kernelWakelocks.size() > 0) {
                const int32_t KWL = kernelWakelocks.size();
                for (int32_t i = 0; i < KWL; i++) {
                    sb->setLength(0);
                    printWakeLockCheckin(sb, kernelWakelocks.valueAt(i), batteryRealtime, NULL, which, new String(""));
                    tmp_string.clear();
                    tmp_string.push(kernelWakelocks.keyAt(i));
                    tmp_string.push(sb->toString());
                    dumpLine(pw_, 0 /* uid */, category, KERNEL_WAKELOCK_DATA(), tmp_string);

                    if (sw != NULL && sw->toString() != NULL) {
                        write(fd, sw->toString()->string(), sw->toString()->size());
                    }
                }
            }
        }

        for (int32_t iu = 0; iu < NU; iu++) {
            const int32_t uid = uidStats.keyAt(iu);

            if (reqUid >= 0 && uid != reqUid) {
                continue;
            }

            sp<Uid> u = uidStats.valueAt(iu);
            // Dump Network stats per uid, if any
            int64_t rx = u->getTcpBytesReceived(which);
            int64_t tx = u->getTcpBytesSent(which);
    #ifdef WIFI_ENABLE
            int64_t fullWifiLockOnTime = u->getFullWifiLockTime(batteryRealtime, which);
            int64_t scanWifiLockOnTime = u->getScanWifiLockTime(batteryRealtime, which);
            int64_t uidWifiRunningTime = u->getWifiRunningTime(batteryRealtime, which);
    #endif  // WIFI_ENABLE
            if (rx > 0 || tx > 0) {
                tmp_string.clear();
                tmp_string.push(String::valueOf(static_cast<int32_t>(rx)));
                tmp_string.push(String::valueOf(static_cast<int32_t>(tx)));
                dumpLine(pw_, uid, category, NETWORK_DATA(), tmp_string);

                if (sw != NULL && sw->toString() != NULL) {
                    write(fd, sw->toString()->string(), sw->toString()->size());
                }
            }

    #ifdef WIFI_ENABLE
            if (fullWifiLockOnTime != 0 || scanWifiLockOnTime != 0
                    || uidWifiRunningTime != 0) {
                tmp_string.clear();
                tmp_string.push(String::valueOf(static_cast<int32_t>(fullWifiLockOnTime)));
                tmp_string.push(String::valueOf(static_cast<int32_t>(scanWifiLockOnTime)));
                tmp_string.push(String::valueOf(static_cast<int32_t>(uidWifiRunningTime)));
                dumpLine(pw_, uid, category, WIFI_LOCK_DATA(), tmp_string);

                if (sw != NULL && sw->toString() != NULL) {
                    write(fd, sw->toString()->string(), sw->toString()->size());
                }
            }
    #endif  // WIFI_ENABLE
            if (u->hasUserActivity()) {
                // args = new Object[Uid.NUM_USER_ACTIVITY_TYPES];
                args = new Blob<int64_t>(Uid::NUM_USER_ACTIVITY_TYPES);
                bool hasData = false;

                for (int32_t i = 0; i < Uid::NUM_USER_ACTIVITY_TYPES; i++) {
                    int32_t val = u->getUserActivityCount(i, which);
                    args[i] = val;

                    if (val != 0) hasData = true;
                }

                if (hasData) {
                    tmp_string.clear();
                    for (int32_t i = 0; i < Uid::NUM_USER_ACTIVITY_TYPES; i++) {
                        tmp_string.push(String::valueOf(static_cast<int64_t>(args[i])));
                    }
                    dumpLine(pw_, 0 /* uid */, category, USER_ACTIVITY_DATA(), tmp_string);

                    if (sw != NULL && sw->toString() != NULL) {
                        write(fd, sw->toString()->string(), sw->toString()->size());
                    }
                }
            }

            // Map < String, ? extends BatteryStats.Uid.Wakelock > wakelocks = u->getWakelockStats();
            KeyedVector<sp<String>, sp<BatteryStats::Uid::Wakelock> > wakelocks = u->getWakelockStats();
            if (wakelocks.size() > 0) {
                const int32_t WL = wakelocks.size();
                for (int32_t i = 0; i < WL; i++) {
                    sp<Uid::Wakelock> wl = wakelocks.valueAt(i);
                    sp<String> linePrefix = new String("");
                    sb->setLength(0);
                    linePrefix = printWakeLockCheckin(sb, wl->getWakeTime(WAKE_TYPE_FULL),
                                                      batteryRealtime, new String("f"), which, linePrefix);
                    linePrefix = printWakeLockCheckin(sb, wl->getWakeTime(WAKE_TYPE_PARTIAL),
                                                      batteryRealtime, new String("p"), which, linePrefix);
                    linePrefix = printWakeLockCheckin(sb, wl->getWakeTime(WAKE_TYPE_WINDOW),
                                                      batteryRealtime, new String("w"), which, linePrefix);

                    // Only log if we had at lease one wakelock...
                    if (sb->length() > 0) {
                        tmp_string.clear();
                        tmp_string.push(wakelocks.keyAt(i));
                        tmp_string.push(sb->toString());
                        dumpLine(pw_, uid, category, WAKELOCK_DATA(), tmp_string);

                        if (sw != NULL && sw->toString() != NULL) {
                            write(fd, sw->toString()->string(), sw->toString()->size());
                        }
                    }
                }
            }

    #ifdef SENSOR_ENABLE
            // Map < Integer, ? extends BatteryStats.Uid.Sensor > sensors = u->getSensorStats();
            KeyedVector<int32_t, sp<BatteryStats::Uid::Sensor> > sensors = u->getSensorStats();

            if (sensors.size() > 0)  {
                const int32_t SS = sensors.size();
                for (int32_t i = 0; i < SS; i++) {
                    sp<Uid::Sensor> se = sensors.valueAt(i);
                    int32_t sensorNumber = sensors.keyAt(i);
                    sp<Timer> timer = se->getSensorTime();

                    if (timer != NULL) {
                        // Convert from microseconds to milliseconds with rounding
                        int64_t totalTime = (timer->getTotalTimeLocked(batteryRealtime, which) + 500) / 1000;
                        int32_t count = timer->getCountLocked(which);

                        if (totalTime != 0) {
                            tmp_string.clear();
                            tmp_string.push(String::valueOf(static_cast<int32_t>(sensorNumber)));
                            tmp_string.push(String::valueOf(static_cast<int64_t>(totalTime)));
                            tmp_string.push(String::valueOf(static_cast<int32_t>(count)));
                            dumpLine(pw_, uid, category, SENSOR_DATA(), tmp_string);

                            if (sw != NULL && sw->toString() != NULL) {
                                write(fd, sw->toString()->string(), sw->toString()->size());
                            }
                        }
                    }
                }
            }
    #endif  // SENSOR_ENABLE

            // Map < String, ? extends BatteryStats.Uid.Proc > processStats = u->getProcessStats();
            KeyedVector<sp<String>, sp<BatteryStats::Uid::Proc> > processStats = u->getProcessStats();
            if (processStats.size() > 0) {
                const int32_t PS = processStats.size();
                for (int32_t i = 0; i < PS; i++) {
                    sp<Uid::Proc> ps = processStats.valueAt(i);
                    int64_t userTime = ps->getUserTime(which);
                    int64_t systemTime = ps->getSystemTime(which);
                    int32_t starts = ps->getStarts(which);

                    if (userTime != 0 || systemTime != 0 || starts != 0) {
                        tmp_string.clear();
                        tmp_string.push(processStats.keyAt(i));  // proc
                        tmp_string.push(String::valueOf(static_cast<int64_t>(userTime * 10)));          // cpu time in ms
                        tmp_string.push(String::valueOf(static_cast<int64_t>(systemTime * 10)));        // user time in ms
                        tmp_string.push(String::valueOf(static_cast<int32_t>(starts)));                 // process starts
                        dumpLine(pw_, uid, category, PROCESS_DATA(), tmp_string);

                        if (sw != NULL && sw->toString() != NULL) {
                            write(fd, sw->toString()->string(), sw->toString()->size());
                        }
                    }
                }
            }

            // Map < String, ? extends BatteryStats.Uid.Pkg > packageStats = u->getPackageStats();
            KeyedVector<sp<String>, sp<BatteryStats::Uid::Pkg> > packageStats = u->getPackageStats();
            if (packageStats.size() > 0) {
                const int32_t PS = packageStats.size();
                for (int32_t i = 0; i < PS; i++) {
                    sp<Uid::Pkg> ps = packageStats.valueAt(i);
                    int32_t wakeups = ps->getWakeups(which);
                    // Map < String, ? extends  Uid.Pkg.Serv > serviceStats = ps.getServiceStats();
                    KeyedVector<sp<String>, sp<BatteryStats::Uid::Pkg::Serv> > serviceStats = ps->getServiceStats();

                    const int32_t SS = serviceStats.size();
                    for (int32_t j = 0; j < SS; j++) {
                        sp<BatteryStats::Uid::Pkg::Serv> ss = serviceStats.valueAt(j);
                        int64_t startTime = ss->getStartTime(batteryUptime, which);
                        int32_t starts = ss->getStarts(which);
                        int32_t launches = ss->getLaunches(which);

                        if (startTime != 0 || starts != 0 || launches != 0) {
                            tmp_string.clear();
                            tmp_string.push(String::valueOf(static_cast<int32_t>(wakeups)));
                            tmp_string.push(packageStats.keyAt(i));
                            tmp_string.push(serviceStats.keyAt(j));
                            tmp_string.push(String::valueOf(static_cast<int32_t>(startTime / 1000)));
                            tmp_string.push(String::valueOf(static_cast<int32_t>(starts)));
                            tmp_string.push(String::valueOf(static_cast<int32_t>(launches)));
                            dumpLine(pw_, uid, category, APK_DATA(), tmp_string);

                            if (sw != NULL && sw->toString() != NULL) {
                                write(fd, sw->toString()->string(), sw->toString()->size());
                            }
                        }
                    }
                }
            }
        }
    } else {
        const int64_t rawUptime = uptimeMillis() * 1000;
        const int64_t rawRealtime = elapsedRealtime() * 1000;
        const int64_t batteryUptime = getBatteryUptime(rawUptime);
        const int64_t batteryRealtime = getBatteryRealtime(rawRealtime);
        const int64_t whichBatteryUptime = computeBatteryUptime(rawUptime, which);
        const int64_t whichBatteryRealtime = computeBatteryRealtime(rawRealtime, which);
        const int64_t totalRealtime = computeRealtime(rawRealtime, which);
        const int64_t totalUptime = computeUptime(rawUptime, which);
        const int64_t screenOnTime = getScreenOnTime(batteryRealtime, which);
    #ifdef PHONE_ENABLE
        const int64_t phoneOnTime = getPhoneOnTime(batteryRealtime, which);
    #endif  // PHONE_ENABLE
    #ifdef WIFI_ENABLE
        const int64_t wifiOnTime = getWifiOnTime(batteryRealtime, which);
        const int64_t wifiRunningTime = getGlobalWifiRunningTime(batteryRealtime, which);
    #else
        const int64_t wifiOnTime = 0;
        const int64_t wifiRunningTime = 0;
    #endif  // WIFI_ENABLE

    #ifndef BT_DISABLE
        const int64_t bluetoothOnTime = getBluetoothOnTime(batteryRealtime, which);
    #endif  // BT_DISABLE
        sp<StringBuilder> sb = new StringBuilder(128);
        KeyedVector<int32_t, sp<BatteryStats::Uid> > uidStats = getUidStats();
        const int32_t NU = uidStats.size();
        sp<String> category = (*STAT_NAMES())[which];
        // Dump "battery" stat

        Vector<sp<String> > tmp_string = Vector<sp<String> >();

        if (which == STATS_SINCE_CHARGED) {
            tmp_string.clear();
            tmp_string.push(String::valueOf(static_cast<int32_t>(getStartCount())));
            tmp_string.push(String::valueOf(static_cast<int64_t>(whichBatteryRealtime / 1000)));
            tmp_string.push(String::valueOf(static_cast<int64_t>(whichBatteryUptime / 1000)));
            tmp_string.push(String::valueOf(static_cast<int64_t>(totalRealtime / 1000)));
            tmp_string.push(String::valueOf(static_cast<int64_t>(totalUptime / 1000)));
            dumpLine(pw, 0 /* uid */, category, BATTERY_DATA(), tmp_string);
        } else {
            tmp_string.clear();
            tmp_string.push(new String("N/A"));
            tmp_string.push(String::valueOf(static_cast<int64_t>(whichBatteryRealtime / 1000)));
            tmp_string.push(String::valueOf(static_cast<int64_t>(whichBatteryUptime / 1000)));
            tmp_string.push(String::valueOf(static_cast<int64_t>(totalRealtime / 1000)));
            tmp_string.push(String::valueOf(static_cast<int64_t>(totalUptime / 1000)));
            dumpLine(pw, 0 /* uid */, category, BATTERY_DATA(), tmp_string);
        }

        // Calculate total network and wakelock times across all uids.
        int64_t rxTotal = 0;
        int64_t txTotal = 0;
        int64_t fullWakeLockTimeTotal = 0;
        int64_t partialWakeLockTimeTotal = 0;

        for (int32_t iu = 0; iu < NU; iu++) {
            sp<Uid> u = uidStats.valueAt(iu);
            rxTotal += u->getTcpBytesReceived(which);
            txTotal += u->getTcpBytesSent(which);
            KeyedVector<sp<String>, sp<BatteryStats::Uid::Wakelock> > wakelocks = u->getWakelockStats();

            const int32_t WL = wakelocks.size();
            if (WL > 0) {
                    for (int32_t i = 0; i < WL; i++) {
                    sp<Uid::Wakelock> wl = wakelocks.valueAt(i);
                    sp<Timer> fullWakeTimer = wl->getWakeTime(WAKE_TYPE_FULL);

                    if (fullWakeTimer != NULL) {
                        fullWakeLockTimeTotal += fullWakeTimer->getTotalTimeLocked(batteryRealtime, which);
                    }

                    sp<Timer> partialWakeTimer = wl->getWakeTime(WAKE_TYPE_PARTIAL);

                    if (partialWakeTimer != NULL) {
                        partialWakeLockTimeTotal += partialWakeTimer->getTotalTimeLocked(
                                                        batteryRealtime, which);
                    }
                }
            }
        }

        tmp_string.clear();
        tmp_string.push(new String("N/A"));
        tmp_string.push(String::valueOf(static_cast<int64_t>(screenOnTime / 1000)));
    #ifdef PHONE_ENABLE
        tmp_string.push(String::valueOf(static_cast<int64_t>(phoneOnTime / 1000)));
    #endif  // PHONE_ENABLE
    #ifdef WIFI_ENABLE
        tmp_string.push(String::valueOf(static_cast<int64_t>(wifiOnTime / 1000)));
        tmp_string.push(String::valueOf(static_cast<int64_t>(wifiRunningTime / 1000)));
    #endif  // WIFI_ENABLE
    #ifndef BT_DISABLE
        tmp_string.push(String::valueOf(static_cast<int64_t>(bluetoothOnTime / 1000)));
    #endif  // BT_DISABLE
        tmp_string.push(String::valueOf(static_cast<int64_t>(rxTotal)));
        tmp_string.push(String::valueOf(static_cast<int64_t>(txTotal)));
        tmp_string.push(String::valueOf(static_cast<int64_t>(fullWakeLockTimeTotal)));
        tmp_string.push(String::valueOf(static_cast<int64_t>(partialWakeLockTimeTotal)));
        tmp_string.push(String::valueOf(static_cast<int64_t>(getInputEventCount(which))));
        dumpLine(pw, 0 /* uid */, category, MISC_DATA(), tmp_string);

        // Dump screen brightness stats
        // Object[] args = new Object[NUM_SCREEN_BRIGHTNESS_BINS];
        sp<Blob<int64_t> > args = new Blob<int64_t>(NUM_SCREEN_BRIGHTNESS_BINS);

    #ifdef BACKLIGHT_ENABLE
        for (int32_t i = 0; i < NUM_SCREEN_BRIGHTNESS_BINS; i++) {
            (*args)[i] = getScreenBrightnessTime(i, batteryRealtime, which) / 1000;
        }

        tmp_string.clear();

        for (int32_t i = 0; i < NUM_SCREEN_BRIGHTNESS_BINS; i++) {
            tmp_string.push(String::valueOf(static_cast<int64_t>(args[i])));
        }

        dumpLine(pw, 0 /* uid */, category, SCREEN_BRIGHTNESS_DATA(), tmp_string);
    #endif   // BACKLIGHT_ENABLE

    #ifdef PHONE_ENABLE
        // Dump signal strength stats
        // args = new Object[SignalStrength::NUM_SIGNAL_STRENGTH_BINS];
        args = new Blob<int64_t>(SignalStrength::NUM_SIGNAL_STRENGTH_BINS);

        for (int32_t i = 0; i < SignalStrength::NUM_SIGNAL_STRENGTH_BINS; i++) {
            (*args)[i] = getPhoneSignalStrengthTime(i, batteryRealtime, which) / 1000;
        }

        tmp_string.clear();

        for (int32_t i = 0; i < SignalStrength::NUM_SIGNAL_STRENGTH_BINS; i++) {
            tmp_string.push(String::valueOf(static_cast<int64_t>(args[i])));
        }

        dumpLine(pw, 0 /* uid */, category, SIGNAL_STRENGTH_TIME_DATA(), tmp_string);

        tmp_string.clear();
        tmp_string.push(String::valueOf(static_cast<int64_t>(getPhoneSignalScanningTime(batteryRealtime, which) / 1000)));
        dumpLine(pw, 0 /* uid */, category, SIGNAL_SCANNING_TIME_DATA(), tmp_string);

        for (int32_t i = 0; i < SignalStrength::NUM_SIGNAL_STRENGTH_BINS; i++) {
            (*args)[i] = getPhoneSignalStrengthCount(i, which);
        }

        tmp_string.clear();

        for (int32_t i = 0; i < SignalStrength::NUM_SIGNAL_STRENGTH_BINS; i++) {
            tmp_string.push(String::valueOf(static_cast<int64_t>(args[i])));
        }

        dumpLine(pw, 0 /* uid */, category, SIGNAL_STRENGTH_COUNT_DATA(), tmp_string);

        // Dump network type stats
        // args = new Object[NUM_DATA_CONNECTION_TYPES];
        args = new Blob<int64_t>(NUM_DATA_CONNECTION_TYPES);

        for (int32_t i = 0; i < NUM_DATA_CONNECTION_TYPES; i++) {
            (*args)[i] = getPhoneDataConnectionTime(i, batteryRealtime, which) / 1000;
        }

        tmp_string.clear();

        for (int32_t i = 0; i < NUM_DATA_CONNECTION_TYPES; i++) {
            tmp_string.push(String::valueOf(static_cast<int64_t>(args[i])));
        }

        dumpLine(pw, 0 /* uid */, category, DATA_CONNECTION_TIME_DATA(), tmp_string);

        for (int32_t i = 0; i < NUM_DATA_CONNECTION_TYPES; i++) {
            (*args)[i] = getPhoneDataConnectionCount(i, which);
        }

        tmp_string.clear();

        for (int32_t i = 0; i < NUM_DATA_CONNECTION_TYPES; i++) {
            tmp_string.push(String::valueOf(static_cast<int64_t>(args[i])));
        }

        dumpLine(pw, 0 /* uid */, category, DATA_CONNECTION_COUNT_DATA(), tmp_string);
    #endif  // PHONE_ENABLE
        if (which == STATS_SINCE_UNPLUGGED) {
            tmp_string.clear();
            tmp_string.push(String::valueOf(static_cast<int32_t>(getDischargeStartLevel())));
            tmp_string.push(String::valueOf(static_cast<int32_t>(getDischargeCurrentLevel())));
            dumpLine(pw, 0 /* uid */, category, BATTERY_LEVEL_DATA(), tmp_string);
        }

        if (which == STATS_SINCE_UNPLUGGED) {
            tmp_string.clear();
            tmp_string.push(String::valueOf(static_cast<int32_t>(getDischargeStartLevel() - getDischargeCurrentLevel())));
            tmp_string.push(String::valueOf(static_cast<int32_t>(getDischargeStartLevel() - getDischargeCurrentLevel())));
            tmp_string.push(String::valueOf(static_cast<int32_t>(getDischargeAmountScreenOn())));
            tmp_string.push(String::valueOf(static_cast<int32_t>(getDischargeAmountScreenOff())));
            dumpLine(pw, 0 /* uid */, category, BATTERY_DISCHARGE_DATA(), tmp_string);
        } else {
            tmp_string.clear();
            tmp_string.push(String::valueOf(static_cast<int32_t>(getLowDischargeAmountSinceCharge())));
            tmp_string.push(String::valueOf(static_cast<int32_t>(getHighDischargeAmountSinceCharge())));
            tmp_string.push(String::valueOf(static_cast<int32_t>(getDischargeAmountScreenOn())));
            tmp_string.push(String::valueOf(static_cast<int32_t>(getDischargeAmountScreenOff())));
            dumpLine(pw, 0 /* uid */, category, BATTERY_DISCHARGE_DATA(), tmp_string);
        }

        if (reqUid < 0) {
            // Map < String, ? extends BatteryStats.Timer > kernelWakelocks = getKernelWakelockStats();
            KeyedVector<sp<String>, sp<BatteryStats::Timer> > kernelWakelocks = getKernelWakelockStats();

            if (kernelWakelocks.size() > 0) {
                const int32_t KWL = kernelWakelocks.size();
                for (int32_t i = 0; i < KWL; i++) {
                    sb->setLength(0);
                    printWakeLockCheckin(sb, kernelWakelocks.valueAt(i), batteryRealtime, NULL, which, new String(""));

                    tmp_string.clear();
                    tmp_string.push(kernelWakelocks.keyAt(i));
                    tmp_string.push(sb->toString());
                    dumpLine(pw, 0 /* uid */, category, KERNEL_WAKELOCK_DATA(), tmp_string);
                }
            }
        }

        for (int32_t iu = 0; iu < NU; iu++) {
            const int32_t uid = uidStats.keyAt(iu);

            if (reqUid >= 0 && uid != reqUid) {
                continue;
            }

            sp<Uid> u = uidStats.valueAt(iu);
            // Dump Network stats per uid, if any
            int64_t rx = u->getTcpBytesReceived(which);
            int64_t tx = u->getTcpBytesSent(which);
    #ifdef WIFI_ENABLE
            int64_t fullWifiLockOnTime = u->getFullWifiLockTime(batteryRealtime, which);
            int64_t scanWifiLockOnTime = u->getScanWifiLockTime(batteryRealtime, which);
            int64_t uidWifiRunningTime = u->getWifiRunningTime(batteryRealtime, which);
    #endif  // WIFI_ENABLE
            if (rx > 0 || tx > 0) {
                tmp_string.clear();
                tmp_string.push(String::valueOf(static_cast<int32_t>(rx)));
                tmp_string.push(String::valueOf(static_cast<int32_t>(tx)));
                dumpLine(pw, uid, category, NETWORK_DATA(), tmp_string);
            }

    #ifdef WIFI_ENABLE
            if (fullWifiLockOnTime != 0 || scanWifiLockOnTime != 0
                    || uidWifiRunningTime != 0) {
                tmp_string.clear();
                tmp_string.push(String::valueOf(static_cast<int32_t>(fullWifiLockOnTime)));
                tmp_string.push(String::valueOf(static_cast<int32_t>(scanWifiLockOnTime)));
                tmp_string.push(String::valueOf(static_cast<int32_t>(uidWifiRunningTime)));
                dumpLine(pw, uid, category, WIFI_LOCK_DATA(), tmp_string);
            }
    #endif  // WIFI_ENABLE
            if (u->hasUserActivity()) {
                // args = new Object[Uid.NUM_USER_ACTIVITY_TYPES];
                args = new Blob<int64_t>(Uid::NUM_USER_ACTIVITY_TYPES);
                bool hasData = false;

                for (int32_t i = 0; i < Uid::NUM_USER_ACTIVITY_TYPES; i++) {
                    int32_t val = u->getUserActivityCount(i, which);
                    args[i] = val;

                    if (val != 0) hasData = true;
                }

                if (hasData) {
                    tmp_string.clear();

                    for (int32_t i = 0; i < Uid::NUM_USER_ACTIVITY_TYPES; i++) {
                        tmp_string.push(String::valueOf(static_cast<int64_t>(args[i])));
                    }

                    dumpLine(pw, 0 /* uid */, category, USER_ACTIVITY_DATA(), tmp_string);
                }
            }

            // Map < String, ? extends BatteryStats.Uid.Wakelock > wakelocks = u->getWakelockStats();
            KeyedVector<sp<String>, sp<BatteryStats::Uid::Wakelock> > wakelocks = u->getWakelockStats();
            if (wakelocks.size() > 0) {
                const int32_t WL = wakelocks.size();
                for (int32_t i = 0; i < WL; i++) {
                    sp<Uid::Wakelock> wl = wakelocks.valueAt(i);
                    sp<String> linePrefix = new String("");
                    sb->setLength(0);
                    linePrefix = printWakeLockCheckin(sb, wl->getWakeTime(WAKE_TYPE_FULL),
                                                      batteryRealtime, new String("f"), which, linePrefix);
                    linePrefix = printWakeLockCheckin(sb, wl->getWakeTime(WAKE_TYPE_PARTIAL),
                                                      batteryRealtime, new String("p"), which, linePrefix);
                    linePrefix = printWakeLockCheckin(sb, wl->getWakeTime(WAKE_TYPE_WINDOW),
                                                      batteryRealtime, new String("w"), which, linePrefix);

                    // Only log if we had at lease one wakelock...
                    if (sb->length() > 0) {
                        tmp_string.clear();
                        tmp_string.push(wakelocks.keyAt(i));
                        tmp_string.push(sb->toString());
                        dumpLine(pw, uid, category, WAKELOCK_DATA(), tmp_string);
                    }
                }
            }

    #ifdef SENSOR_ENABLE
            // Map < Integer, ? extends BatteryStats.Uid.Sensor > sensors = u->getSensorStats();
            KeyedVector<int32_t, sp<BatteryStats::Uid::Sensor> > sensors = u->getSensorStats();

            if (sensors.size() > 0)  {
                const int32_t SS = sensors.size();
                for (int32_t i = 0; i < SS; i++) {
                    sp<Uid::Sensor> se = sensors.valueAt(i);
                    int32_t sensorNumber = sensors.keyAt(i);
                    sp<Timer> timer = se->getSensorTime();

                    if (timer != NULL) {
                        // Convert from microseconds to milliseconds with rounding
                        int64_t totalTime = (timer->getTotalTimeLocked(batteryRealtime, which) + 500) / 1000;
                        int32_t count = timer->getCountLocked(which);

                        if (totalTime != 0) {
                            tmp_string.clear();
                            tmp_string.push(String::valueOf(static_cast<int32_t>(sensorNumber)));
                            tmp_string.push(String::valueOf(static_cast<int64_t>(totalTime)));
                            tmp_string.push(String::valueOf(static_cast<int32_t>(count)));
                            dumpLine(pw, uid, category, SENSOR_DATA(), tmp_string);
                        }
                    }
                }
            }
    #endif  // SENSOR_ENABLE

            // Map < String, ? extends BatteryStats.Uid.Proc > processStats = u->getProcessStats();
            KeyedVector<sp<String>, sp<BatteryStats::Uid::Proc> > processStats = u->getProcessStats();
            if (processStats.size() > 0) {
                const int32_t PS = processStats.size();
                for (int32_t i = 0; i < PS; i++) {
                    sp<Uid::Proc> ps = processStats.valueAt(i);
                    int64_t userTime = ps->getUserTime(which);
                    int64_t systemTime = ps->getSystemTime(which);
                    int32_t starts = ps->getStarts(which);

                    if (userTime != 0 || systemTime != 0 || starts != 0) {
                        tmp_string.clear();
                        tmp_string.push(processStats.keyAt(i));  // proc
                        tmp_string.push(String::valueOf(static_cast<int64_t>(userTime * 10)));          // cpu time in ms
                        tmp_string.push(String::valueOf(static_cast<int64_t>(systemTime * 10)));        // user time in ms
                        tmp_string.push(String::valueOf(static_cast<int32_t>(starts)));                 // process starts
                        dumpLine(pw, uid, category, PROCESS_DATA(), tmp_string);
                    }
                }
            }

            // Map < String, ? extends BatteryStats.Uid.Pkg > packageStats = u->getPackageStats();
            KeyedVector<sp<String>, sp<BatteryStats::Uid::Pkg> > packageStats = u->getPackageStats();
            if (packageStats.size() > 0) {
                const int32_t PS = packageStats.size();
                for (int32_t i = 0; i < PS; i++) {
                    sp<Uid::Pkg> ps = packageStats.valueAt(i);
                    int32_t wakeups = ps->getWakeups(which);
                    // Map < String, ? extends  Uid.Pkg.Serv > serviceStats = ps.getServiceStats();
                    KeyedVector<sp<String>, sp<BatteryStats::Uid::Pkg::Serv> > serviceStats = ps->getServiceStats();

                    const int32_t SS = serviceStats.size();
                    for (int32_t j = 0; j < SS; j++) {
                        sp<BatteryStats::Uid::Pkg::Serv> ss = serviceStats.valueAt(j);
                        int64_t startTime = ss->getStartTime(batteryUptime, which);
                        int32_t starts = ss->getStarts(which);
                        int32_t launches = ss->getLaunches(which);

                        if (startTime != 0 || starts != 0 || launches != 0) {
                            tmp_string.clear();
                            tmp_string.push(String::valueOf(static_cast<int32_t>(wakeups)));
                            tmp_string.push(packageStats.keyAt(i));
                            tmp_string.push(serviceStats.keyAt(j));
                            tmp_string.push(String::valueOf(static_cast<int32_t>(startTime / 1000)));
                            tmp_string.push(String::valueOf(static_cast<int32_t>(starts)));
                            tmp_string.push(String::valueOf(static_cast<int32_t>(launches)));
                            dumpLine(pw, uid, category, APK_DATA(), tmp_string);
                        }
                    }
                }
            }
        }
    }
}

void BatteryStats::dumpLocked(int32_t fd, sp<PrintWriter>& pw, const sp<String>& prefix, int32_t which, int32_t reqUid) {
    GLOGENTRY();

    if (fd != -1) {
        sp<StringWriter> sw = new StringWriter();
        sp<PrintWriter> pw_ = new PrintWriter(sp<Writer>(sw.get()));

        if (pw_ == NULL) {
            GLOGW("pw_ = NULL");
            return;
        }

        const int64_t rawUptime = uptimeMillis() * 1000;
        const int64_t rawRealtime = elapsedRealtime() * 1000;
        const int64_t batteryUptime = getBatteryUptime(rawUptime);
        const int64_t batteryRealtime = getBatteryRealtime(rawRealtime);
        const int64_t whichBatteryUptime = computeBatteryUptime(rawUptime, which);
        const int64_t whichBatteryRealtime = computeBatteryRealtime(rawRealtime, which);
        const int64_t totalRealtime = computeRealtime(rawRealtime, which);
        const int64_t totalUptime = computeUptime(rawUptime, which);

        sp<StringBuilder> sb = new StringBuilder(128);
        KeyedVector<int32_t, sp<BatteryStats::Uid> > uidStats = getUidStats();
        const int32_t NU = uidStats.size();
        sb->setLength(0);
        sb->append(prefix);
        sb->append(String("  Time on battery: "));
        formatTimeMs(sb, whichBatteryRealtime / 1000);
        sb->append("(");
        sb->append(formatRatioLocked(whichBatteryRealtime, totalRealtime));
        sb->append(") realtime, ");
        formatTimeMs(sb, whichBatteryUptime / 1000);
        sb->append("(");
        sb->append(formatRatioLocked(whichBatteryUptime, totalRealtime));
        sb->append(") uptime");
        pw_->println(sb->toString());
        sb->setLength(0);
        sb->append(prefix);
        sb->append("  Total run time: ");
        formatTimeMs(sb, totalRealtime / 1000);
        sb->append("realtime, ");
        formatTimeMs(sb, totalUptime / 1000);
        sb->append("uptime, ");
        pw_->println(sb->toString());
        const int64_t screenOnTime = getScreenOnTime(batteryRealtime, which);
    #ifdef PHONE_ENABLE
        const int64_t phoneOnTime = getPhoneOnTime(batteryRealtime, which);
    #endif  // PHONE_ENABLE

    #ifdef WIFI_ENABLE
        const int64_t wifiRunningTime = getGlobalWifiRunningTime(batteryRealtime, which);
        const int64_t wifiOnTime = getWifiOnTime(batteryRealtime, which);
    #endif  // WIFI_ENABLE

    #ifndef BT_DISABLE
        const int64_t bluetoothOnTime = getBluetoothOnTime(batteryRealtime, which);
    #endif  // BT_DISABLE
        sb = new StringBuilder(128);
        sb->setLength(0);
        sb->append(prefix);
        sb->append("  Screen on: ");
        formatTimeMs(sb, screenOnTime / 1000);
        sb->append("(");
        sb->append(formatRatioLocked(screenOnTime, whichBatteryRealtime));
        sb->append("), Input events: ");
        sb->append(getInputEventCount(which));
    #ifdef PHONE_ENABLE
        sb->append(", Active phone call: ");
        formatTimeMs(sb, phoneOnTime / 1000);
        sb->append("(");
        sb->append(formatRatioLocked(phoneOnTime, whichBatteryRealtime));
        sb->append(")");
    #endif  // PHONE_ENABLE
        pw_->println(sb->toString());
    #ifdef BACKLIGHT_ENABLE
        sb = new StringBuilder(128);
        sb->setLength(0);
        sb->append(prefix);
        sb->append("  Screen brightnesses: ");
        bool didOne = false;

        for (int32_t i = 0; i < NUM_SCREEN_BRIGHTNESS_BINS; i++) {
            const int64_t time = getScreenBrightnessTime(i, batteryRealtime, which);

            if (time == 0) {
                continue;
            }

            if (didOne) sb->append(", ");

            didOne = true;
            sb->append((*SCREEN_BRIGHTNESS_NAMES())[i]);
            sb->append(" ");
            formatTimeMs(sb, time / 1000);
            sb->append("(");
            sb->append(formatRatioLocked(time, screenOnTime));
            sb->append(")");
        }

        if (!didOne) sb->append("No activity");
    #endif  // BACKLIGHT_ENABLE

        pw_->println(sb->toString());

        if (sw != NULL && sw->toString() != NULL) {
            write(fd, sw->toString()->string(), sw->toString()->size());
            sw = new StringWriter();
            pw_ = new PrintWriter(sp<Writer>(sw.get()));
        }

        sb = new StringBuilder(128);
        sb->setLength(0);

        // Calculate total network and wakelock times across all uids.
        int64_t rxTotal = 0;
        int64_t txTotal = 0;
        int64_t fullWakeLockTimeTotalMicros = 0;
        int64_t partialWakeLockTimeTotalMicros = 0;

        if (reqUid < 0) {
            KeyedVector<sp<String>, sp<BatteryStats::Timer> > kernelWakelocks = getKernelWakelockStats();
            // Map < String, ? extends BatteryStats.Timer > kernelWakelocks = getKernelWakelockStats();
            const int32_t KWL = kernelWakelocks.size();
            if (KWL > 0) {
                for (int32_t i = 0; i < KWL; i++) {
                    sp<String> linePrefix = new String(": ");
                    sb->setLength(0);
                    sb->append(prefix);
                    sb->append("  Kernel Wake lock ");
                    sb->append(kernelWakelocks.keyAt(i));
                    linePrefix = printWakeLock(sb, kernelWakelocks.valueAt(i), batteryRealtime, NULL, which,
                                               linePrefix);

                    if (!linePrefix->equals(": ")) {
                        sb->append(" realtime");
                        // Only print out wake locks that were held
                        pw_->println(sb->toString());
                    }

                    if ((i % 10) == 0) {
                        if (sw != NULL && sw->toString() != NULL) {
                            write(fd, sw->toString()->string(), sw->toString()->size());
                            sw = new StringWriter();
                            pw_ = new PrintWriter(sp<Writer>(sw.get()));
                        }
                    }
                }
            }
        }

        for (int32_t iu = 0; iu < NU; iu++) {
            sp<Uid> u = uidStats.valueAt(iu);
            rxTotal += u->getTcpBytesReceived(which);
            txTotal += u->getTcpBytesSent(which);
            KeyedVector<sp<String>, sp<BatteryStats::Uid::Wakelock> > wakelocks = u->getWakelockStats();

            if (wakelocks.size()> 0) {
                const int32_t WL = wakelocks.size();

                for (int32_t i = 0; i < WL; i++) {
                    sp<Uid::Wakelock> wl = wakelocks.valueAt(i);
                    sp<Timer> fullWakeTimer = wl->getWakeTime(WAKE_TYPE_FULL);

                    if (fullWakeTimer != NULL) {
                        fullWakeLockTimeTotalMicros += fullWakeTimer->getTotalTimeLocked(
                                                           batteryRealtime, which);
                    }

                    sp<Timer> partialWakeTimer = wl->getWakeTime(WAKE_TYPE_PARTIAL);

                    if (partialWakeTimer != NULL) {
                        partialWakeLockTimeTotalMicros += partialWakeTimer->getTotalTimeLocked(
                                                              batteryRealtime, which);
                    }
                }
            }
        }

        pw_->print(prefix);
        pw_->print("  Total received: ");
        pw_->print(formatBytesLocked(rxTotal));
        pw_->print(", Total sent: ");
        pw_->println(formatBytesLocked(txTotal));
        sb->setLength(0);
        sb->append(prefix);
        sb->append("  Total full wakelock time: ");
        formatTimeMs(sb,
                     (fullWakeLockTimeTotalMicros + 500) / 1000);
        sb->append(", Total partial waklock time: ");
        formatTimeMs(sb,
                     (partialWakeLockTimeTotalMicros + 500) / 1000);
        pw_->println(sb->toString());

        if (sw != NULL && sw->toString() != NULL) {
            write(fd, sw->toString()->string(), sw->toString()->size());
            sw = new StringWriter();
            pw_ = new PrintWriter(sp<Writer>(sw.get()));
        }

        sb->setLength(0);
        sb->append(prefix);

    #ifdef PHONE_ENABLE
        sb->append("  Signal levels: ");
        didOne = false;

        for (int32_t i = 0; i < SignalStrength::NUM_SIGNAL_STRENGTH_BINS; i++) {
            const int64_t time = getPhoneSignalStrengthTime(i, batteryRealtime, which);

            if (time == 0) {
                continue;
            }

            if (didOne) sb->append(", ");

            didOne = true;
            sb->append((*BatteryStats::SIGNAL_STRENGTH_NAMES())[i]);
            sb->append(" ");
            formatTimeMs(sb, time / 1000);
            sb->append("(");
            sb->append(formatRatioLocked(time, whichBatteryRealtime));
            sb->append(") ");
            sb->append(getPhoneSignalStrengthCount(i, which));
            sb->append("x");
        }

        if (!didOne) {
            sb->append("No activity");
        }

        pw_->println(sb->toString());
        sb->setLength(0);
        sb->append(prefix);
        sb->append("  Signal scanning time: ");
        formatTimeMs(sb, getPhoneSignalScanningTime(batteryRealtime, which) / 1000);
        pw_->println(sb->toString());

        if (sw != NULL && sw->toString() != NULL) {
            write(fd, sw->toString()->string(), sw->toString()->size());
            sw = new StringWriter();
            pw_ = new PrintWriter(sp<Writer>(sw.get()));
        }

        sb->setLength(0);
        sb->append(prefix);
        sb->append("  Radio types: ");
        didOne = false;

        for (int32_t i = 0; i < NUM_DATA_CONNECTION_TYPES; i++) {
            const int64_t time = getPhoneDataConnectionTime(i, batteryRealtime, which);

            if (time == 0) {
                continue;
            }

            if (didOne) sb->append(", ");

            didOne = true;
            sb->append((*DATA_CONNECTION_NAMES())[i]);
            sb->append(" ");
            formatTimeMs(sb, time / 1000);
            sb->append("(");
            sb->append(formatRatioLocked(time, whichBatteryRealtime));
            sb->append(") ");
            sb->append(getPhoneDataConnectionCount(i, which));
            sb->append("x");
        }

        if (!didOne) {
            sb->append("No activity");
        }
    #endif  // PHONE_ENABLE

        pw_->println(sb->toString());
        sb->setLength(0);
        sb->append(prefix);
        sb->append("  Radio data uptime when unplugged: ");
        sb->append(getRadioDataUptime() / 1000);
        sb->append(" ms");
        pw_->println(sb->toString());

        if (sw != NULL && sw->toString() != NULL) {
            write(fd, sw->toString()->string(), sw->toString()->size());
            sw = new StringWriter();
            pw_ = new PrintWriter(sp<Writer>(sw.get()));
        }

        sb->setLength(0);
        sb->append(prefix);
    #ifdef WIFI_ENABLE
        sb->append("  Wifi on: ");
        formatTimeMs(sb, wifiOnTime / 1000);
        sb->append("(");
        sb->append(formatRatioLocked(wifiOnTime, whichBatteryRealtime));
        sb->append("), Wifi running: ");
        formatTimeMs(sb, wifiRunningTime / 1000);
        sb->append("(");
        sb->append(formatRatioLocked(wifiRunningTime, whichBatteryRealtime));
    #endif  // WIFI_ENABLE
    #ifndef BT_DISABLE
        sb->append("), Bluetooth on: ");
        formatTimeMs(sb, bluetoothOnTime / 1000);
        sb->append("(");
        sb->append(formatRatioLocked(bluetoothOnTime, whichBatteryRealtime));
        sb->append(")");
        pw_->println(sb->toString());
        pw_->println(" ");
    #endif  // BT_DISABLE

        if (which == STATS_SINCE_UNPLUGGED) {
            if (getIsOnBattery()) {
                pw_->print(prefix);
                pw_->println("  Device is currently unplugged");
                pw_->print(prefix);
                pw_->print("    Discharge cycle start level: ");
                pw_->println(getDischargeStartLevel());
                pw_->print(prefix);
                pw_->print("    Discharge cycle current level: ");
                pw_->println(getDischargeCurrentLevel());
            } else {
                pw_->print(prefix);
                pw_->println("  Device is currently plugged into power");
                pw_->print(prefix);
                pw_->print("    Last discharge cycle start level: ");
                pw_->println(getDischargeStartLevel());
                pw_->print(prefix);
                pw_->print("    Last discharge cycle end level: ");
                pw_->println(getDischargeCurrentLevel());
            }

            pw_->print(prefix);
            pw_->print("    Amount discharged while screen on: ");
            pw_->println(getDischargeAmountScreenOn());
            pw_->print(prefix);
            pw_->print("    Amount discharged while screen off: ");
            pw_->println(getDischargeAmountScreenOff());
            pw_->println(" ");
        } else {
            pw_->print(prefix);
            pw_->println("  Device battery use since last full charge");
            pw_->print(prefix);
            pw_->print("    Amount discharged (lower bound): ");
            pw_->println(getLowDischargeAmountSinceCharge());
            pw_->print(prefix);
            pw_->print("    Amount discharged (upper bound): ");
            pw_->println(getHighDischargeAmountSinceCharge());
            pw_->print(prefix);
            pw_->print("    Amount discharged while screen on: ");
            pw_->println(getDischargeAmountScreenOnSinceCharge());
            pw_->print(prefix);
            pw_->print("    Amount discharged while screen off: ");
            pw_->println(getDischargeAmountScreenOffSinceCharge());
            pw_->println(" ");
        }

        if (sw != NULL && sw->toString() != NULL) {
            write(fd, sw->toString()->string(), sw->toString()->size());
            sw = new StringWriter();
            pw_ = new PrintWriter(sp<Writer>(sw.get()));
        }
#if 1
        for (int32_t iu = 0; iu < NU; iu++) {
            const int32_t uid = uidStats.keyAt(iu);

            if (reqUid >= 0 && uid != reqUid && uid != Process::SYSTEM_UID) {
                continue;
            }

            sp<Uid> u = uidStats.valueAt(iu);
            pw_->println(prefix + "  #" + uid + ":");
            bool uidActivity = false;
            int64_t tcpReceived = u->getTcpBytesReceived(which);
            int64_t tcpSent = u->getTcpBytesSent(which);
    #ifdef WIFI_ENABLE
            int64_t fullWifiLockOnTime = u->getFullWifiLockTime(batteryRealtime, which);
            int64_t scanWifiLockOnTime = u->getScanWifiLockTime(batteryRealtime, which);
            int64_t uidWifiRunningTime = u->getWifiRunningTime(batteryRealtime, which);
    #endif  // WIFI_ENABLE
            if (tcpReceived != 0 || tcpSent != 0) {
                pw_->print(prefix);
                pw_->print("    Network: ");
                pw_->print(formatBytesLocked(tcpReceived));
                pw_->print(" received, ");
                pw_->print(formatBytesLocked(tcpSent));
                pw_->println(" sent");
            }

            if (u->hasUserActivity()) {
                bool hasData = false;

                for (int32_t i = 0; i < NUM_SCREEN_BRIGHTNESS_BINS; i++) {
                    int32_t val = u->getUserActivityCount(i, which);

                    if (val != 0) {
                        if (!hasData) {
                            sb->setLength(0);
                            sb->append("    User activity: ");
                            hasData = true;
                        } else {
                            sb->append(", ");
                        }

                        sb->append(val);
                        sb->append(" ");
                        sb->append((*BatteryStats::Uid::USER_ACTIVITY_TYPES())[i]);
                    }
                }

                if (hasData) {
                    pw_->println(sb->toString());
                }
            }

    #ifdef WIFI_ENABLE
            if (fullWifiLockOnTime != 0 || scanWifiLockOnTime != 0
                    || uidWifiRunningTime != 0) {
                sb->setLength(0);
                sb->append(prefix);
                sb->append("    Wifi Running: ");
                formatTimeMs(sb, uidWifiRunningTime / 1000);
                sb->append("(");
                sb->append(formatRatioLocked(uidWifiRunningTime,
                                            whichBatteryRealtime));
                sb->append(")\n");
                sb->append(prefix);
                sb->append("    Full Wifi Lock: ");
                formatTimeMs(sb, fullWifiLockOnTime / 1000);
                sb->append("(");
                sb->append(formatRatioLocked(fullWifiLockOnTime,
                                            whichBatteryRealtime));
                sb->append(")\n");
                sb->append(prefix);
                sb->append("    Scan Wifi Lock: ");
                formatTimeMs(sb, scanWifiLockOnTime / 1000);
                sb->append("(");
                sb->append(formatRatioLocked(scanWifiLockOnTime,
                                            whichBatteryRealtime));
                sb->append(")");
                pw_->println(sb->toString());
            }
            #endif  // WIFI_ENABLE

            // Map < String, ? extends BatteryStats.Uid.Wakelock > wakelocks = u->getWakelockStats();
            KeyedVector<sp<String> , sp<BatteryStats::Uid::Wakelock> > wakelocks = u->getWakelockStats();



            if (wakelocks.size() > 0) {
                int64_t totalFull = 0, totalPartial = 0, totalWindow = 0;
                int32_t count = 0;
                const int32_t WL = wakelocks.size();

                for (int32_t i = 0; i < WL; i++) {
                    sp<Uid::Wakelock> wl = wakelocks.valueAt(i);

                    sp<String> linePrefix = new String(": ");
                    sb->setLength(0);
                    sb->append(prefix);
                    sb->append("    Wake lock ");
                    sb->append(wakelocks.keyAt(i));
                    linePrefix = printWakeLock(sb, wl->getWakeTime(WAKE_TYPE_FULL), batteryRealtime,
                                               new String("full"), which, linePrefix);
                    linePrefix = printWakeLock(sb, wl->getWakeTime(WAKE_TYPE_PARTIAL), batteryRealtime,
                                               new String("partial"), which, linePrefix);
                    linePrefix = printWakeLock(sb, wl->getWakeTime(WAKE_TYPE_WINDOW), batteryRealtime,
                                               new String("window"), which, linePrefix);

                    if (!linePrefix->equals(": ")) {
                        sb->append(" realtime");
                        // Only print out wake locks that were held
                        pw_->println(sb->toString());
                        uidActivity = true;
                        count++;
                    }

                    totalFull += computeWakeLock(wl->getWakeTime(WAKE_TYPE_FULL),
                                                 batteryRealtime, which);
                    totalPartial += computeWakeLock(wl->getWakeTime(WAKE_TYPE_PARTIAL),
                                                    batteryRealtime, which);
                    totalWindow += computeWakeLock(wl->getWakeTime(WAKE_TYPE_WINDOW),
                                                   batteryRealtime, which);
                }

                if (count > 1) {
                    if (totalFull != 0 || totalPartial != 0 || totalWindow != 0) {
                        sb->setLength(0);
                        sb->append(prefix);
                        sb->append("    TOTAL wake: ");
                        bool needComma = false;

                        if (totalFull != 0) {
                            needComma = true;
                            formatTimeMs(sb, totalFull);
                            sb->append("full");
                        }

                        if (totalPartial != 0) {
                            if (needComma) {
                                sb->append(", ");
                            }

                            needComma = true;
                            formatTimeMs(sb, totalPartial);
                            sb->append("partial");
                        }

                        if (totalWindow != 0) {
                            if (needComma) {
                                sb->append(", ");
                            }

                            needComma = true;
                            formatTimeMs(sb, totalWindow);
                            sb->append("window");
                        }

                        sb->append(" realtime");
                        pw_->println(sb->toString());
                    }
                }
            }

    #ifdef SENSOR_ENABLE
            // Map < Integer, ? extends BatteryStats.Uid.Sensor > sensors = u->getSensorStats();
            KeyedVector<int32_t, sp<BatteryStats::Uid::Sensor> > sensors = u->getSensorStats();

            if (sensors.size() > 0) {
                const int32_t WL = sensors.size();
                for (int32_t i = 0; i < WL; i++) {
                    sp<Uid::Sensor> se = sensors.valueAt(i);
                    int32_t sensorNumber = sensors.keyAt(i);
                    sb->setLength(0);
                    sb->append(prefix);
                    sb->append("    Sensor ");
                    int32_t handle = se->getHandle();

                    if (handle == Uid::Sensor::GPS) {
                        sb->append("GPS");
                    } else {
                        sb->append(handle);
                    }

                    sb->append(": ");
                    sp<Timer> timer = se->getSensorTime();

                    if (timer != NULL) {
                        // Convert from microseconds to milliseconds with rounding
                        int64_t totalTime = (timer->getTotalTimeLocked(
                                              batteryRealtime, which) + 500) / 1000;
                        int32_t count = timer->getCountLocked(which);

                        // timer.logState();
                        if (totalTime != 0) {
                            formatTimeMs(sb, totalTime);
                            sb->append("realtime (");
                            sb->append(count);
                            sb->append(" times)");
                        } else {
                            sb->append("(not used)");
                        }
                    } else {
                        sb->append("(not used)");
                    }

                    pw_->println(sb->toString());
                    uidActivity = true;
                }
            }
    #endif  // SENSOR_ENABLE

            // Map < String, ? extends BatteryStats.Uid.Proc > processStats = u->getProcessStats();
            KeyedVector<sp<String> , sp<BatteryStats::Uid::Proc> > processStats = u->getProcessStats();

            if (processStats.size() > 0) {
                const int32_t PS = processStats.size();

                for (int32_t i = 0; i < PS; i++) {
                    sp<Uid::Proc> ps = processStats.valueAt(i);
                    int64_t userTime;
                    int64_t systemTime;
                    int32_t starts;
                    int32_t numExcessive;
                    userTime = ps->getUserTime(which);
                    systemTime = ps->getSystemTime(which);
                    starts = ps->getStarts(which);
                    numExcessive = which == STATS_SINCE_CHARGED
                                   ? ps->countExcessivePowers() : 0;

                    if (userTime != 0 || systemTime != 0 || starts != 0
                            || numExcessive != 0) {
                        sb->setLength(0);
                        sb->append(prefix);
                        sb->append("    Proc ");
                        sb->append(processStats.keyAt(i));
                        sb->append(":\n");
                        sb->append(prefix);
                        sb->append("      CPU: ");
                        formatTime(sb, userTime);
                        sb->append("usr + ");
                        formatTime(sb, systemTime);
                        sb->append("krn");

                        if (starts != 0) {
                            sb->append("\n");
                            sb->append(prefix);
                            sb->append("      ");
                            sb->append(starts);
                            sb->append(" proc starts");
                        }

                        pw_->println(sb->toString());

                        for (int32_t e = 0; e < numExcessive; e++) {
                            sp<Uid::Proc::ExcessivePower> ew = ps->getExcessivePower(e);

                            if (ew != NULL) {
                                pw_->print(prefix);
                                pw_->print("      * Killed for ");

                                if (ew->type == Uid::Proc::ExcessivePower::TYPE_WAKE) {
                                    pw_->print("wake lock");
                                } else if (ew->type == Uid::Proc::ExcessivePower::TYPE_CPU) {
                                    pw_->print("cpu");
                                } else {
                                    pw_->print("unknown");
                                }

                                pw_->print(" use: ");
                                TimeUtils::formatDuration(ew->usedTime, pw_);
                                pw_->print(" over ");
                                TimeUtils::formatDuration(ew->overTime, pw_);
                                pw_->print(" (");
                                pw_->print((ew->usedTime*100) / ew->overTime);
                                pw_->println("%)");
                            }
                        }

                        uidActivity = true;
                    }
                }
            }

            // Map < String, ? extends BatteryStats.Uid.Pkg > packageStats = u->getPackageStats();
            KeyedVector<sp<String> , sp<BatteryStats::Uid::Pkg> > packageStats = u->getPackageStats();

            if (packageStats.size() > 0) {
                const int32_t PS = packageStats.size();
                for (int32_t i = 0; i < PS; i++) {
                    pw_->print(prefix);
                    pw_->print("    Apk ");
                    pw_->print(packageStats.keyAt(i));
                    pw_->println(":");
                    bool apkActivity = false;
                    sp<Uid::Pkg> ps = packageStats.valueAt(i);
                    int32_t wakeups = ps->getWakeups(which);

                    if (wakeups != 0) {
                        pw_->print(prefix);
                        pw_->print("      ");
                        pw_->print(wakeups);
                        pw_->println(" wakeup alarms");
                        apkActivity = true;
                    }

                    // Map < String, ? extends  Uid.Pkg.Serv > serviceStats = ps->getServiceStats();
                    KeyedVector<sp<String> , sp<BatteryStats::Uid::Pkg::Serv> >  serviceStats = ps->getServiceStats();

                    if (serviceStats.size() > 0) {
                        const int32_t SS = serviceStats.size();

                        for (int32_t i = 0; i < SS; i++) {
                            sp<BatteryStats::Uid::Pkg::Serv> ss = serviceStats.valueAt(i);
                            int64_t startTime = ss->getStartTime(batteryUptime, which);
                            int32_t starts = ss->getStarts(which);
                            int32_t launches = ss->getLaunches(which);

                            if (startTime != 0 || starts != 0 || launches != 0) {
                                sb->setLength(0);
                                sb->append(prefix);
                                sb->append("      Service ");
                                sb->append(serviceStats.keyAt(i));
                                sb->append(":\n");
                                sb->append(prefix);
                                sb->append("        Created for: ");
                                formatTimeMs(sb, startTime / 1000);
                                sb->append(" uptime\n");
                                sb->append(prefix);
                                sb->append("        Starts: ");
                                sb->append(starts);
                                sb->append(", launches: ");
                                sb->append(launches);
                                pw_->println(sb->toString());
                                apkActivity = true;
                            }
                        }
                    }

                    if (!apkActivity) {
                        pw_->print(prefix);
                        pw_->println("      (nothing executed)");
                    }

                    uidActivity = true;
                }
            }

            if (!uidActivity) {
                pw_->print(prefix);
                pw_->println("    (nothing executed)");
            }

            if (sw != NULL && sw->toString() != NULL) {
                write(fd, sw->toString()->string(), sw->toString()->size());
                sw = new StringWriter();
                pw_ = new PrintWriter(sp<Writer>(sw.get()));
            }
        }
#endif
    } else {
        const int64_t rawUptime = uptimeMillis() * 1000;
        const int64_t rawRealtime = elapsedRealtime() * 1000;
        const int64_t batteryUptime = getBatteryUptime(rawUptime);
        const int64_t batteryRealtime = getBatteryRealtime(rawRealtime);
        const int64_t whichBatteryUptime = computeBatteryUptime(rawUptime, which);
        const int64_t whichBatteryRealtime = computeBatteryRealtime(rawRealtime, which);
        const int64_t totalRealtime = computeRealtime(rawRealtime, which);
        const int64_t totalUptime = computeUptime(rawUptime, which);

        sp<StringBuilder> sb = new StringBuilder(128);
        KeyedVector<int32_t, sp<BatteryStats::Uid> > uidStats = getUidStats();
        const int32_t NU = uidStats.size();
        sb->setLength(0);
        sb->append(prefix);
        sb->append(String("  Time on battery: "));
        formatTimeMs(sb, whichBatteryRealtime / 1000);
        sb->append("(");
        sb->append(formatRatioLocked(whichBatteryRealtime, totalRealtime));
        sb->append(") realtime, ");
        formatTimeMs(sb, whichBatteryUptime / 1000);
        sb->append("(");
        sb->append(formatRatioLocked(whichBatteryUptime, totalRealtime));
        sb->append(") uptime");
        pw->println(sb->toString());
        sb->setLength(0);
        sb->append(prefix);
        sb->append("  Total run time: ");
        formatTimeMs(sb, totalRealtime / 1000);
        sb->append("realtime, ");
        formatTimeMs(sb, totalUptime / 1000);
        sb->append("uptime, ");
        pw->println(sb->toString());
        const int64_t screenOnTime = getScreenOnTime(batteryRealtime, which);
    #ifdef PHONE_ENABLE
        const int64_t phoneOnTime = getPhoneOnTime(batteryRealtime, which);
    #endif  // PHONE_ENABLE

    #ifdef WIFI_ENABLE
        const int64_t wifiRunningTime = getGlobalWifiRunningTime(batteryRealtime, which);
        const int64_t wifiOnTime = getWifiOnTime(batteryRealtime, which);
    #endif  // WIFI_ENABLE

    #ifndef BT_DISABLE
        const int64_t bluetoothOnTime = getBluetoothOnTime(batteryRealtime, which);
    #endif  // BT_DISABLE
        sb = new StringBuilder(128);
        sb->setLength(0);
        sb->append(prefix);
        sb->append("  Screen on: ");
        formatTimeMs(sb, screenOnTime / 1000);
        sb->append("(");
        sb->append(formatRatioLocked(screenOnTime, whichBatteryRealtime));
        sb->append("), Input events: ");
        sb->append(getInputEventCount(which));
    #ifdef PHONE_ENABLE
        sb->append(", Active phone call: ");
        formatTimeMs(sb, phoneOnTime / 1000);
        sb->append("(");
        sb->append(formatRatioLocked(phoneOnTime, whichBatteryRealtime));
        sb->append(")");
    #endif  // PHONE_ENABLE
        pw->println(sb->toString());
    #ifdef BACKLIGHT_ENABLE
        sb = new StringBuilder(128);
        sb->setLength(0);
        sb->append(prefix);
        sb->append("  Screen brightnesses: ");
        bool didOne = false;

        for (int32_t i = 0; i < NUM_SCREEN_BRIGHTNESS_BINS; i++) {
            const int64_t time = getScreenBrightnessTime(i, batteryRealtime, which);

            if (time == 0) {
                continue;
            }

            if (didOne) sb->append(", ");

            didOne = true;
            sb->append((*SCREEN_BRIGHTNESS_NAMES())[i]);
            sb->append(" ");
            formatTimeMs(sb, time / 1000);
            sb->append("(");
            sb->append(formatRatioLocked(time, screenOnTime));
            sb->append(")");
        }

        if (!didOne) sb->append("No activity");
    #endif  // BACKLIGHT_ENABLE

        pw->println(sb->toString());

        sb = new StringBuilder(128);
        sb->setLength(0);
        // Calculate total network and wakelock times across all uids.
        int64_t rxTotal = 0;
        int64_t txTotal = 0;
        int64_t fullWakeLockTimeTotalMicros = 0;
        int64_t partialWakeLockTimeTotalMicros = 0;
#if 0
        if (reqUid < 0) {
            KeyedVector<sp<String>, sp<BatteryStats::Timer> > kernelWakelocks = getKernelWakelockStats();
            // Map < String, ? extends BatteryStats.Timer > kernelWakelocks = getKernelWakelockStats();
            const int32_t KWL = kernelWakelocks.size();
            if (KWL > 0) {
                for (int32_t i = 0; i < KWL; i++) {
                    sp<String> linePrefix = new String(": ");
                    sb->setLength(0);
                    sb->append(prefix);
                    sb->append("  Kernel Wake lock ");
                    sb->append(kernelWakelocks.keyAt(i));
                    linePrefix = printWakeLock(sb, kernelWakelocks.valueAt(i), batteryRealtime, NULL, which,
                                               linePrefix);

                    if (!linePrefix->equals(": ")) {
                        sb->append(" realtime");
                        // Only print out wake locks that were held
                        pw->println(sb->toString());
                    }
                }
            }
        }

        for (int32_t iu = 0; iu < NU; iu++) {
            sp<Uid> u = uidStats.valueAt(iu);
            rxTotal += u->getTcpBytesReceived(which);
            txTotal += u->getTcpBytesSent(which);
            KeyedVector<sp<String>, sp<BatteryStats::Uid::Wakelock> > wakelocks = u->getWakelockStats();

            if (wakelocks.size()> 0) {
                const int32_t WL = wakelocks.size();

                for (int32_t i = 0; i < WL; i++) {
                    sp<Uid::Wakelock> wl = wakelocks.valueAt(i);
                    sp<Timer> fullWakeTimer = wl->getWakeTime(WAKE_TYPE_FULL);

                    if (fullWakeTimer != NULL) {
                        fullWakeLockTimeTotalMicros += fullWakeTimer->getTotalTimeLocked(
                                                           batteryRealtime, which);
                    }

                    sp<Timer> partialWakeTimer = wl->getWakeTime(WAKE_TYPE_PARTIAL);

                    if (partialWakeTimer != NULL) {
                        partialWakeLockTimeTotalMicros += partialWakeTimer->getTotalTimeLocked(
                                                              batteryRealtime, which);
                    }
                }
            }
        }

        pw->print(prefix);
        pw->print("  Total received: ");
        pw->print(formatBytesLocked(rxTotal));
        pw->print(", Total sent: ");
        pw->println(formatBytesLocked(txTotal));
        sb->setLength(0);
        sb->append(prefix);
        sb->append("  Total full wakelock time: ");
        formatTimeMs(sb,
                     (fullWakeLockTimeTotalMicros + 500) / 1000);
        sb->append(", Total partial waklock time: ");
        formatTimeMs(sb,
                     (partialWakeLockTimeTotalMicros + 500) / 1000);
        pw->println(sb->toString());
        sb->setLength(0);
        sb->append(prefix);

    #ifdef PHONE_ENABLE
        sb->append("  Signal levels: ");
        didOne = false;

        for (int32_t i = 0; i < SignalStrength::NUM_SIGNAL_STRENGTH_BINS; i++) {
            const int64_t time = getPhoneSignalStrengthTime(i, batteryRealtime, which);

            if (time == 0) {
                continue;
            }

            if (didOne) sb->append(", ");

            didOne = true;
            sb->append((*BatteryStats::SIGNAL_STRENGTH_NAMES())[i]);
            sb->append(" ");
            formatTimeMs(sb, time / 1000);
            sb->append("(");
            sb->append(formatRatioLocked(time, whichBatteryRealtime));
            sb->append(") ");
            sb->append(getPhoneSignalStrengthCount(i, which));
            sb->append("x");
        }

        if (!didOne) {
            sb->append("No activity");
        }

        pw->println(sb->toString());
        sb->setLength(0);
        sb->append(prefix);
        sb->append("  Signal scanning time: ");
        formatTimeMs(sb, getPhoneSignalScanningTime(batteryRealtime, which) / 1000);
        pw->println(sb->toString());
        sb->setLength(0);
        sb->append(prefix);
        sb->append("  Radio types: ");
        didOne = false;

        for (int32_t i = 0; i < NUM_DATA_CONNECTION_TYPES; i++) {
            const int64_t time = getPhoneDataConnectionTime(i, batteryRealtime, which);

            if (time == 0) {
                continue;
            }

            if (didOne) sb->append(", ");

            didOne = true;
            sb->append((*DATA_CONNECTION_NAMES())[i]);
            sb->append(" ");
            formatTimeMs(sb, time / 1000);
            sb->append("(");
            sb->append(formatRatioLocked(time, whichBatteryRealtime));
            sb->append(") ");
            sb->append(getPhoneDataConnectionCount(i, which));
            sb->append("x");
        }

        if (!didOne) {
            sb->append("No activity");
        }
    #endif  // PHONE_ENABLE

        pw->println(sb->toString());
        sb->setLength(0);
        sb->append(prefix);
        sb->append("  Radio data uptime when unplugged: ");
        sb->append(getRadioDataUptime() / 1000);
        sb->append(" ms");
        pw->println(sb->toString());
        sb->setLength(0);
        sb->append(prefix);
    #ifdef WIFI_ENABLE
        sb->append("  Wifi on: ");
        formatTimeMs(sb, wifiOnTime / 1000);
        sb->append("(");
        sb->append(formatRatioLocked(wifiOnTime, whichBatteryRealtime));
        sb->append("), Wifi running: ");
        formatTimeMs(sb, wifiRunningTime / 1000);
        sb->append("(");
        sb->append(formatRatioLocked(wifiRunningTime, whichBatteryRealtime));
    #endif  // WIFI_ENABLE
    #ifndef BT_DISABLE
        sb->append("), Bluetooth on: ");
        formatTimeMs(sb, bluetoothOnTime / 1000);
        sb->append("(");
        sb->append(formatRatioLocked(bluetoothOnTime, whichBatteryRealtime));
        sb->append(")");
        pw->println(sb->toString());
        pw->println(" ");
    #endif  // BT_DISABLE

        if (which == STATS_SINCE_UNPLUGGED) {
            if (getIsOnBattery()) {
                pw->print(prefix);
                pw->println("  Device is currently unplugged");
                pw->print(prefix);
                pw->print("    Discharge cycle start level: ");
                pw->println(getDischargeStartLevel());
                pw->print(prefix);
                pw->print("    Discharge cycle current level: ");
                pw->println(getDischargeCurrentLevel());
            } else {
                pw->print(prefix);
                pw->println("  Device is currently plugged into power");
                pw->print(prefix);
                pw->print("    Last discharge cycle start level: ");
                pw->println(getDischargeStartLevel());
                pw->print(prefix);
                pw->print("    Last discharge cycle end level: ");
                pw->println(getDischargeCurrentLevel());
            }

            pw->print(prefix);
            pw->print("    Amount discharged while screen on: ");
            pw->println(getDischargeAmountScreenOn());
            pw->print(prefix);
            pw->print("    Amount discharged while screen off: ");
            pw->println(getDischargeAmountScreenOff());
            pw->println(" ");
        } else {
            pw->print(prefix);
            pw->println("  Device battery use since last full charge");
            pw->print(prefix);
            pw->print("    Amount discharged (lower bound): ");
            pw->println(getLowDischargeAmountSinceCharge());
            pw->print(prefix);
            pw->print("    Amount discharged (upper bound): ");
            pw->println(getHighDischargeAmountSinceCharge());
            pw->print(prefix);
            pw->print("    Amount discharged while screen on: ");
            pw->println(getDischargeAmountScreenOnSinceCharge());
            pw->print(prefix);
            pw->print("    Amount discharged while screen off: ");
            pw->println(getDischargeAmountScreenOffSinceCharge());
            pw->println(" ");
        }

        for (int32_t iu = 0; iu < NU; iu++) {
            const int32_t uid = uidStats.keyAt(iu);

            if (reqUid >= 0 && uid != reqUid && uid != Process::SYSTEM_UID) {
                continue;
            }

            sp<Uid> u = uidStats.valueAt(iu);
            pw->println(prefix + "  #" + uid + ":");
            bool uidActivity = false;
            int64_t tcpReceived = u->getTcpBytesReceived(which);
            int64_t tcpSent = u->getTcpBytesSent(which);
    #ifdef WIFI_ENABLE
            int64_t fullWifiLockOnTime = u->getFullWifiLockTime(batteryRealtime, which);
            int64_t scanWifiLockOnTime = u->getScanWifiLockTime(batteryRealtime, which);
            int64_t uidWifiRunningTime = u->getWifiRunningTime(batteryRealtime, which);
    #endif  // WIFI_ENABLE
            if (tcpReceived != 0 || tcpSent != 0) {
                pw->print(prefix);
                pw->print("    Network: ");
                pw->print(formatBytesLocked(tcpReceived));
                pw->print(" received, ");
                pw->print(formatBytesLocked(tcpSent));
                pw->println(" sent");
            }

            if (u->hasUserActivity()) {
                bool hasData = false;

                for (int32_t i = 0; i < NUM_SCREEN_BRIGHTNESS_BINS; i++) {
                    int32_t val = u->getUserActivityCount(i, which);

                    if (val != 0) {
                        if (!hasData) {
                            sb->setLength(0);
                            sb->append("    User activity: ");
                            hasData = true;
                        } else {
                            sb->append(", ");
                        }

                        sb->append(val);
                        sb->append(" ");
                        sb->append((*BatteryStats::Uid::USER_ACTIVITY_TYPES())[i]);
                    }
                }

                if (hasData) {
                    pw->println(sb->toString());
                }
            }

    #ifdef WIFI_ENABLE
            if (fullWifiLockOnTime != 0 || scanWifiLockOnTime != 0
                    || uidWifiRunningTime != 0) {
                sb->setLength(0);
                sb->append(prefix);
                sb->append("    Wifi Running: ");
                formatTimeMs(sb, uidWifiRunningTime / 1000);
                sb->append("(");
                sb->append(formatRatioLocked(uidWifiRunningTime,
                                            whichBatteryRealtime));
                sb->append(")\n");
                sb->append(prefix);
                sb->append("    Full Wifi Lock: ");
                formatTimeMs(sb, fullWifiLockOnTime / 1000);
                sb->append("(");
                sb->append(formatRatioLocked(fullWifiLockOnTime,
                                            whichBatteryRealtime));
                sb->append(")\n");
                sb->append(prefix);
                sb->append("    Scan Wifi Lock: ");
                formatTimeMs(sb, scanWifiLockOnTime / 1000);
                sb->append("(");
                sb->append(formatRatioLocked(scanWifiLockOnTime,
                                            whichBatteryRealtime));
                sb->append(")");
                pw->println(sb->toString());
            }
            #endif  // WIFI_ENABLE

            // Map < String, ? extends BatteryStats.Uid.Wakelock > wakelocks = u->getWakelockStats();
            KeyedVector<sp<String> , sp<BatteryStats::Uid::Wakelock> > wakelocks = u->getWakelockStats();



            if (wakelocks.size() > 0) {
                int64_t totalFull = 0, totalPartial = 0, totalWindow = 0;
                int32_t count = 0;
                const int32_t WL = wakelocks.size();

                for (int32_t i = 0; i < WL; i++) {
                    sp<Uid::Wakelock> wl = wakelocks.valueAt(i);

                    sp<String> linePrefix = new String(": ");
                    sb->setLength(0);
                    sb->append(prefix);
                    sb->append("    Wake lock ");
                    sb->append(wakelocks.keyAt(i));
                    linePrefix = printWakeLock(sb, wl->getWakeTime(WAKE_TYPE_FULL), batteryRealtime,
                                               new String("full"), which, linePrefix);
                    linePrefix = printWakeLock(sb, wl->getWakeTime(WAKE_TYPE_PARTIAL), batteryRealtime,
                                               new String("partial"), which, linePrefix);
                    linePrefix = printWakeLock(sb, wl->getWakeTime(WAKE_TYPE_WINDOW), batteryRealtime,
                                               new String("window"), which, linePrefix);

                    if (!linePrefix->equals(": ")) {
                        sb->append(" realtime");
                        // Only print out wake locks that were held
                        pw->println(sb->toString());
                        uidActivity = true;
                        count++;
                    }

                    totalFull += computeWakeLock(wl->getWakeTime(WAKE_TYPE_FULL),
                                                 batteryRealtime, which);
                    totalPartial += computeWakeLock(wl->getWakeTime(WAKE_TYPE_PARTIAL),
                                                    batteryRealtime, which);
                    totalWindow += computeWakeLock(wl->getWakeTime(WAKE_TYPE_WINDOW),
                                                   batteryRealtime, which);
                }

                if (count > 1) {
                    if (totalFull != 0 || totalPartial != 0 || totalWindow != 0) {
                        sb->setLength(0);
                        sb->append(prefix);
                        sb->append("    TOTAL wake: ");
                        bool needComma = false;

                        if (totalFull != 0) {
                            needComma = true;
                            formatTimeMs(sb, totalFull);
                            sb->append("full");
                        }

                        if (totalPartial != 0) {
                            if (needComma) {
                                sb->append(", ");
                            }

                            needComma = true;
                            formatTimeMs(sb, totalPartial);
                            sb->append("partial");
                        }

                        if (totalWindow != 0) {
                            if (needComma) {
                                sb->append(", ");
                            }

                            needComma = true;
                            formatTimeMs(sb, totalWindow);
                            sb->append("window");
                        }

                        sb->append(" realtime");
                        pw->println(sb->toString());
                    }
                }
            }

    #ifdef SENSOR_ENABLE
            // Map < Integer, ? extends BatteryStats.Uid.Sensor > sensors = u->getSensorStats();
            KeyedVector<int32_t, sp<BatteryStats::Uid::Sensor> > sensors = u->getSensorStats();

            if (sensors.size() > 0) {
                const int32_t WL = sensors.size();
                for (int32_t i = 0; i < WL; i++) {
                    sp<Uid::Sensor> se = sensors.valueAt(i);
                    int32_t sensorNumber = sensors.keyAt(i);
                    sb->setLength(0);
                    sb->append(prefix);
                    sb->append("    Sensor ");
                    int32_t handle = se->getHandle();

                    if (handle == Uid::Sensor::GPS) {
                        sb->append("GPS");
                    } else {
                        sb->append(handle);
                    }

                    sb->append(": ");
                    sp<Timer> timer = se->getSensorTime();

                    if (timer != NULL) {
                        // Convert from microseconds to milliseconds with rounding
                        int64_t totalTime = (timer->getTotalTimeLocked(
                                              batteryRealtime, which) + 500) / 1000;
                        int32_t count = timer->getCountLocked(which);

                        // timer.logState();
                        if (totalTime != 0) {
                            formatTimeMs(sb, totalTime);
                            sb->append("realtime (");
                            sb->append(count);
                            sb->append(" times)");
                        } else {
                            sb->append("(not used)");
                        }
                    } else {
                        sb->append("(not used)");
                    }

                    pw->println(sb->toString());
                    uidActivity = true;
                }
            }
    #endif  // SENSOR_ENABLE

            // Map < String, ? extends BatteryStats.Uid.Proc > processStats = u->getProcessStats();
            KeyedVector<sp<String> , sp<BatteryStats::Uid::Proc> > processStats = u->getProcessStats();

            if (processStats.size() > 0) {
                const int32_t PS = processStats.size();

                for (int32_t i = 0; i < PS; i++) {
                    sp<Uid::Proc> ps = processStats.valueAt(i);
                    int64_t userTime;
                    int64_t systemTime;
                    int32_t starts;
                    int32_t numExcessive;
                    userTime = ps->getUserTime(which);
                    systemTime = ps->getSystemTime(which);
                    starts = ps->getStarts(which);
                    numExcessive = which == STATS_SINCE_CHARGED
                                   ? ps->countExcessivePowers() : 0;

                    if (userTime != 0 || systemTime != 0 || starts != 0
                            || numExcessive != 0) {
                        sb->setLength(0);
                        sb->append(prefix);
                        sb->append("    Proc ");
                        sb->append(processStats.keyAt(i));
                        sb->append(":\n");
                        sb->append(prefix);
                        sb->append("      CPU: ");
                        formatTime(sb, userTime);
                        sb->append("usr + ");
                        formatTime(sb, systemTime);
                        sb->append("krn");

                        if (starts != 0) {
                            sb->append("\n");
                            sb->append(prefix);
                            sb->append("      ");
                            sb->append(starts);
                            sb->append(" proc starts");
                        }

                        pw->println(sb->toString());

                        for (int32_t e = 0; e < numExcessive; e++) {
                            sp<Uid::Proc::ExcessivePower> ew = ps->getExcessivePower(e);

                            if (ew != NULL) {
                                pw->print(prefix);
                                pw->print("      * Killed for ");

                                if (ew->type == Uid::Proc::ExcessivePower::TYPE_WAKE) {
                                    pw->print("wake lock");
                                } else if (ew->type == Uid::Proc::ExcessivePower::TYPE_CPU) {
                                    pw->print("cpu");
                                } else {
                                    pw->print("unknown");
                                }

                                pw->print(" use: ");
                                TimeUtils::formatDuration(ew->usedTime, pw);
                                pw->print(" over ");
                                TimeUtils::formatDuration(ew->overTime, pw);
                                pw->print(" (");
                                pw->print((ew->usedTime*100) / ew->overTime);
                                pw->println("%)");
                            }
                        }

                        uidActivity = true;
                    }
                }
            }

            // Map < String, ? extends BatteryStats.Uid.Pkg > packageStats = u->getPackageStats();
            KeyedVector<sp<String> , sp<BatteryStats::Uid::Pkg> > packageStats = u->getPackageStats();

            if (packageStats.size() > 0) {
                const int32_t PS = packageStats.size();
                for (int32_t i = 0; i < PS; i++) {
                    pw->print(prefix);
                    pw->print("    Apk ");
                    pw->print(packageStats.keyAt(i));
                    pw->println(":");
                    bool apkActivity = false;
                    sp<Uid::Pkg> ps = packageStats.valueAt(i);
                    int32_t wakeups = ps->getWakeups(which);

                    if (wakeups != 0) {
                        pw->print(prefix);
                        pw->print("      ");
                        pw->print(wakeups);
                        pw->println(" wakeup alarms");
                        apkActivity = true;
                    }

                    // Map < String, ? extends  Uid.Pkg.Serv > serviceStats = ps->getServiceStats();
                    KeyedVector<sp<String> , sp<BatteryStats::Uid::Pkg::Serv> >  serviceStats = ps->getServiceStats();

                    if (serviceStats.size() > 0) {
                        const int32_t SS = serviceStats.size();

                        for (int32_t i = 0; i < SS; i++) {
                            sp<BatteryStats::Uid::Pkg::Serv> ss = serviceStats.valueAt(i);
                            int64_t startTime = ss->getStartTime(batteryUptime, which);
                            int32_t starts = ss->getStarts(which);
                            int32_t launches = ss->getLaunches(which);

                            if (startTime != 0 || starts != 0 || launches != 0) {
                                sb->setLength(0);
                                sb->append(prefix);
                                sb->append("      Service ");
                                sb->append(serviceStats.keyAt(i));
                                sb->append(":\n");
                                sb->append(prefix);
                                sb->append("        Created for: ");
                                formatTimeMs(sb, startTime / 1000);
                                sb->append(" uptime\n");
                                sb->append(prefix);
                                sb->append("        Starts: ");
                                sb->append(starts);
                                sb->append(", launches: ");
                                sb->append(launches);
                                pw->println(sb->toString());
                                apkActivity = true;
                            }
                        }
                    }

                    if (!apkActivity) {
                        pw->print(prefix);
                        pw->println("      (nothing executed)");
                    }

                    uidActivity = true;
                }
            }

            if (!uidActivity) {
                pw->print(prefix);
                pw->println("    (nothing executed)");
            }
        }
#endif
    }
}  // NOLINT

void BatteryStats::printBitDescriptions(const sp<PrintWriter>& pw, int32_t oldval, int32_t newval, const sp<Blob<sp<BitDescription> > >& descriptions) {
    GLOGENTRY();

    int32_t diff = oldval ^ newval;
    if (diff == 0) {
        return;
    }
    for (size_t i = 0; i < descriptions->length(); i++) {
        sp<BitDescription> bd = descriptions[i];
        if ((diff & bd->mask) != 0) {
            if (bd->shift < 0) {
                pw->print((newval & bd->mask) != 0 ? " +" : " -");
                pw->print(bd->name);
            } else {
                pw->print(" ");
                pw->print(bd->name);
                pw->print("=");
                int32_t val = (newval & bd->mask) >> bd->shift;
                if (bd->values != NULL && val >= 0 && val < static_cast<int32_t>(bd->values->length())) {
                    pw->print((*bd->values)[val]);
                } else {
                    pw->print(val);
                }
            }
        }
    }
}

void BatteryStats::prepareForDumpLocked() {
    GLOGENTRY();
}

void BatteryStats::dumpLocked(int32_t fd, sp<PrintWriter>& pw) {  // NOLINT
    GLOGENTRY();

    if (fd != -1) {
        sp<StringWriter> sw = new StringWriter();
        sp<PrintWriter> pw_ = new PrintWriter(sp<Writer>(sw.get()));

        if (pw_ == NULL) {
            GLOGW("pw_ = NULL");
            return;
        }

        prepareForDumpLocked();
        int64_t now = getHistoryBaseTime() + elapsedRealtime();
        int32_t i = 0;
        sp<HistoryItem> rec = new HistoryItem();

        if (startIteratingHistoryLocked()) {
            pw_->println("Battery History:");
            sp<HistoryPrinter> hprinter = new HistoryPrinter();

            i = 0;
            while (getNextHistoryLocked(rec)) {
                // Customize +++
                if (1) {  // (ProfileConfig.getProfileDebugBatteryHistory())
                    pw_->print(String::format("(%08llx)", rec->time));
                }
                // Customize ---
                hprinter->printNextItem(pw_, rec, now);

                if ((i % 5) == 0) {
                    if (sw != NULL && sw->toString() != NULL) {
                        write(fd, sw->toString()->string(), sw->toString()->size());
                        sw = new StringWriter();
                        pw_ = new PrintWriter(sp<Writer>(sw.get()));
                    }
                }

                i++;
            }
            finishIteratingHistoryLocked();
            pw_->println("");
        }

        if (sw != NULL && sw->toString() != NULL) {
            write(fd, sw->toString()->string(), sw->toString()->size());
            sw = new StringWriter();
            pw_ = new PrintWriter(sp<Writer>(sw.get()));
        }

        if (startIteratingOldHistoryLocked()) {
            pw_->println("Old battery History:");
            sp<HistoryPrinter> hprinter = new HistoryPrinter();

            i = 0;

            while (getNextOldHistoryLocked(rec)) {
                hprinter->printNextItem(pw_, rec, now);

                if ((i % 5) == 0) {
                    if (sw != NULL && sw->toString() != NULL) {
                        write(fd, sw->toString()->string(), sw->toString()->size());
                        sw = new StringWriter();
                        pw_ = new PrintWriter(sp<Writer>(sw.get()));
                    }
                }

                i++;
            }

            finishIteratingOldHistoryLocked();
            pw_->println("");
        }

        if (sw != NULL && sw->toString() != NULL) {
            write(fd, sw->toString()->string(), sw->toString()->size());
            sw = new StringWriter();
            pw_ = new PrintWriter(sp<Writer>(sw.get()));
        }

        // SparseArray <? extends Uid > uidStats = getUidStats();
        KeyedVector<int32_t, sp<BatteryStats::Uid> > uidStats = getUidStats();
        const int32_t NU = uidStats.size();
        bool didPid = false;
        int64_t nowRealtime = elapsedRealtime();

        for (int32_t i = 0; i < NU; i++) {
            sp<Uid> uid = uidStats.valueAt(i);
            // SparseArray <? extends Uid.Pid > pids = uid.getPidStats();
            KeyedVector<int32_t, sp<BatteryStats::Uid::Pid> > pids = uid->getPidStats();
            if (!pids.isEmpty()) {
                for (size_t j = 0; j < pids.size(); j++) {
                    sp<Uid::Pid> pid = pids.valueAt(j);

                    if (!didPid) {
                        pw_->println("Per-PID Stats:");
                        didPid = true;
                    }

                    int64_t time = pid->mWakeSum + (pid->mWakeStart != 0 ? (nowRealtime - pid->mWakeStart) : 0);
                    pw_->print("  PID ");
                    pw_->print(pids.keyAt(j));
                    pw_->print(" wake time: ");
                    TimeUtils::formatDuration(time, pw_);
                    pw_->println("");
                }
            }
        }

        if (sw != NULL && sw->toString() != NULL) {
            write(fd, sw->toString()->string(), sw->toString()->size());
            sw = new StringWriter();
            pw_ = new PrintWriter(sp<Writer>(sw.get()));
        }

        if (didPid) {
            pw_->println("");
        }

        pw_->println("Statistics since last charge:");
        pw_->println(String::format("  System starts:%d , currently on battery:%d ", getStartCount(), getIsOnBattery()));

        if (sw != NULL && sw->toString() != NULL) {
            write(fd, sw->toString()->string(), sw->toString()->size());
            sw = new StringWriter();
            pw_ = new PrintWriter(sp<Writer>(sw.get()));
        }

        dumpLocked(fd, pw_, new String(""), STATS_SINCE_CHARGED, -1);

        pw_->println("");
        pw_->println("Statistics since last unplugged:");

        if (sw != NULL && sw->toString() != NULL) {
            write(fd, sw->toString()->string(), sw->toString()->size());
            sw = new StringWriter();
            pw_ = new PrintWriter(sp<Writer>(sw.get()));
        }

        dumpLocked(fd, pw_, new String(""), STATS_SINCE_UNPLUGGED, -1);

        if (sw != NULL && sw->toString() != NULL) {
            write(fd, sw->toString()->string(), sw->toString()->size());
            sw = new StringWriter();
            pw_ = new PrintWriter(sp<Writer>(sw.get()));
        }
    } else {
        prepareForDumpLocked();
        int64_t now = getHistoryBaseTime() + elapsedRealtime();
        sp<HistoryItem> rec = new HistoryItem();

        if (startIteratingHistoryLocked()) {
            pw->println("Battery History:");
            sp<HistoryPrinter> hprinter = new HistoryPrinter();

            while (getNextHistoryLocked(rec)) {
                // Customize +++
                if (1) {  // (ProfileConfig.getProfileDebugBatteryHistory())
                    pw->print(String::format("(%08llx)", rec->time));
                }
                // Customize ---
                hprinter->printNextItem(pw, rec, now);
            }

            finishIteratingHistoryLocked();
            pw->println("");
        }

        if (startIteratingOldHistoryLocked()) {
            pw->println("Old battery History:");
            sp<HistoryPrinter> hprinter = new HistoryPrinter();

            while (getNextOldHistoryLocked(rec)) {
                hprinter->printNextItem(pw, rec, now);
            }

            finishIteratingOldHistoryLocked();
            pw->println("");
        }

        // SparseArray <? extends Uid > uidStats = getUidStats();
        KeyedVector<int32_t, sp<BatteryStats::Uid> > uidStats = getUidStats();
        const int32_t NU = uidStats.size();
        bool didPid = false;
        int64_t nowRealtime = elapsedRealtime();

        for (int32_t i = 0; i < NU; i++) {
            sp<Uid> uid = uidStats.valueAt(i);
            // SparseArray <? extends Uid.Pid > pids = uid.getPidStats();
            KeyedVector<int32_t, sp<BatteryStats::Uid::Pid> > pids = uid->getPidStats();
            if (!pids.isEmpty()) {
                for (size_t j = 0; j < pids.size(); j++) {
                    sp<Uid::Pid> pid = pids.valueAt(j);

                    if (!didPid) {
                        pw->println("Per-PID Stats:");
                        didPid = true;
                    }

                    int64_t time = pid->mWakeSum + (pid->mWakeStart != 0 ? (nowRealtime - pid->mWakeStart) : 0);
                    pw->print("  PID ");
                    pw->print(pids.keyAt(j));
                    pw->print(" wake time: ");
                    TimeUtils::formatDuration(time, pw);
                    pw->println("");
                }
            }
        }

        if (didPid) {
            pw->println("");
        }

        pw->println("Statistics since last charge:");
        pw->println(String::format("  System starts:%d , currently on battery:%d ", getStartCount(), getIsOnBattery()));

        dumpLocked(fd, pw, new String(""), STATS_SINCE_CHARGED, -1);
        pw->println("");
        pw->println("Statistics since last unplugged:");

        dumpLocked(fd, pw, new String(""), STATS_SINCE_UNPLUGGED, -1);
    }
}

void  BatteryStats::dumpCheckinLocked(int32_t fd, sp<PrintWriter>& pw, const android::Vector<android::String16>& args, Vector<sp<ApplicationInfo> >& apps) {
    GLOGENTRY();

    if (fd != -1) {
        sp<StringWriter> sw = new StringWriter();
        sp<PrintWriter> pw_ = new PrintWriter(sp<Writer>(sw.get()));

        if (pw_ == NULL) {
            GLOGW("pw_ = NULL");
            return;
        }

        Vector<sp<String> > tmp_string = Vector<sp<String> >();
        prepareForDumpLocked();
        bool isUnpluggedOnly = false;
        sp<String> value = new String("-u");

        for (size_t i = 0 ; i < args.size() ; i++) {
            if (value->equals(args[i])) {
                if (LOCAL_LOGV) {
                    GLOGI("BatteryStats Dumping unplugged data");
                }
                isUnpluggedOnly = true;
            }
        }

        if (!apps.isEmpty()) {
            // SparseArray<ArrayList<String>> uids = new SparseArray<ArrayList<String>>();
            KeyedVector<int32_t, List<sp<String> > > *uids = new KeyedVector<int32_t, List<sp<String> > >();

            for (int32_t i = 0; i < static_cast<int32_t>(apps.size()); i++) {
                // ApplicationInfo ai = apps.get(i);
                sp<ApplicationInfo> ai = apps.itemAt(i);

                // ArrayList<String> pkgs = uids.get(ai->uid);
                List<sp<String> > pkgs;
                ssize_t index = uids->indexOfKey(ai->uid);
                pkgs = uids->valueAt(index);

                if (pkgs.empty()) {
                    // pkgs = new List<String>();
                    List<sp<String> > pkgs;
                    uids->add(ai->uid, pkgs);
                }

                pkgs.push_back(ai->packageName);
            }

            // SparseArray <? extends Uid > uidStats = getUidStats();
            KeyedVector<int32_t, sp<BatteryStats::Uid> > uidStats = getUidStats();
            const int32_t NU = uidStats.size();
            sp<Blob<sp<String> > > lineArgs = new Blob<sp<String> >(2);

            for (int32_t i = 0; i < NU; i++) {
                int32_t uid = uidStats.keyAt(i);
                List<sp<String> > pkgs;  // = uids->get(uid);
                ssize_t index = uids->indexOfKey(uid);
                pkgs = uids->valueAt(index);

                if (!pkgs.empty()) {
                    typedef List<sp<String> >::iterator Iter;
                    for (Iter it = pkgs.begin(); it != pkgs.end(); it++) {
                        (*lineArgs)[0] = Integer::toString(uid);
                        (*lineArgs)[1] = (*it);
                        tmp_string.clear();

                        for (int32_t i = 0; i < static_cast<int32_t>(lineArgs->length()); i++) {
                            tmp_string.push(lineArgs[i]);
                        }

                        dumpLine(pw_, 0 /* uid */, new String("i") /* category */, UID_DATA(), tmp_string);

                        if (sw != NULL && sw->toString() != NULL) {
                            write(fd, sw->toString()->string(), sw->toString()->size());
                            sw = new StringWriter();
                            pw_ = new PrintWriter(sp<Writer>(sw.get()));
                        }
                    }
                }
            }
        }

        if (isUnpluggedOnly) {
            dumpCheckinLocked(fd, pw_, STATS_SINCE_UNPLUGGED, -1);
        } else {
            dumpCheckinLocked(fd, pw_, STATS_SINCE_CHARGED, -1);
            dumpCheckinLocked(fd, pw_, STATS_SINCE_UNPLUGGED, -1);
        }
    } else {
        Vector<sp<String> > tmp_string = Vector<sp<String> >();
        prepareForDumpLocked();
        bool isUnpluggedOnly = false;
        sp<String> value = new String("-u");

        for (size_t i = 0 ; i < args.size() ; i++) {
            if (value->equals(args[i])) {
                if (LOCAL_LOGV) {
                    GLOGI("BatteryStats Dumping unplugged data");
                }
                isUnpluggedOnly = true;
            }
        }

        if (!apps.isEmpty()) {
            // SparseArray<ArrayList<String>> uids = new SparseArray<ArrayList<String>>();
            KeyedVector<int32_t, List<sp<String> > > *uids = new KeyedVector<int32_t, List<sp<String> > >();

            for (int32_t i = 0; i < static_cast<int32_t>(apps.size()); i++) {
                // ApplicationInfo ai = apps.get(i);
                sp<ApplicationInfo> ai = apps.itemAt(i);

                // ArrayList<String> pkgs = uids.get(ai->uid);
                List<sp<String> > pkgs;
                ssize_t index = uids->indexOfKey(ai->uid);
                pkgs = uids->valueAt(index);

                if (pkgs.empty()) {
                    // pkgs = new List<String>();
                    List<sp<String> > pkgs;
                    uids->add(ai->uid, pkgs);
                }

                pkgs.push_back(ai->packageName);
            }

            // SparseArray <? extends Uid > uidStats = getUidStats();
            KeyedVector<int32_t, sp<BatteryStats::Uid> > uidStats = getUidStats();
            const int32_t NU = uidStats.size();
            sp<Blob<sp<String> > > lineArgs = new Blob<sp<String> >(2);

            for (int32_t i = 0; i < NU; i++) {
                int32_t uid = uidStats.keyAt(i);
                List<sp<String> > pkgs;  // = uids->get(uid);
                ssize_t index = uids->indexOfKey(uid);
                pkgs = uids->valueAt(index);

                if (!pkgs.empty()) {
                    typedef List<sp<String> >::iterator Iter;
                    for (Iter it = pkgs.begin(); it != pkgs.end(); it++) {
                        (*lineArgs)[0] = Integer::toString(uid);
                        (*lineArgs)[1] = (*it);

                        tmp_string.clear();

                        for (int32_t i = 0; i < static_cast<int32_t>(lineArgs->length()); i++) {
                            tmp_string.push(lineArgs[i]);
                        }

                        dumpLine(pw, 0 /* uid */, new String("i") /* category */, UID_DATA(), tmp_string);
                    }
                }
            }
        }

        if (isUnpluggedOnly) {
            dumpCheckinLocked(fd, pw, STATS_SINCE_UNPLUGGED, -1);
        } else {
            dumpCheckinLocked(fd, pw, STATS_SINCE_CHARGED, -1);
            dumpCheckinLocked(fd, pw, STATS_SINCE_UNPLUGGED, -1);
        }
    }
}

BatteryStats::HistoryItem::HistoryItem()
    : PREINIT_DYNAMIC(),
    next(NULL),
    time(0),
    cmd(CMD_NULL),
    batteryLevel(0),
    batteryStatus(0),
    batteryHealth(0),
    batteryPlugType(0),
    batteryTemperature(0),
    batteryVoltage(0),
    states(0) {
    GLOGENTRY();
}

BatteryStats::HistoryItem::HistoryItem(int64_t time, const Parcel& src)
    : PREINIT_DYNAMIC(),
    next(NULL),
    time(time),
    cmd(CMD_NULL),
    batteryLevel(0),
    batteryStatus(0),
    batteryHealth(0),
    batteryPlugType(0),
    batteryTemperature(0),
    batteryVoltage(0),
    states(0) {
    GLOGENTRY();

    readFromParcel(src);
}

int32_t BatteryStats::HistoryItem::describeContents() {
    GLOGENTRY();

    return 0;
}

void BatteryStats::HistoryItem::writeToParcel(Parcel* dest, int32_t flags) {  // NOLINT
    GLOGENTRY();

    dest->writeInt64(time);
    int32_t bat = ((static_cast<int32_t>(cmd)) & 0xff)
              | (((static_cast<int32_t>(batteryLevel)) << 8) & 0xff00)
              | (((static_cast<int32_t>(batteryStatus)) << 16) & 0xf0000)
              | (((static_cast<int32_t>(batteryHealth)) << 20) & 0xf00000)
              | (((static_cast<int32_t>(batteryPlugType)) << 24) & 0xf000000);
    dest->writeInt32(bat);
    bat = ((static_cast<int32_t>(batteryTemperature)) & 0xffff)
          | (((static_cast<int32_t>(batteryVoltage) << 16) & 0xffff0000));
    dest->writeInt32(bat);
    dest->writeInt32(states);
}

void BatteryStats::HistoryItem::readFromParcel(const Parcel& src) {
    GLOGENTRY();

    int32_t bat = src.readInt32();
    cmd = static_cast<byte_t>(bat & 0xff);
    batteryLevel = static_cast<byte_t>((bat >> 8) & 0xff);
    batteryStatus = static_cast<byte_t>((bat >> 16) & 0xf);
    batteryHealth = static_cast<byte_t>((bat >> 20) & 0xf);
    batteryPlugType = static_cast<byte_t>((bat >> 24) & 0xf);
    bat = src.readInt32();
    batteryTemperature = static_cast<char16_t>(bat & 0xffff);
    batteryVoltage = static_cast<char16_t>((bat >> 16) & 0xffff);
    states = src.readInt32();
}

void BatteryStats::HistoryItem::writeDelta(Parcel& dest, const sp<HistoryItem>& last) {
    GLOGENTRY();

    if (last == NULL || last->cmd != CMD_UPDATE) {
        dest.writeInt32(DELTA_TIME_ABS);
        writeToParcel(&dest, 0);
        return;
    }

    const int64_t deltaTime = time - last->time;
    const int32_t lastBatteryLevelInt = last->buildBatteryLevelInt();
    const int32_t lastStateInt = last->buildStateInt();
    int32_t deltaTimeToken;

    if (deltaTime < 0 || deltaTime > Integer::MAX_VALUE) {
        deltaTimeToken = DELTA_TIME_LONG;
    } else if (deltaTime >= DELTA_TIME_ABS) {
        deltaTimeToken = DELTA_TIME_INT;
    } else {
        deltaTimeToken = static_cast<int32_t>(deltaTime);
    }

    int32_t firstToken = deltaTimeToken
                     | (cmd << DELTA_CMD_SHIFT)
                     | (states & DELTA_STATE_MASK);
    const int32_t batteryLevelInt = buildBatteryLevelInt();
    const bool batteryLevelIntChanged = batteryLevelInt != lastBatteryLevelInt;

    if (batteryLevelIntChanged) {
        firstToken |= DELTA_BATTERY_LEVEL_FLAG;
    }

    const int32_t stateInt = buildStateInt();
    const bool stateIntChanged = stateInt != lastStateInt;

    if (stateIntChanged) {
        firstToken |= DELTA_STATE_FLAG;
    }

    dest.writeInt32(firstToken);

    if (DEBUG) {
        // GLOGI("%s WRITE DELTA: firstToken=0x%s deltaTime=%lld", LOG_TAG, Integer::toHexString(firstToken), deltaTime);
    }

    if (deltaTimeToken >= DELTA_TIME_INT) {
        if (deltaTimeToken == DELTA_TIME_INT) {
            if (DEBUG) {
                GLOGI("%s WRITE DELTA: int32_t deltaTime=%d", LOG_TAG, static_cast<int32_t>(deltaTime));
            }

            dest.writeInt32(static_cast<int32_t>(deltaTime));
        } else {
            if (DEBUG) {
            GLOGI("%s WRITE DELTA: long deltaTime=%lld", LOG_TAG, deltaTime);
        }

            dest.writeInt64(deltaTime);
        }
    }

    if (batteryLevelIntChanged) {
        dest.writeInt32(batteryLevelInt);

        if (DEBUG) {
            // GLOGI("%s WRITE DELTA: batteryToken=0x%s batteryLevel=%d  batteryTemp=%d  batteryVolt=%d", LOG_TAG,
            //                                                                                           Integer::toHexString(batteryLevelInt),
            //                                                                                           batteryLevel,
            //                                                                                           (int)batteryTemperature,
            //                                                                                           (int)batteryVoltage);
        }
    }

    if (stateIntChanged) {
        dest.writeInt32(stateInt);

        if (DEBUG) {
            // GLOGI("%s WRITE DELTA: stateToken=0x%s batteryStatus=%d  batteryHealth=%d  batteryPlugType=%d states=0x", LOG_TAG,
              //                                                                                                        Integer::toHexString(stateInt),
                //                                                                                                      batteryStatus,
                  //                                                                                                    batteryHealth,
                    //                                                                                                  batteryPlugType,
                      //                                                                                                Integer::toHexString(states));
        }
    }
}

int32_t BatteryStats::HistoryItem::buildBatteryLevelInt() {
    GLOGENTRY();

    return (((static_cast<int32_t>(batteryLevel)) << 24)&0xff000000)
           | (((static_cast<int32_t>(batteryTemperature)) << 14)&0x00ffc000)
           | ((static_cast<int32_t>(batteryVoltage))&0x00003fff);
}

int32_t BatteryStats::HistoryItem::buildStateInt() {
    GLOGENTRY();

    return (((static_cast<int32_t>(batteryStatus)) << 28)&0xf0000000)
           | (((static_cast<int32_t>(batteryHealth)) << 24)&0x0f000000)
           | (((static_cast<int32_t>(batteryPlugType)) << 22)&0x00c00000)
           | (states&(~DELTA_STATE_MASK));
}

void BatteryStats::HistoryItem::readDelta(Parcel& src) {  // NOLINT
    GLOGENTRY();

    int32_t firstToken = src.readInt32();
    int32_t deltaTimeToken = firstToken & DELTA_TIME_MASK;
    cmd = static_cast<byte_t>((firstToken >> DELTA_CMD_SHIFT) & DELTA_CMD_MASK);

    if (DEBUG) {
        // GLOGI("%s READ DELTA: firstToken=0x%s deltaTimeToken=%d", LOG_TAG,
          //                                                        Integer::toHexString(firstToken),
            //                                                      deltaTimeToken);
    }

    if (deltaTimeToken < DELTA_TIME_ABS) {
        time += deltaTimeToken;
    } else if (deltaTimeToken == DELTA_TIME_ABS) {
        time = src.readInt64();
        readFromParcel(src);
        return;
    } else if (deltaTimeToken == DELTA_TIME_INT) {
        int32_t delta = src.readInt32();
        time += delta;

        if (DEBUG) {
            // GLOGI("%s READ DELTA: time delta=%lld new time=%lld", LOG_TAG,
              //                                                    delta,
                //                                                  time);
        }
    } else {
        int64_t delta = src.readInt64();

        if (DEBUG) {
            GLOGI("%s READ DELTA: time delta=%lld new time=%lld", LOG_TAG,
                                                                  delta,
                                                                  time);
        }

        time += delta;
    }

    if ((firstToken & DELTA_BATTERY_LEVEL_FLAG) != 0) {
        int32_t batteryLevelInt = src.readInt32();
        batteryLevel = static_cast<byte_t>((batteryLevelInt >> 24) & 0xff);
        batteryTemperature = static_cast<char16_t>((batteryLevelInt >> 14) & 0x3ff);
        batteryVoltage = static_cast<char16_t>(batteryLevelInt & 0x3fff);

        if (DEBUG) {
             // GLOGI("%s READ DELTA: batteryToken=0x%s batteryLevel=%d batteryTemp=%d batteryVolt=%d", LOG_TAG,
               //                                                                                     Integer::toHexString(batteryLevelInt),
                 //                                                                                   batteryLevel,
                   //                                                                                 batteryTemperature,
                     //                                                                               batteryVoltage);
        }
    }

    if ((firstToken & DELTA_STATE_FLAG) != 0) {
        int32_t stateInt = src.readInt32();
        states = (firstToken & DELTA_STATE_MASK) | (stateInt & (~DELTA_STATE_MASK));
        batteryStatus = static_cast<byte_t>((stateInt >> 28) & 0xf);
        batteryHealth = static_cast<byte_t>((stateInt >> 24) & 0xf);
        batteryPlugType = static_cast<byte_t>((stateInt >> 22) & 0x3);

        if (DEBUG) {
             // GLOGI("%s READ DELTA: stateToken=0x%s batteryStatus=%d batteryHealth=%d batteryPlugType=%d states=0x%s", LOG_TAG,
               //                                                                                                      Integer::toHexString(stateInt),
                 //                                                                                                    batteryStatus,
                   //                                                                                                  batteryHealth,
                     //                                                                                                batteryPlugType,
                       //                                                                                              Integer::toHexString(states));
        }
    } else {
        states = (firstToken & DELTA_STATE_MASK) | (states & (~DELTA_STATE_MASK));
    }
}

void BatteryStats::HistoryItem::clear() {
    GLOGENTRY();

    time = 0;
    cmd = CMD_NULL;
    batteryLevel = 0;
    batteryStatus = 0;
    batteryHealth = 0;
    batteryPlugType = 0;
    batteryTemperature = 0;
    batteryVoltage = 0;
    states = 0;
}

void BatteryStats::HistoryItem::setTo(const sp<HistoryItem>& o) {
    GLOGENTRY();

    time = o->time;
    cmd = o->cmd;
    batteryLevel = o->batteryLevel;
    batteryStatus = o->batteryStatus;
    batteryHealth = o->batteryHealth;
    batteryPlugType = o->batteryPlugType;
    batteryTemperature = o->batteryTemperature;
    batteryVoltage = o->batteryVoltage;
    states = o->states;
}

void BatteryStats::HistoryItem::setTo(int64_t time, byte_t cmd, const sp<HistoryItem>& o) {
    GLOGENTRY();

    this->time = time;
    this->cmd = cmd;
    batteryLevel = o->batteryLevel;
    batteryStatus = o->batteryStatus;
    batteryHealth = o->batteryHealth;
    batteryPlugType = o->batteryPlugType;
    batteryTemperature = o->batteryTemperature;
    batteryVoltage = o->batteryVoltage;
    states = o->states;
}

bool BatteryStats::HistoryItem::same(const sp<HistoryItem>& o) {
    GLOGENTRY();

    return batteryLevel == o->batteryLevel
           && batteryStatus == o->batteryStatus
           && batteryHealth == o->batteryHealth
           && batteryPlugType == o->batteryPlugType
           && batteryTemperature == o->batteryTemperature
           && batteryVoltage == o->batteryVoltage
           && states == o->states;
}

BatteryStats::BitDescription::BitDescription(const int32_t mask, const sp<String>& name)
        : PREINIT_DYNAMIC()
        , mask(mask)
        , shift(-1)
        , name(name)
        , values(NULL) {
    GLOGENTRY();
}

BatteryStats::BitDescription::BitDescription(const int32_t mask, const int32_t shift, const sp<String>& name, const sp<Blob<sp<String> > >& values)
        : PREINIT_DYNAMIC()
        , mask(mask)
        , shift(shift)
        , name(name)
        , values(values) {
    GLOGENTRY();
}

BatteryStats::HistoryPrinter::HistoryPrinter()
    : PREINIT_DYNAMIC(),
    oldState(0),
    oldStatus(-1),
    oldHealth(-1),
    oldPlug(-1),
    oldTemp(-1),
    oldVolt(-1) {
    GLOGENTRY();
}

void BatteryStats::HistoryPrinter::printNextItem(const sp<PrintWriter>& pw, const sp<HistoryItem>& rec, int64_t now) {
    GLOGENTRY();

    pw->print("  ");
    TimeUtils::formatDuration(static_cast<long long>(rec->time - now), pw, TimeUtils::HUNDRED_DAY_FIELD_LEN);
    pw->print(" ");

    if (rec->cmd == HistoryItem::CMD_START) {
        pw->println(" START");
    } else if (rec->cmd == HistoryItem::CMD_OVERFLOW) {
        pw->println(" *OVERFLOW*");
    } else {
        if (rec->batteryLevel < 10) pw->print("00");
        else if (rec->batteryLevel < 100) pw->print("0");

        pw->print(rec->batteryLevel);
        pw->print(" ");

        if (rec->states < 0x10) pw->print("0000000");
        else if (rec->states < 0x100) pw->print("000000");
        else if (rec->states < 0x1000) pw->print("00000");
        else if (rec->states < 0x10000) pw->print("0000");
        else if (rec->states < 0x100000) pw->print("000");
        else if (rec->states < 0x1000000) pw->print("00");
        else if (rec->states < 0x10000000) pw->print("0");

        pw->print(Integer::toHexString(rec->states));

        if (oldStatus != rec->batteryStatus) {
            oldStatus = rec->batteryStatus;
            pw->print(" status=");

            switch (oldStatus) {
                case BatteryManager::BATTERY_STATUS_UNKNOWN:
                    pw->print("unknown");
                    break;
                case BatteryManager::BATTERY_STATUS_CHARGING:
                    pw->print("charging");
                    break;
                case BatteryManager::BATTERY_STATUS_DISCHARGING:
                    pw->print("discharging");
                    break;
                case BatteryManager::BATTERY_STATUS_NOT_CHARGING:
                    pw->print("not-charging");
                    break;
                case BatteryManager::BATTERY_STATUS_FULL:
                    pw->print("full");
                    break;
                default:
                    pw->print(oldStatus);
                    break;
            }
        }

        if (oldHealth != rec->batteryHealth) {
            oldHealth = rec->batteryHealth;
            pw->print(" health=");

            switch (oldHealth) {
                case BatteryManager::BATTERY_HEALTH_UNKNOWN:
                    pw->print("unknown");
                    break;
                case BatteryManager::BATTERY_HEALTH_GOOD:
                    pw->print("good");
                    break;
                case BatteryManager::BATTERY_HEALTH_OVERHEAT:
                    pw->print("overheat");
                    break;
                case BatteryManager::BATTERY_HEALTH_DEAD:
                    pw->print("dead");
                    break;
                case BatteryManager::BATTERY_HEALTH_OVER_VOLTAGE:
                    pw->print("over-voltage");
                    break;
                case BatteryManager::BATTERY_HEALTH_UNSPECIFIED_FAILURE:
                    pw->print("failure");
                    break;
                default:
                    pw->print(oldHealth);
                    break;
            }
        }

        if (oldPlug != rec->batteryPlugType) {
            oldPlug = rec->batteryPlugType;
            pw->print(" plug=");

            switch (oldPlug) {
                case 0:
                    pw->print("none");
                    break;
                case BatteryManager::BATTERY_PLUGGED_AC:
                    pw->print("ac");
                    break;
                case BatteryManager::BATTERY_PLUGGED_USB:
                    pw->print("usb");
                    break;
                default:
                    pw->print(oldPlug);
                    break;
            }
        }

        if (oldTemp != rec->batteryTemperature) {
            oldTemp = rec->batteryTemperature;
            pw->print(" temp=");
            pw->print(oldTemp);
        }

        if (oldVolt != rec->batteryVoltage) {
            oldVolt = rec->batteryVoltage;
            pw->print(" volt=");
            pw->print(oldVolt);
        }

        printBitDescriptions(pw, oldState, rec->states, HISTORY_STATE_DESCRIPTIONS());
        pw->println();
    }

    oldState = rec->states;
}
