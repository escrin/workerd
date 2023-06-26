// Copyright (c) 2017-2022 Cloudflare, Inc.
// Licensed under the Apache 2.0 license found in the LICENSE file or at:
//     https://opensource.org/licenses/Apache-2.0

#pragma once

#include <workerd/jsg/jsg.h>
#include <kj/vector.h>

namespace workerd::jsg {

class Serializer: v8::ValueSerializer::Delegate {
  // Wraps the v8::ValueSerializer and v8::ValueSerializer::Delegate implementation.
public:
  struct Options {
    kj::Maybe<uint32_t> version;
    // When set, overrides the default wire format version with the one provided.
    bool omitHeader = false;
    // When set to true, the serialization header is not written to the output buffer.
  };

  struct Released {
    kj::Array<kj::byte> data;
    kj::Array<std::shared_ptr<v8::BackingStore>> sharedArrayBuffers;
    kj::Array<std::shared_ptr<v8::BackingStore>> transferedArrayBuffers;
  };

  explicit Serializer(v8::Isolate* isolate, kj::Maybe<Options> maybeOptions = nullptr);

  inline ~Serializer() noexcept(true) {}  // noexcept(true) because Delegate's is noexcept

  void write(v8::Local<v8::Value> value);

  inline void write(Value value) { write(value.getHandle(isolate)); }

  template <typename T>
  inline void write(V8Ref<T> value) { write(value.getHandle(isolate)); }

  void transfer(v8::Local<v8::ArrayBuffer> arrayBuffer);

  Released release();

private:
  // v8::ValueSerializer::Delegate implementation
  void ThrowDataCloneError(v8::Local<v8::String> message) override;

  v8::Maybe<uint32_t> GetSharedArrayBufferId(
      v8::Isolate* isolate,
      v8::Local<v8::SharedArrayBuffer> sab) override;

  kj::Vector<V8Ref<v8::SharedArrayBuffer>> sharedArrayBuffers;
  kj::Vector<V8Ref<v8::ArrayBuffer>> arrayBuffers;
  kj::Vector<std::shared_ptr<v8::BackingStore>> sharedBackingStores;
  kj::Vector<std::shared_ptr<v8::BackingStore>> backingStores;
  v8::Isolate* isolate;
  v8::ValueSerializer ser;
  bool released = false;
};

class Deserializer: v8::ValueDeserializer::Delegate {
public:
  struct Options {
    kj::Maybe<uint32_t> version;
    bool readHeader = true;
  };

  inline explicit Deserializer(
      v8::Isolate* isolate,
      auto data,
      kj::Maybe<kj::ArrayPtr<std::shared_ptr<v8::BackingStore>>> transferedArrayBuffers = nullptr,
      kj::Maybe<kj::ArrayPtr<std::shared_ptr<v8::BackingStore>>> sharedArrayBuffers = nullptr,
      kj::Maybe<Options> maybeOptions = nullptr)
      : deser(isolate, data.begin(), data.size(), this),
        isolate(isolate),
        sharedBackingStores(kj::mv(sharedArrayBuffers)) {
    init(kj::mv(transferedArrayBuffers), kj::mv(maybeOptions));
  }

  inline explicit Deserializer(
      v8::Isolate* isolate,
      Serializer::Released& released,
      kj::Maybe<Options> maybeOptions = nullptr)
      : Deserializer(
          isolate,
          released.data.asPtr(),
          released.transferedArrayBuffers.asPtr(),
          released.sharedArrayBuffers.asPtr(),
          kj::mv(maybeOptions)) {}

  ~Deserializer() noexcept(true) {}  // noexcept(true) because Delegate's is noexcept

  v8::Local<v8::Value> readValue();

  inline uint32_t getVersion() const { return deser.GetWireFormatVersion(); }

protected:
  v8::ValueDeserializer deser;

private:
  void init(
      kj::Maybe<kj::ArrayPtr<std::shared_ptr<v8::BackingStore>>> transferedArrayBuffers = nullptr,
      kj::Maybe<Options> maybeOptions = nullptr);

  v8::MaybeLocal<v8::SharedArrayBuffer> GetSharedArrayBufferFromId(
      v8::Isolate* isolate,
      uint32_t clone_id) override;

  v8::Isolate* isolate;
  kj::Maybe<kj::ArrayPtr<std::shared_ptr<v8::BackingStore>>> sharedBackingStores;
};

class SerializedBufferDisposer: public kj::ArrayDisposer {
  // Intended for use with v8::ValueSerializer data released into a kj::Array.
protected:
  void disposeImpl(void* firstElement, size_t elementSize, size_t elementCount,
                   size_t capacity, void (*destroyElement)(void*)) const override;
};
constexpr SerializedBufferDisposer SERIALIZED_BUFFER_DISPOSER;

v8::Local<v8::Value> structuredClone(
    v8::Local<v8::Value> value,
    v8::Isolate* isolate,
    kj::Maybe<kj::ArrayPtr<jsg::Value>> maybeTransfer = nullptr);

}  // namespace workerd::jsg
