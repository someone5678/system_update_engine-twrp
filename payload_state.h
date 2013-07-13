// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_PLATFORM_UPDATE_ENGINE_PAYLOAD_STATE_H__
#define CHROMEOS_PLATFORM_UPDATE_ENGINE_PAYLOAD_STATE_H__

#include <base/time.h>
#include <gtest/gtest_prod.h>  // for FRIEND_TEST

#include "update_engine/payload_state_interface.h"
#include "update_engine/prefs_interface.h"

namespace chromeos_update_engine {

class SystemState;

// Encapsulates all the payload state required for download. This includes the
// state necessary for handling multiple URLs in Omaha response, the backoff
// state, etc. All state is persisted so that we use the most recently saved
// value when resuming the update_engine process. All state is also cached in
// memory so that we ensure we always make progress based on last known good
// state even when there's any issue in reading/writing from the file system.
class PayloadState : public PayloadStateInterface {
 public:
  PayloadState();
  virtual ~PayloadState() {}

  // Initializes a payload state object using the given global system state.
  // It performs the initial loading of all persisted state into memory and
  // dumps the initial state for debugging purposes.  Note: the other methods
  // should be called only after calling Initialize on this object.
  bool Initialize(SystemState* system_state);

  // Implementation of PayloadStateInterface methods.
  virtual void SetResponse(const OmahaResponse& response);
  virtual void DownloadComplete();
  virtual void DownloadProgress(size_t count);
  virtual void UpdateResumed();
  virtual void UpdateRestarted();
  virtual void UpdateSucceeded();
  virtual void UpdateFailed(ErrorCode error);
  virtual void ResetUpdateStatus();
  virtual bool ShouldBackoffDownload();
  virtual void Rollback();
  virtual void ExpectRebootInNewVersion(const std::string& target_version_uid);

  virtual inline std::string GetResponseSignature() {
    return response_signature_;
  }

  virtual inline int GetFullPayloadAttemptNumber() {
    return full_payload_attempt_number_;
  }

  virtual inline int GetPayloadAttemptNumber() {
    return payload_attempt_number_;
  }

  virtual inline std::string GetCurrentUrl() {
    return candidate_urls_.size() ? candidate_urls_[url_index_] : "";
  }

  virtual inline uint32_t GetUrlFailureCount() {
    return url_failure_count_;
  }

  virtual inline uint32_t GetUrlSwitchCount() {
    return url_switch_count_;
  }

  virtual inline int GetNumResponsesSeen() {
    return num_responses_seen_;
  }

  virtual inline base::Time GetBackoffExpiryTime() {
    return backoff_expiry_time_;
  }

  virtual base::TimeDelta GetUpdateDuration();

  virtual base::TimeDelta GetUpdateDurationUptime();

  virtual inline uint64_t GetCurrentBytesDownloaded(DownloadSource source) {
    return source < kNumDownloadSources ? current_bytes_downloaded_[source] : 0;
  }

  virtual inline uint64_t GetTotalBytesDownloaded(DownloadSource source) {
    return source < kNumDownloadSources ? total_bytes_downloaded_[source] : 0;
  }

  virtual inline uint32_t GetNumReboots() {
    return num_reboots_;
  }

  virtual void UpdateEngineStarted();

  virtual inline std::string GetRollbackVersion() {
    return rollback_version_;
  }

 private:
  friend class PayloadStateTest;
  FRIEND_TEST(PayloadStateTest, RebootAfterUpdateFailedMetric);
  FRIEND_TEST(PayloadStateTest, RebootAfterUpdateSucceed);
  FRIEND_TEST(PayloadStateTest, RebootAfterCanceledUpdate);
  FRIEND_TEST(PayloadStateTest, UpdateSuccessWithWipedPrefs);

  // Increments the payload attempt number used for metrics.
  void IncrementPayloadAttemptNumber();

  // Increments the payload attempt number which governs the backoff behavior
  // at the time of the next update check.
  void IncrementFullPayloadAttemptNumber();

  // Advances the current URL index to the next available one. If all URLs have
  // been exhausted during the current payload download attempt (as indicated
  // by the payload attempt number), then it will increment the payload attempt
  // number and wrap around again with the first URL in the list. This also
  // updates the URL switch count, if needed.
  void IncrementUrlIndex();

