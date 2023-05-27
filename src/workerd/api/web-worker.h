#pragma once

#include <kj/async.h>
#include <kj/common.h>

#include <workerd/api/basics.h>
#include <workerd/jsg/jsg.h>
#include <workerd/jsg/ser.h>

namespace workerd::api {

class WebWorker: public EventTarget {

public:
  WebWorker(kj::ForkedPromise<uint> subrequestChannelPromise)
    : subrequestChannelPromise(kj::mv(subrequestChannelPromise)) {}

  struct Options {
    jsg::Optional<kj::String> type;
    jsg::Optional<kj::String> credentials;
    jsg::Optional<kj::String> name;

    JSG_STRUCT(type, credentials, name);
    JSG_STRUCT_TS_OVERRIDE(Options {
      type: 'classic' | 'module';
      credentials: 'omit' | 'same-origin' | 'include';
      name: string;
    });
  };

  static jsg::Ref<WebWorker> constructor(jsg::Lock& js,
      kj::String aUrl, jsg::Optional<Options> options);

  static kj::Promise<void> postMessageRequest(jsg::Lock& js,
      IoContext& context, uint subrequestChannel, kj::StringPtr entrypointName,
      v8::Local<v8::Value> message, jsg::Optional<kj::Array<jsg::Value>> transfer);

  void postMessage(jsg::Lock& js,
      v8::Local<v8::Value> value,
      jsg::Optional<kj::Array<jsg::Value>> transfer);

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
