/*
 * Copyright (C) 2019 The Android Open Source Project
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

#ifndef INCLUDE_PERFETTO_TRACING_TRACK_H_
#define INCLUDE_PERFETTO_TRACING_TRACK_H_

#include "perfetto/base/export.h"
#include "perfetto/base/proc_utils.h"
#include "perfetto/base/thread_utils.h"
#include "perfetto/protozero/message_handle.h"
#include "perfetto/protozero/scattered_heap_buffer.h"
#include "perfetto/tracing/internal/fnv1a.h"
#include "perfetto/tracing/internal/tracing_muxer.h"
#include "perfetto/tracing/platform.h"
#include "protos/perfetto/trace/trace_packet.pbzero.h"
#include "protos/perfetto/trace/track_event/counter_descriptor.gen.h"
#include "protos/perfetto/trace/track_event/counter_descriptor.pbzero.h"
#include "protos/perfetto/trace/track_event/track_descriptor.gen.h"
#include "protos/perfetto/trace/track_event/track_descriptor.pbzero.h"

#include <stdint.h>
#include <map>
#include <mutex>

namespace perfetto {
namespace internal {
class TrackRegistry;
}
class Flow;
class TerminatingFlow;

// Track events are recorded on a timeline track, which maintains the relative
// time ordering of all events on that track. Each thread has its own default
// track (ThreadTrack), which is by default where all track events are written.
// Thread tracks are grouped under their hosting process (ProcessTrack).

// Events which aren't strictly scoped to a thread or a process, or don't
// correspond to synchronous code execution on a thread can use a custom
// track (Track, ThreadTrack or ProcessTrack). A Track object can also
// optionally be parented to a thread or a process.
//
// A track is represented by a uuid, which must be unique across the entire
// recorded trace.
//
// For example, to record an event that begins and ends on different threads,
// use a matching id to tie the begin and end events together:
//
//   TRACE_EVENT_BEGIN("category", "AsyncEvent", perfetto::Track(8086));
//   ...
//   TRACE_EVENT_END("category", perfetto::Track(8086));
//
// Tracks can also be annotated with metadata:
//
//   auto desc = track.Serialize();
//   desc.set_name("MyTrack");
//   perfetto::TrackEvent::SetTrackDescriptor(track, desc);
//
// Threads and processes can also be named in a similar way, e.g.:
//
//   auto desc = perfetto::ProcessTrack::Current().Serialize();
//   desc.mutable_process()->set_process_name("MyProcess");
//   perfetto::TrackEvent::SetTrackDescriptor(
//       perfetto::ProcessTrack::Current(), desc);
//
// The metadata remains valid between tracing sessions. To free up data for a
// track, call EraseTrackDescriptor:
//
//   perfetto::TrackEvent::EraseTrackDescriptor(track);
//
struct PERFETTO_EXPORT_COMPONENT Track {
  const uint64_t uuid;
  const uint64_t parent_uuid;
  constexpr Track() : uuid(0), parent_uuid(0) {}

  // Construct a track with identifier |id|, optionally parented under |parent|.
  // If no parent is specified, the track's parent is the current process's
  // track.
  //
  // To minimize the chances for accidental id collisions across processes, the
  // track's effective uuid is generated by xorring |id| with a random,
  // per-process cookie.
  explicit constexpr Track(uint64_t id, Track parent = MakeProcessTrack())
      : uuid(id ^ parent.uuid), parent_uuid(parent.uuid) {}

  explicit operator bool() const { return uuid; }
  void Serialize(protos::pbzero::TrackDescriptor*) const;
  protos::gen::TrackDescriptor Serialize() const;

  // Construct a global track with identifier |id|.
  //
  // Beware: the globally unique |id| should be chosen carefully to avoid
  // accidental clashes with track identifiers emitted by other producers.
  static Track Global(uint64_t id) { return Track(id, Track()); }

  // Construct a track using |ptr| as identifier.
  static Track FromPointer(const void* ptr, Track parent = MakeProcessTrack()) {
    // Using pointers as global TrackIds isn't supported as pointers are
    // per-proccess and the same pointer value can be used in different
    // processes. If you hit this check but are providing no |parent| track,
    // verify that Tracing::Initialize() was called for the current process.
    PERFETTO_DCHECK(parent.uuid != Track().uuid);

    return Track(static_cast<uint64_t>(reinterpret_cast<uintptr_t>(ptr)),
                 parent);
  }

  // Construct a track using |ptr| as identifier within thread-scope.
  // Shorthand for `Track::FromPointer(ptr, ThreadTrack::Current())`
  // Usage: TRACE_EVENT_BEGIN("...", "...", perfetto::Track::ThreadScoped(this))
  static Track ThreadScoped(
      const void* ptr,
      Track parent = MakeThreadTrack(base::GetThreadId())) {
    return Track::FromPointer(ptr, parent);
  }

 protected:
  constexpr Track(uint64_t uuid_, uint64_t parent_uuid_)
      : uuid(uuid_), parent_uuid(parent_uuid_) {}

  static Track MakeThreadTrack(base::PlatformThreadId tid) {
    // If tid were 0 here (which is an invalid tid), we would create a thread
    // track with a uuid that conflicts with the corresponding ProcessTrack.
    PERFETTO_DCHECK(tid != 0);
    return Track(static_cast<uint64_t>(tid), MakeProcessTrack());
  }

  static Track MakeProcessTrack() { return Track(process_uuid, Track()); }

  static constexpr inline uint64_t CompileTimeHash(const char* string) {
    return internal::Fnv1a(string);
  }

 private:
  friend class internal::TrackRegistry;
  friend class Flow;
  friend class TerminatingFlow;
  static uint64_t process_uuid;
};

// A process track represents events that describe the state of the entire
// application (e.g., counter events). Currently a ProcessTrack can only
// represent the current process.
struct PERFETTO_EXPORT_COMPONENT ProcessTrack : public Track {
  const base::PlatformProcessId pid;

  static ProcessTrack Current() { return ProcessTrack(); }

  void Serialize(protos::pbzero::TrackDescriptor*) const;
  protos::gen::TrackDescriptor Serialize() const;

 private:
  ProcessTrack()
      : Track(MakeProcessTrack()), pid(Platform::GetCurrentProcessId()) {}
};

// A thread track is associated with a specific thread of execution. Currently
// only threads in the current process can be referenced.
struct PERFETTO_EXPORT_COMPONENT ThreadTrack : public Track {
  const base::PlatformProcessId pid;
  const base::PlatformThreadId tid;
  bool disallow_merging_with_system_tracks = false;

  static ThreadTrack Current();

  // Represents a thread in the current process.
  static ThreadTrack ForThread(base::PlatformThreadId tid_);

  void Serialize(protos::pbzero::TrackDescriptor*) const;
  protos::gen::TrackDescriptor Serialize() const;

 private:
  explicit ThreadTrack(base::PlatformThreadId tid_,
                       bool disallow_merging_with_system_tracks_)
      : Track(MakeThreadTrack(tid_)),
        pid(ProcessTrack::Current().pid),
        tid(tid_),
        disallow_merging_with_system_tracks(
            disallow_merging_with_system_tracks_) {}
};

// A track for recording counter values with the TRACE_COUNTER macro. Counter
// tracks can optionally be given units and other metadata. See
// /protos/perfetto/trace/track_event/counter_descriptor.proto for details.
class PERFETTO_EXPORT_COMPONENT CounterTrack : public Track {
  // A random value mixed into counter track uuids to avoid collisions with
  // other types of tracks.
  static constexpr uint64_t kCounterMagic = 0xb1a4a67d7970839eul;

 public:
  using Unit = perfetto::protos::pbzero::CounterDescriptor::Unit;
  using CounterType =
      perfetto::protos::gen::CounterDescriptor::BuiltinCounterType;

  // |name| must outlive this object.
  constexpr explicit CounterTrack(const char* name,
                                  Track parent = MakeProcessTrack())
      : Track(internal::Fnv1a(name) ^ kCounterMagic, parent),
        name_(name),
        category_(nullptr) {}

  // |unit_name| is a free-form description of the unit used by this counter. It
  // must outlive this object.
  constexpr CounterTrack(const char* name,
                         const char* unit_name,
                         Track parent = MakeProcessTrack())
      : Track(internal::Fnv1a(name) ^ kCounterMagic, parent),
        name_(name),
        category_(nullptr),
        unit_name_(unit_name) {}

  constexpr CounterTrack(const char* name,
                         Unit unit,
                         Track parent = MakeProcessTrack())
      : Track(internal::Fnv1a(name) ^ kCounterMagic, parent),
        name_(name),
        category_(nullptr),
        unit_(unit) {}

  static constexpr CounterTrack Global(const char* name,
                                       const char* unit_name) {
    return CounterTrack(name, unit_name, Track());
  }

  static constexpr CounterTrack Global(const char* name, Unit unit) {
    return CounterTrack(name, unit, Track());
  }

  static constexpr CounterTrack Global(const char* name) {
    return Global(name, nullptr);
  }

  constexpr CounterTrack set_unit(Unit unit) const {
    return CounterTrack(uuid, parent_uuid, name_, category_, unit, unit_name_,
                        unit_multiplier_, is_incremental_, type_);
  }

  constexpr CounterTrack set_type(CounterType type) const {
    return CounterTrack(uuid, parent_uuid, name_, category_, unit_, unit_name_,
                        unit_multiplier_, is_incremental_, type);
  }

  constexpr CounterTrack set_unit_name(const char* unit_name) const {
    return CounterTrack(uuid, parent_uuid, name_, category_, unit_, unit_name,
                        unit_multiplier_, is_incremental_, type_);
  }

  constexpr CounterTrack set_unit_multiplier(int64_t unit_multiplier) const {
    return CounterTrack(uuid, parent_uuid, name_, category_, unit_, unit_name_,
                        unit_multiplier, is_incremental_, type_);
  }

  constexpr CounterTrack set_category(const char* category) const {
    return CounterTrack(uuid, parent_uuid, name_, category, unit_, unit_name_,
                        unit_multiplier_, is_incremental_, type_);
  }

  constexpr CounterTrack set_is_incremental(bool is_incremental = true) const {
    return CounterTrack(uuid, parent_uuid, name_, category_, unit_, unit_name_,
                        unit_multiplier_, is_incremental, type_);
  }

  constexpr bool is_incremental() const { return is_incremental_; }

  void Serialize(protos::pbzero::TrackDescriptor*) const;
  protos::gen::TrackDescriptor Serialize() const;

 private:
  constexpr CounterTrack(uint64_t uuid_,
                         uint64_t parent_uuid_,
                         const char* name,
                         const char* category,
                         Unit unit,
                         const char* unit_name,
                         int64_t unit_multiplier,
                         bool is_incremental,
                         CounterType type)
      : Track(uuid_, parent_uuid_),
        name_(name),
        category_(category),
        unit_(unit),
        unit_name_(unit_name),
        unit_multiplier_(unit_multiplier),
        is_incremental_(is_incremental),
        type_(type) {}

  const char* const name_;
  const char* const category_;
  Unit unit_ = perfetto::protos::pbzero::CounterDescriptor::UNIT_UNSPECIFIED;
  const char* const unit_name_ = nullptr;
  int64_t unit_multiplier_ = 1;
  const bool is_incremental_ = false;
  CounterType type_ =
      perfetto::protos::gen::CounterDescriptor::COUNTER_UNSPECIFIED;
};

namespace internal {

// Keeps a map of uuids to serialized track descriptors and provides a
// thread-safe way to read and write them. Each trace writer keeps a TLS set of
// the tracks it has seen (see TrackEventIncrementalState). In the common case,
// this registry is not consulted (and no locks are taken). However when a new
// track is seen, this registry is used to write either 1) the default
// descriptor for that track (see *Track::Serialize) or 2) a serialized
// descriptor stored in the registry which may have additional metadata (e.g.,
// track name).
// TODO(eseckler): Remove PERFETTO_EXPORT_COMPONENT once Chromium no longer
// calls TrackRegistry::InitializeInstance() directly.
class PERFETTO_EXPORT_COMPONENT TrackRegistry {
 public:
  using SerializedTrackDescriptor = std::string;

  TrackRegistry();
  ~TrackRegistry();

  static void InitializeInstance();
  static void ResetForTesting();
  static uint64_t ComputeProcessUuid();
  static TrackRegistry* Get() { return instance_; }

  void EraseTrack(Track);

  // Store metadata for |track| in the registry. |fill_function| is called
  // synchronously to record additional properties for the track.
  template <typename TrackType>
  void UpdateTrack(
      const TrackType& track,
      std::function<void(protos::pbzero::TrackDescriptor*)> fill_function) {
    UpdateTrackImpl(track, [&](protos::pbzero::TrackDescriptor* desc) {
      track.Serialize(desc);
      fill_function(desc);
    });
  }

  // This variant lets the user supply a serialized track descriptor directly.
  void UpdateTrack(Track, const std::string& serialized_desc);

  // If |track| exists in the registry, write out the serialized track
  // descriptor for it into |packet|. Otherwise just the ephemeral track object
  // is serialized without any additional metadata.
  template <typename TrackType>
  void SerializeTrack(
      const TrackType& track,
      protozero::MessageHandle<protos::pbzero::TracePacket> packet) {
    // If the track has extra metadata (recorded with UpdateTrack), it will be
    // found in the registry. To minimize the time the lock is held, make a copy
    // of the data held in the registry and write it outside the lock.
    std::string desc_copy;
    {
      std::lock_guard<std::mutex> lock(mutex_);
      const auto& it = tracks_.find(track.uuid);
      if (it != tracks_.end()) {
        desc_copy = it->second;
        PERFETTO_DCHECK(!desc_copy.empty());
      }
    }
    if (!desc_copy.empty()) {
      WriteTrackDescriptor(std::move(desc_copy), std::move(packet));
    } else {
      // Otherwise we just write the basic descriptor for this type of track
      // (e.g., just uuid, no name).
      track.Serialize(packet->set_track_descriptor());
    }
  }

  static void WriteTrackDescriptor(
      const SerializedTrackDescriptor& desc,
      protozero::MessageHandle<protos::pbzero::TracePacket> packet);

 private:
  void UpdateTrackImpl(
      Track,
      std::function<void(protos::pbzero::TrackDescriptor*)> fill_function);

  std::mutex mutex_;
  std::map<uint64_t /* uuid */, SerializedTrackDescriptor> tracks_;

  static TrackRegistry* instance_;
};

}  // namespace internal
}  // namespace perfetto

#endif  // INCLUDE_PERFETTO_TRACING_TRACK_H_