  // Increments the failure count of the current URL. If the configured max
  // failure count is reached for this URL, it advances the current URL index
  // to the next URL and resets the failure count for that URL.
  void IncrementFailureCount();

  // Updates the backoff expiry time exponentially based on the current
  // payload attempt number.
  void UpdateBackoffExpiryTime();

  // Updates the value of current download source based on the current URL
  // index. If the download source is not one of the known sources, it's set
  // to kNumDownloadSources.
  void UpdateCurrentDownloadSource();

  // Updates the various metrics corresponding with the given number of bytes
  // that were downloaded recently.
  void UpdateBytesDownloaded(size_t count);

  // Reports the various metrics related to the number of bytes downloaded.
  void ReportBytesDownloadedMetrics();

  // Reports the metric related to number of URL switches.
  void ReportUpdateUrlSwitchesMetric();

  // Reports the various metrics related to rebooting during an update.
  void ReportRebootMetrics();

  // Reports the various metrics related to update duration.
  void ReportDurationMetrics();

  // Reports the metric related to the applied payload type.
  void ReportPayloadTypeMetric();

  // Reports the various metrics related to update attempts counts.
  void ReportAttemptsCountMetrics();

  // Checks if we were expecting to be running in the new version but the
  // boot into the new version failed for some reason. If that's the case, an
  // UMA metric is sent reporting the number of attempts the same applied
  // payload was attempted to reboot. This function is called by UpdateAttempter
  // every time the update engine starts and there's no reboot pending.
  void ReportFailedBootIfNeeded();

  // Resets all the persisted state values which are maintained relative to the
  // current response signature. The response signature itself is not reset.
  void ResetPersistedState();

  // Resets the appropriate state related to download sources that need to be
  // reset on a new update.
  void ResetDownloadSourcesOnNewUpdate();

  // Returns the persisted value for the given key. It also validates that
  // the value returned is non-negative. If |across_powerwash| is True,
  // get the value that will persist across a powerwash.
  int64_t GetPersistedValue(const std::string& key, bool across_powerwash);

  // Calculates the response "signature", which is basically a string composed
  // of the subset of the fields in the current response that affect the
  // behavior of the PayloadState.
  std::string CalculateResponseSignature();

  // Initializes the current response signature from the persisted state.
  void LoadResponseSignature();

  // Sets the response signature to the given value. Also persists the value
  // being set so that we resume from the save value in case of a process
  // restart.
  void SetResponseSignature(const std::string& response_signature);

  // Initializes the payload attempt number from the persisted state.
  void LoadPayloadAttemptNumber();

  // Initializes the payload attempt number for full payloads from the persisted
  // state.
  void LoadFullPayloadAttemptNumber();

  // Sets the payload attempt number to the given value. Also persists the
  // value being set so that we resume from the same value in case of a process
  // restart.
  void SetPayloadAttemptNumber(int payload_attempt_number);

  // Sets the payload attempt number for full updates to the given value. Also
  // persists the value being set so that we resume from the same value in case
  // of a process restart.
  void SetFullPayloadAttemptNumber(int payload_attempt_number);

  // Initializes the current URL index from the persisted state.
  void LoadUrlIndex();

  // Sets the current URL index to the given value. Also persists the value
  // being set so that we resume from the same value in case of a process
  // restart.
  void SetUrlIndex(uint32_t url_index);

  // Initializes the current URL's failure count from the persisted stae.
  void LoadUrlFailureCount();

  // Sets the current URL's failure count to the given value. Also persists the
  // value being set so that we resume from the same value in case of a process
  // restart.
  void SetUrlFailureCount(uint32_t url_failure_count);

  // Sets |url_switch_count_| to the given value and persists the value.
  void SetUrlSwitchCount(uint32_t url_switch_count);

  // Initializes |url_switch_count_| from the persisted stae.
  void LoadUrlSwitchCount();

  // Initializes the backoff expiry time from the persisted state.
  void LoadBackoffExpiryTime();

  // Sets the backoff expiry time to the given value. Also persists the value
  // being set so that we resume from the same value in case of a process
  // restart.
  void SetBackoffExpiryTime(const base::Time& new_time);

  // Initializes |update_timestamp_start_| from the persisted state.
  void LoadUpdateTimestampStart();

  // Sets |update_timestamp_start_| to the given value and persists the value.
  void SetUpdateTimestampStart(const base::Time& value);

