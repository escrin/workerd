#pragma once

#include <kj/async.h>
#include <kj/common.h>

#include <workerd/api/basics.h>
#include <workerd/api/crypto.h>
#include <workerd/jsg/jsg.h>
#include <workerd/jsg/ser.h>

namespace workerd::api {

class WebWorker: public EventTarget {

public:
  WebWorker(kj::ForkedPromise<uint> subrequestChannelPromise)
    : subrequestChannelPromise(kj::mv(subrequestChannelPromise)) {}

  class Serializer final: public jsg::Serializer {
  // Wraps the jsg::Serializer implementation to add custom (API) types.
  public:
  inline explicit Serializer(v8::Isolate* isolate,
      const jsg::TypeHandler<JsonWebKey>& jwkHandler,
      const jsg::TypeHandler<jsg::Ref<CryptoKey>>& keyHandler):
    jsg::Serializer(isolate, jsg::Serializer::Options { .version = 15, .omitHeader = false }),
    keyHandler(keyHandler),
    jwkHandler(jwkHandler)
  {}

  inline ~Serializer() noexcept(true) {}  // noexcept(true) because Delegate's is noexcept

  private:
    bool HasCustomHostObject(v8::Isolate* isolate) override { return true; }
    v8::Maybe<bool> IsHostObject(v8::Isolate* isolate, v8::Local<v8::Object> object) override;
    v8::Maybe<bool> WriteHostObject(v8::Isolate* isolate, v8::Local<v8::Object> object) override;

    const jsg::TypeHandler<jsg::Ref<CryptoKey>>& keyHandler;
    const jsg::TypeHandler<JsonWebKey>& jwkHandler;
  };

  class Deserializer final: public jsg::Deserializer {
  public:
    inline explicit Deserializer(v8::Isolate* isolate, auto data,
        const jsg::TypeHandler<JsonWebKey>& jwkHandler,
        const jsg::TypeHandler<jsg::Ref<CryptoKey>>& keyHandler)
      : jsg::Deserializer(isolate, data, nullptr, nullptr, jsg::Deserializer::Options {
          .version = 15, // Use a fixed version for backward compatibility of old services.
          .readHeader = true }),
        jwkHandler(jwkHandler),
        keyHandler(keyHandler) {}

  private:
    v8::MaybeLocal<v8::Object> ReadHostObject(v8::Isolate* isolate) override;

    const jsg::TypeHandler<JsonWebKey>& jwkHandler;
    const jsg::TypeHandler<jsg::Ref<CryptoKey>>& keyHandler;
  };

  struct Options {
    jsg::Optional<kj::String> type;
    jsg::Optional<kj::String> credentials;
    jsg::Optional<kj::String> name;

    JSG_STRUCT(type, credentials, name);
    JSG_STRUCT_TS_OVERRIDE(Options {
      type?: 'classic' | 'module';
      credentials?: 'omit' | 'same-origin' | 'include';
      name?: string;
    });
  };

  static jsg::Ref<WebWorker> constructor(jsg::Lock& js,
      kj::String aUrl, jsg::Optional<Options> options);

  static kj::Promise<void> postMessageRequest(jsg::Lock& js,
      IoContext& context, uint subrequestChannel, kj::StringPtr entrypointName,
      v8::Local<v8::Value> message, jsg::Optional<kj::Array<jsg::Value>> transfer,
      const jsg::TypeHandler<JsonWebKey>& jwkHandler,
      const jsg::TypeHandler<jsg::Ref<CryptoKey>>& keyHandler);

  void postMessage(jsg::Lock& js,
      v8::Local<v8::Value> value,
      jsg::Optional<kj::Array<jsg::Value>> transfer,
      const jsg::TypeHandler<JsonWebKey>& jwkHandler,
      const jsg::TypeHandler<jsg::Ref<CryptoKey>>& keyHandler);

  JSG_RESOURCE_TYPE(WebWorker) {
    JSG_INHERIT(EventTarget);

    JSG_NESTED_TYPE(EventTarget);

    JSG_METHOD(postMessage);
  }

private:
  kj::ForkedPromise<uint> subrequestChannelPromise;

  kj::Maybe<jsg::Value> error;
  // If any error has occurred.

  void init(jsg::Lock& js, kj::Promise<kj::String> name);
  // Handles any post-construction setup. For example, registering web worker entrypoints.

  void reportError(jsg::Lock& js, kj::Exception&& e);
  void reportError(jsg::Lock& js, jsg::Value err);
  // @see WebSocket::reportError
};

#define EW_WEB_WORKER_ISOLATE_TYPES \
  api::WebWorker,                   \
  api::WebWorker::Options

}  // namespace workerd::api
