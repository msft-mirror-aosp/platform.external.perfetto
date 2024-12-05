/*
 * Copyright (C) 2020 The Android Open Source Project
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

#ifndef SRC_TRACE_PROCESSOR_IMPORTERS_PROTO_TRACK_EVENT_TRACKER_H_
#define SRC_TRACE_PROCESSOR_IMPORTERS_PROTO_TRACK_EVENT_TRACKER_H_

#include <cstdint>
#include <map>
#include <optional>
#include <tuple>
#include <unordered_set>
#include <vector>

#include "perfetto/base/logging.h"
#include "src/trace_processor/importers/common/args_tracker.h"
#include "src/trace_processor/storage/trace_storage.h"
#include "src/trace_processor/types/trace_processor_context.h"

namespace perfetto::trace_processor {

// Tracks and stores tracks based on track types, ids and scopes.
class TrackEventTracker {
 public:
  // Data from TrackDescriptor proto used to reserve a track before interning it
  // with |TrackTracker|.
  struct DescriptorTrackReservation {
    // Maps to TrackDescriptor::ChildTracksOrdering proto values
    enum class ChildTracksOrdering {
      kUnknown = 0,
      kLexicographic = 1,
      kChronological = 2,
      kExplicit = 3,
    };
    struct CounterDetails {
      StringId category = kNullStringId;
      int64_t unit_multiplier = 1;
      bool is_incremental = false;
      uint32_t packet_sequence_id = 0;
      double latest_value = 0;
      StringId unit = kNullStringId;

      bool operator==(const CounterDetails& o) const {
        return std::tie(category, unit_multiplier, is_incremental,
                        packet_sequence_id, latest_value) ==
               std::tie(o.category, o.unit_multiplier, o.is_incremental,
                        o.packet_sequence_id, o.latest_value);
      }
    };

    uint64_t parent_uuid = 0;
    std::optional<uint32_t> pid;
    std::optional<uint32_t> tid;
    int64_t min_timestamp = 0;  // only set if |pid| and/or |tid| is set.
    StringId name = kNullStringId;
    bool use_separate_track = false;
    bool is_counter = false;

    // For counter tracks.
    std::optional<CounterDetails> counter_details;

    // For UI visualisation
    ChildTracksOrdering ordering = ChildTracksOrdering::kUnknown;
    std::optional<int32_t> sibling_order_rank;

    // Whether |other| is a valid descriptor for this track reservation. A track
    // should always remain nested underneath its original parent.
    bool IsForSameTrack(const DescriptorTrackReservation& other) {
      // Note that |min_timestamp|, |latest_value|, and |name| are ignored for
      // this comparison.
      return std::tie(parent_uuid, pid, tid, is_counter, counter_details) ==
             std::tie(other.parent_uuid, other.pid, other.tid, other.is_counter,
                      other.counter_details);
    }
  };
  explicit TrackEventTracker(TraceProcessorContext*);

  // Associate a TrackDescriptor track identified by the given |uuid| with a
  // given track description. This is called during tokenization. If a
  // reservation for the same |uuid| already exists, verifies that the present
  // reservation matches the new one.
  //
  // The track will be resolved to the track (see TrackTracker::InternTrack())
  // upon the first call to GetDescriptorTrack() with the same |uuid|. At this
  // time, |pid| will be resolved to a |upid| and |tid| to |utid|.
  void ReserveDescriptorTrack(uint64_t uuid, const DescriptorTrackReservation&);

  // Returns the ID of the track for the TrackDescriptor with the given |uuid|.
  // This is called during parsing. The first call to GetDescriptorTrack() for
  // each |uuid| resolves and inserts the track (and its parent tracks,
  // following the parent_uuid chain recursively) based on reservations made for
  // the |uuid|. If the track is a child track and doesn't have a name yet,
  // updates the track's name to event_name. Returns std::nullopt if no track
  // for a descriptor with this |uuid| has been reserved.
  // TODO(lalitm): this method needs to be split up and moved back to
  // TrackTracker.
  std::optional<TrackId> GetDescriptorTrack(
      uint64_t uuid,
      StringId event_name = kNullStringId,
      std::optional<uint32_t> packet_sequence_id = std::nullopt);

  // Converts the given counter value to an absolute value in the unit of the
  // counter, applying incremental delta encoding or unit multipliers as
  // necessary. If the counter uses incremental encoding, |packet_sequence_id|
  // must match the one in its track reservation. Returns std::nullopt if the
  // counter track is unknown or an invalid |packet_sequence_id| was passed.
  std::optional<double> ConvertToAbsoluteCounterValue(
      uint64_t counter_track_uuid,
      uint32_t packet_sequence_id,
      double value);

  // Returns the ID of the implicit trace-global default TrackDescriptor track.
  // TODO(lalitm): this method needs to be moved back to TrackTracker once
  // GetDescriptorTrack is moved back.
  TrackId GetOrCreateDefaultDescriptorTrack();

  // Called by ProtoTraceReader whenever incremental state is cleared on a
  // packet sequence. Resets counter values for any incremental counters of
  // the sequence identified by |packet_sequence_id|.
  void OnIncrementalStateCleared(uint32_t packet_sequence_id);

  void OnFirstPacketOnSequence(uint32_t packet_sequence_id);

  void SetRangeOfInterestStartUs(int64_t range_of_interest_start_us) {
    range_of_interest_start_us_ = range_of_interest_start_us;
  }

  std::optional<int64_t> range_of_interest_start_us() const {
    return range_of_interest_start_us_;
  }

 private:
  class ResolvedDescriptorTrack {
   public:
    enum class Scope {
      kThread,
      kProcess,
      kGlobal,
    };

    static ResolvedDescriptorTrack Process(UniquePid upid,
                                           bool is_counter,
                                           bool is_root);
    static ResolvedDescriptorTrack Thread(UniqueTid utid,
                                          bool is_counter,
                                          bool is_root,
                                          bool use_separate_track);
    static ResolvedDescriptorTrack Global(bool is_counter, bool is_root);

    Scope scope() const { return scope_; }
    bool is_counter() const { return is_counter_; }
    UniqueTid utid() const {
      PERFETTO_DCHECK(scope() == Scope::kThread);
      return utid_;
    }
    UniquePid upid() const {
      PERFETTO_DCHECK(scope() == Scope::kProcess);
      return upid_;
    }
    UniqueTid is_root_in_scope() const { return is_root_in_scope_; }
    bool use_separate_track() const { return use_separate_track_; }

   private:
    Scope scope_;
    bool is_counter_;
    bool is_root_in_scope_;
    bool use_separate_track_;

    // Only set when |scope| == |Scope::kThread|.
    UniqueTid utid_;

    // Only set when |scope| == |Scope::kProcess|.
    UniquePid upid_;
  };

  std::optional<TrackId> GetDescriptorTrackImpl(
      uint64_t uuid,
      std::optional<uint32_t> packet_sequence_id = std::nullopt);
  TrackId CreateTrackFromResolved(uint64_t uuid,
                                  std::optional<uint32_t> packet_sequence_id,
                                  const DescriptorTrackReservation&,
                                  const ResolvedDescriptorTrack&);
  std::optional<ResolvedDescriptorTrack> ResolveDescriptorTrack(
      uint64_t uuid,
      std::vector<uint64_t>* descendent_uuids);
  std::optional<ResolvedDescriptorTrack> ResolveDescriptorTrackImpl(
      uint64_t uuid,
      const DescriptorTrackReservation&,
      std::vector<uint64_t>* descendent_uuids);

  void AddTrackArgs(uint64_t uuid,
                    std::optional<uint32_t> packet_sequence_id,
                    const DescriptorTrackReservation&,
                    const ResolvedDescriptorTrack&,
                    ArgsTracker::BoundInserter&);

  static constexpr uint64_t kDefaultDescriptorTrackUuid = 0u;

  std::map<UniqueTid, TrackId> thread_tracks_;
  std::map<UniquePid, TrackId> process_tracks_;

  std::map<uint64_t /* uuid */, DescriptorTrackReservation>
      reserved_descriptor_tracks_;
  std::map<uint64_t /* uuid */, ResolvedDescriptorTrack>
      resolved_descriptor_tracks_;
  std::map<uint64_t /* uuid */, TrackId> descriptor_tracks_;

  // Stores the descriptor uuid used for the primary process/thread track
  // for the given upid / utid. Used for pid/tid reuse detection.
  std::map<UniquePid, uint64_t /*uuid*/> descriptor_uuids_by_upid_;
  std::map<UniqueTid, uint64_t /*uuid*/> descriptor_uuids_by_utid_;

  std::unordered_set<uint32_t> sequences_with_first_packet_;

  const StringId source_key_;
  const StringId source_id_key_;
  const StringId is_root_in_scope_key_;
  const StringId category_key_;
  const StringId has_first_packet_on_sequence_key_id_;
  const StringId child_ordering_key_;
  const StringId explicit_id_;
  const StringId lexicographic_id_;
  const StringId chronological_id_;
  const StringId sibling_order_rank_key_;

  const StringId descriptor_source_;

  const StringId default_descriptor_track_name_;

  std::optional<int64_t> range_of_interest_start_us_;

  TraceProcessorContext* const context_;
};

}  // namespace perfetto::trace_processor

#endif  // SRC_TRACE_PROCESSOR_IMPORTERS_PROTO_TRACK_EVENT_TRACKER_H_
