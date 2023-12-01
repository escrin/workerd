// Copyright (c) 2017-2022 Cloudflare, Inc.
// Licensed under the Apache 2.0 license found in the LICENSE file or at:
//     https://opensource.org/licenses/Apache-2.0

#pragma once

#include <fcntl.h>
#include <workerd/jsg/jsg.h>

namespace workerd::api {

// A capability to the Nitro Secure Module.
class NitroSecureModule: public jsg::Object {
public:
  explicit NitroSecureModule() {
    KJ_SYSCALL(fd = open("/dev/nsm", O_RDWR));
  }

  kj::Array<kj::byte> request(jsg::Lock& js, kj::Array<kj::byte> request);

  JSG_RESOURCE_TYPE(NitroSecureModule) {
    JSG_METHOD(request);
  }

private:
  uint32_t fd;
};

#define EW_NSM_ISOLATE_TYPES                 \
  api::NitroSecureModule
}  // namespace workerd::api