  // Sets |update_timestamp_end_| to the given value. This is not persisted
  // as it happens at the end of the update process where state is deleted
  // anyway.
  void SetUpdateTimestampEnd(const base::Time& value);

  // Initializes |update_duration_uptime_| from the persisted state.
  void LoadUpdateDurationUptime();

  // Helper method used in SetUpdateDurationUptime() and
  // CalculateUpdateDurationUptime().
  void SetUpdateDurationUptimeExtended(const base::TimeDelta& value,
                                       const base::Time& timestamp,
                                       bool use_logging);

  // Sets |update_duration_uptime_| to the given value and persists
  // the value and sets |update_duration_uptime_timestamp_| to the
  // current monotonic time.
  void SetUpdateDurationUptime(const base::TimeDelta& value);

  // Adds the difference between current monotonic time and
  // |update_duration_uptime_timestamp_| to |update_duration_uptime_| and
  // sets |update_duration_uptime_timestamp_| to current monotonic time.
  void CalculateUpdateDurationUptime();

  // Returns the full key for a download source given the prefix.
  std::string GetPrefsKey(const std::string& prefix, DownloadSource source);

  // Loads the number of bytes that have been currently downloaded through the
  // previous attempts from the persisted state for the given source. It's
  // reset to 0 everytime we begin a full update and is continued from previous
  // attempt if we're resuming the update.
  void LoadCurrentBytesDownloaded(DownloadSource source);

  // Sets the number of bytes that have been currently downloaded for the
  // given source. This value is also persisted.
  void SetCurrentBytesDownloaded(DownloadSource source,
                                 uint64_t current_bytes_downloaded,
                                 bool log);

  // Loads the total number of bytes that have been downloaded (since the last
  // successful update) from the persisted state for the given source. It's
  // reset to 0 everytime we successfully apply an update and counts the bytes
  // downloaded for both successful and failed attempts since then.
  void LoadTotalBytesDownloaded(DownloadSource source);

  // Sets the total number of bytes that have been downloaded so far for the
  // given source. This value is also persisted.
  void SetTotalBytesDownloaded(DownloadSource source,
                               uint64_t total_bytes_downloaded,
                               bool log);

  // Loads the blacklisted version from our prefs file.
  void LoadRollbackVersion();

  // Blacklists this version from getting AU'd to until we receive a new update
  // response.
  void SetRollbackVersion(const std::string& rollback_version);

  // Clears any blacklisted version.
  void ResetRollbackVersion();

  inline uint32_t GetUrlIndex() {
    return url_index_;
  }

  // Computes the list of candidate URLs from the total list of payload URLs in
  // the Omaha response.
  void ComputeCandidateUrls();

  // Sets |num_responses_seen_| and persist it to disk.
  void SetNumResponsesSeen(int num_responses_seen);

  // Initializes |num_responses_seen_| from persisted state.
  void LoadNumResponsesSeen();

  // Reports metric conveying how many times updates were abandoned
  // before an update was applied.
  void ReportUpdatesAbandonedCountMetric();

  // The global state of the system.
  SystemState* system_state_;

  // Initializes |num_reboots_| from the persisted state.
  void LoadNumReboots();

  // Sets |num_reboots| for the update attempt. Also persists the
  // value being set so that we resume from the same value in case of a process
  // restart.
  void SetNumReboots(uint32_t num_reboots);

  // Checks to see if the device rebooted since the last call and if so
  // increments num_reboots.
  void UpdateNumReboots();

  // Writes the current wall-clock time to the kPrefsSystemUpdatedMarker
  // state variable.
  void CreateSystemUpdatedMarkerFile();

  // Called at program startup if the device booted into a new update.
  // The |time_to_reboot| parameter contains the (wall-clock) duration
  // from when the update successfully completed (the value written
  // into the kPrefsSystemUpdatedMarker state variable) until the device
  // was booted into the update (current wall-clock time).
  void BootedIntoUpdate(base::TimeDelta time_to_reboot);

  // Interface object with which we read/write persisted state. This must
  // be set by calling the Initialize method before calling any other method.
  PrefsInterface* prefs_;

  // Interface object with which we read/write persisted state. This must
  // be set by calling the Initialize method before calling any other method.
  // This object persists across powerwashes.
  PrefsInterface* powerwash_safe_prefs_;

  // This is the current response object from Omaha.
  OmahaResponse response_;

