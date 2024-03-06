#pragma once

#include <workerd/jsg/jsg.h>
#include <workerd/api/http.h>

namespace workerd::api {

// A special binding object that allows for dynamic evaluation.
class WorkerdApi: public jsg::Object {
public:
  explicit WorkerdApi(uint serviceChannel): serviceChannel(serviceChannel) {}

  jsg::Promise<jsg::Ref<Fetcher>> newWorker(jsg::Lock& js, jsg::JsValue args);

  JSG_RESOURCE_TYPE(WorkerdApi) {
    JSG_METHOD(newWorker);
  }

private:
  uint serviceChannel;
};
#define EW_WORKERD_API_ISOLATE_TYPES \
  api::WorkerdApi
} // namespace workerd::api
