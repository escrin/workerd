#include <kj/async.h>
#include <kj/common.h>
#include <kj/debug.h>

#include "web-socket.h"
#include "web-worker.h"
#include <capnp/serialize-async.h>
#include <workerd/api/basics.h>
#include <workerd/api/crypto.h>
#include <workerd/api/global-scope.h>
#include <workerd/api/web-worker-api.capnp.h>
#include <workerd/util/uuid.h>

namespace workerd::api {

jsg::Ref<WebWorker> WebWorker::constructor(jsg::Lock& js,
    kj::String aUrl, jsg::Optional<Options> options) {
  kj::Own<capnp::MallocMessageBuilder> requestMessage = kj::heap<capnp::MallocMessageBuilder>();
  auto requestBuilder = requestMessage->initRoot<experimental::CreateWorkerRequest>();
  KJ_REQUIRE(aUrl.startsWith(url::OBJECT_URL_PREFIX), "unsupported script URL");

  auto id = workerd::randomUUID(nullptr);

  requestBuilder.setId(id);
  requestBuilder.setScript(KJ_REQUIRE_NONNULL(url::URL::getObjectByUrl(js, aUrl),
      "worker script not found"));
  auto optionsBuilder = requestBuilder.initOptions();
  KJ_IF_MAYBE(opts, options) {
    KJ_IF_MAYBE(name, opts->name) {
      optionsBuilder.setName(*name);
    }
    KJ_IF_MAYBE(type, opts->type) {
      auto ty = optionsBuilder.initType();
      if (*type == "module"_kj) {
        ty.setModule();
      } else if (*type == "classic"_kj) {
        ty.setClassic();
      } else {
        throw JSG_KJ_EXCEPTION(FAILED, TypeError,
            *type, " is not a valid value for WorkerOptions.type");
      }
    }
    KJ_IF_MAYBE(credentials, opts->credentials) {
      auto creds = optionsBuilder.initCredentials();
      if (*credentials == "omit"_kj) {
        creds.setOmit();
      } else if (*credentials == "same-origin"_kj) {
        creds.setSameOrigin();
      } else if (*credentials == "include"_kj) {
        creds.setInclude();
      } else {
        throw JSG_KJ_EXCEPTION(FAILED, TypeError,
            *credentials, " is not a valid value for WorkerOptions.credentials");
      }
    }
  }

  auto& context = IoContext::current();
  auto client = context.getHttpClient(IoContext::SELF_CLIENT_CHANNEL, true, nullptr, "create"_kjc);
  auto headers = kj::HttpHeaders(context.getHeaderTable());
  auto req = client->request(kj::HttpMethod::POST, "", headers);

  auto chanProm = capnp::writeMessage(*req.body, *requestMessage)
      .attach(kj::mv(req.body), kj::mv(requestMessage))
      .then([resp = kj::mv(req.response)]() mutable {
    return resp.then([](kj::HttpClient::Response&& res) {
      return capnp::readMessage(*res.body).attach(kj::mv(res.body)).then(
          [statusCode = kj::mv(res.statusCode)](auto msgReader) {
        auto resMsg = msgReader->template getRoot<api::experimental::CreateWorkerResponse>();
        if (statusCode != 201) throw KJ_EXCEPTION(FAILED, resMsg.getError());
        return resMsg.getChannel();
      });
    });
  });
  auto worker = jsg::alloc<WebWorker>(kj::mv(id), chanProm.fork());
  worker->init(js);
  return worker;
}

void WebWorker::init(jsg::Lock& js) {
  // The name of this WebWorker instance, as returned by the WebWorkerService,
  // must be registered as a named entrypoint having the effect of notifying
  // this object of a `postMessage` from the dedicated worker.
  auto& lock = IoContext::current().getCurrentLock();
  lock.addExportedHandler(kj::str("web-worker-", this->id), api::ExportedHandler {
    .postMessage = [this](auto& js, auto value) {
      if (this->terminated) return;
      auto event = jsg::alloc<api::MessageEvent>(js.v8Isolate, value);
      this->dispatchEventImpl(js, kj::mv(event));
    },
    .self = nullptr
  });
}

void WebWorker::postMessage(jsg::Lock& js,
    v8::Local<v8::Value> message, jsg::Optional<kj::Array<jsg::Value>> transfer,
    const jsg::TypeHandler<JsonWebKey>& jwkHandler,
    const jsg::TypeHandler<jsg::Ref<CryptoKey>>& keyHandler) {
  if (this->terminated) return;
  auto& context = IoContext::current();
  context.addWaitUntil(context.awaitJs(context.awaitIo(js, subrequestChannelPromise.addBranch(),
      [message = js.v8Ref(message), &jwkHandler, &keyHandler](jsg::Lock& js, auto chan) mutable {
    auto& context = IoContext::current();
    context.addWaitUntil(WebWorker::postMessageRequest(
      js, context, kj::mv(chan), "DedicatedWorkerGlobalScope"_kj,
      message.getHandle(js), nullptr, jwkHandler, keyHandler));
  })));
}

void WebWorker::terminate(jsg::Lock& js) {
  if (this->terminated) return;
  this->terminated = true;
  auto& context = IoContext::current();
  auto client = context.getHttpClient(IoContext::SELF_CLIENT_CHANNEL,
      true, nullptr, "terminate"_kjc);
  auto headers = kj::HttpHeaders(context.getHeaderTable());
  auto initialized = subrequestChannelPromise.addBranch().ignoreResult();
  auto req = client->request(kj::HttpMethod::DELETE, this->id, headers);
  context.addWaitUntil(initialized.attach(kj::mv(client)).then([req = kj::mv(req)]() mutable {
    return req.body->write(nullptr, 0)
      .attach(kj::mv(req.body))
      .then([resp = kj::mv(req.response)]() mutable {
        return resp.then([](kj::HttpClient::Response&& response) mutable {
          return response.body->readAllBytes().attach(kj::mv(response.body)).ignoreResult();
        });
      });
  }));
}

void WebWorker::reportError(jsg::Lock& js, kj::Exception&& e) {
  jsg::Value err = js.exceptionToJs(kj::cp(e));
  reportError(js, kj::mv(err));
}

void WebWorker::reportError(jsg::Lock& js, jsg::Value err) {
  if (error == nullptr) {
    auto msg = kj::str(v8::Exception::CreateMessage(js.v8Isolate, err.getHandle(js))->Get());
    error = err.addRef(js);
    dispatchEventImpl(js, jsg::alloc<ErrorEvent>(kj::mv(msg), kj::mv(err), js.v8Isolate));
  }
}

kj::Promise<void> WebWorker::postMessageRequest(jsg::Lock& js,
    IoContext& context, uint subrequestChannel, kj::StringPtr entrypointName,
    v8::Local<v8::Value> message, jsg::Optional<kj::Array<jsg::Value>> transfer,
    const jsg::TypeHandler<JsonWebKey>& jwkHandler,
    const jsg::TypeHandler<jsg::Ref<CryptoKey>>& keyHandler) {
  WebWorker::Serializer serializer(js.v8Isolate, jwkHandler, keyHandler);
  serializer.write(message);
  kj::Own<kj::Array<kj::byte>> serialized = kj::heap(serializer.release().data);

  auto client = context.getHttpClient(subrequestChannel, true, nullptr, "post_message"_kjc);

  kj::HttpHeaders headers(context.getHeaderTable());
  headers.set(kj::HttpHeaderId::CONTENT_TYPE, "application/octet-stream"_kj);
  headers.set(kj::HttpHeaderId::HOST, entrypointName);

  auto size = serialized->size();
  auto req = client->request(kj::HttpMethod::POST, "post-message", headers, size);
  return req.body->write(serialized->begin(), size)
    .attach(kj::mv(serialized), kj::mv(req.body))
    .then([resp = kj::mv(req.response)]() mutable {
      return resp.then([](kj::HttpClient::Response&& response) mutable {
        if (response.statusCode != 204) {
          KJ_DBG("postMessage failed", response.statusText); // TODO: does this error just disappear?
        }
        // Read and discard response body, otherwise we might burn the HTTP connection.
        return response.body->readAllBytes().attach(kj::mv(response.body)).ignoreResult();
    });
  }).attach(kj::mv(client));
}

v8::Maybe<bool> WebWorker::Serializer::IsHostObject(v8::Isolate* isolate,
    v8::Local<v8::Object> object) {
  return v8::Just(keyHandler.tryUnwrap(jsg::Lock::from(isolate), object) != nullptr);
};

v8::Maybe<bool> WebWorker::Serializer::WriteHostObject(v8::Isolate* isolate,
    v8::Local<v8::Object> object) {
  auto& js = jsg::Lock::from(isolate);
  KJ_IF_MAYBE(key, keyHandler.tryUnwrap(js, object)) {
    write(js.wrapString((*key)->getAlgorithmName()));
    auto jwk = api::SubtleCrypto().exportKeySync(js, kj::str("jwk"), **key).get<JsonWebKey>();
    write(jwkHandler.wrap(js, kj::mv(jwk)));
    return v8::Just(true);
  }
  return v8::Just(false);
};

v8::MaybeLocal<v8::Object> WebWorker::Deserializer::ReadHostObject(v8::Isolate* isolate) {
  auto& js = jsg::Lock::from(isolate);
  auto v8Context = js.v8Isolate->GetCurrentContext();

  kj::String alg = js.toString(jsg::check(deser.ReadValue(v8Context)));
  JsonWebKey jwk = KJ_UNWRAP_OR_RETURN(
    jwkHandler.tryUnwrap(js, jsg::check(deser.ReadValue(v8Context))),
    { v8::MaybeLocal<v8::Object>() });

  bool extractable = jwk.ext.orDefault(false);
  auto ops = jwk.key_ops.map([](auto& ops) { return ops.asPtr(); }).orDefault({});
  jsg::Ref<api::CryptoKey> key = api::SubtleCrypto()
    .importKeySync(js, "jwk"_kj, kj::mv(jwk), {.name = kj::mv(alg) }, extractable, ops);

  return v8::MaybeLocal<v8::Object>(
      v8::Local<v8::Object>::Cast(keyHandler.wrap(js, kj::mv(key))));
}

}  // namespace workerd::api