  // This stores a "signature" of the current response. The signature here
  // refers to a subset of the current response from Omaha.  Each update to
  // this value is persisted so we resume from the same value in case of a
  // process restart.
  std::string response_signature_;

  // The number of times we've tried to download the payload. This is
  // incremented each time we download the payload successsfully or when we
  // exhaust all failure limits for all URLs and are about to wrap around back
  // to the first URL.  Each update to this value is persisted so we resume from
  // the same value in case of a process restart.
  int payload_attempt_number_;

  // The number of times we've tried to download the payload in full. This is
  // incremented each time we download the payload in full successsfully or
  // when we exhaust all failure limits for all URLs and are about to wrap
  // around back to the first URL.  Each update to this value is persisted so
  // we resume from the same value in case of a process restart.
  int full_payload_attempt_number_;

  // The index of the current URL.  This type is different from the one in the
  // accessor methods because PrefsInterface supports only int64_t but we want
  // to provide a stronger abstraction of uint32_t.  Each update to this value
  // is persisted so we resume from the same value in case of a process
  // restart.
  int64_t url_index_;

  // The count of failures encountered in the current attempt to download using
  // the current URL (specified by url_index_).  Each update to this value is
  // persisted so we resume from the same value in case of a process restart.
  int64_t url_failure_count_;

  // The number of times we've switched URLs.
  int32_t url_switch_count_;

  // The current download source based on the current URL. This value is
  // not persisted as it can be recomputed everytime we update the URL.
  // We're storing this so as not to recompute this on every few bytes of
  // data we read from the socket.
  DownloadSource current_download_source_;

  // The number of different Omaha responses seen. Increases every time
  // a new response is seen. Resets to 0 only when the system has been
  // successfully updated.
  int num_responses_seen_;

  // The number of system reboots during an update attempt. Technically since
  // we don't go out of our way to not update it when not attempting an update,
  // also records the number of reboots before the next update attempt starts.
  uint32_t num_reboots_;

  // The timestamp until which we've to wait before attempting to download the
  // payload again, so as to backoff repeated downloads.
  base::Time backoff_expiry_time_;

  // The most recently calculated value of the update duration.
  base::TimeDelta update_duration_current_;

  // The point in time (wall-clock) that the update was started.
  base::Time update_timestamp_start_;

  // The point in time (wall-clock) that the update ended. If the update
  // is still in progress, this is set to the Epoch (e.g. 0).
  base::Time update_timestamp_end_;

  // The update duration uptime
  base::TimeDelta update_duration_uptime_;

  // The monotonic time when |update_duration_uptime_| was last set
  base::Time update_duration_uptime_timestamp_;

  // The number of bytes that have been downloaded for each source for each new
  // update attempt. If we resume an update, we'll continue from the previous
  // value, but if we get a new response or if the previous attempt failed,
  // we'll reset this to 0 to start afresh. Each update to this value is
  // persisted so we resume from the same value in case of a process restart.
  // The extra index in the array is to no-op accidental access in case the
  // return value from GetCurrentDownloadSource is used without validation.
  uint64_t current_bytes_downloaded_[kNumDownloadSources + 1];

  // The number of bytes that have been downloaded for each source since the
  // the last successful update. This is used to compute the overhead we incur.
  // Each update to this value is persisted so we resume from the same value in
  // case of a process restart.
  // The extra index in the array is to no-op accidental access in case the
  // return value from GetCurrentDownloadSource is used without validation.
  uint64_t total_bytes_downloaded_[kNumDownloadSources + 1];

  // A small timespan used when comparing wall-clock times for coping
  // with the fact that clocks drift and consequently are adjusted
  // (either forwards or backwards) via NTP.
  static const base::TimeDelta kDurationSlack;

  // The ordered list of the subset of payload URL candidates which are
  // allowed as per device policy.
  std::vector<std::string> candidate_urls_;

  // This stores a blacklisted version set as part of rollback. When we rollback
  // we store the version of the os from which we are rolling back from in order
  // to guarantee that we do not re-update to it on the next au attempt after
  // reboot.
  std::string rollback_version_;

  DISALLOW_COPY_AND_ASSIGN(PayloadState);
};

}  // namespace chromeos_update_engine

#endif  // CHROMEOS_PLATFORM_UPDATE_ENGINE_PAYLOAD_STATE_H__
