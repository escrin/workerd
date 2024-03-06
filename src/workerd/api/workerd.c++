#include "workerd.h"
#include <kj/compat/http.h>
#include <capnp/compat/json.h>
#include <workerd/util/http-util.h>

namespace workerd::api {

kj::Promise<uint> doCreateWorkerRequest(kj::Own<kj::HttpClient> client, kj::String serializedArgs) {
  auto& context = IoContext::current();
  auto headers = kj::HttpHeaders(context.getHeaderTable());
  auto req = client->request(kj::HttpMethod::POST, "http://workerd.local/workers"_kjc, headers, serializedArgs.size());
  co_await req.body->write(serializedArgs.begin(), serializedArgs.size());
  auto res = co_await req.response;
  auto resBody = co_await res.body->readAllText();
  if (res.statusCode >= 400) {
    JSG_FAIL_REQUIRE(Error, resBody);
  }
  co_return atoi(resBody.cStr());
}

jsg::Promise<jsg::Ref<Fetcher>> WorkerdApi::newWorker(jsg::Lock& js, jsg::JsValue args) {
  auto& context = IoContext::current();
  kj::String serializedArgs = args.toJson(js);
  auto client = context.getHttpClient(serviceChannel, true, kj::none, "create_worker"_kjc);
  auto promise = doCreateWorkerRequest(kj::mv(client), kj::mv(serializedArgs));
  return context.awaitIo(js, kj::mv(promise), [](jsg::Lock& js, uint chan) {
      return jsg::alloc<Fetcher>(chan, Fetcher::RequiresHostAndProtocol::NO, true);
  });
}

} // namespace workerd::api
