// Copyright (c) 2017-2022 Cloudflare, Inc.
// Licensed under the Apache 2.0 license found in the LICENSE file or at:
//     https://opensource.org/licenses/Apache-2.0

#include "nsm.h"

#if !_WIN32
#include <errno.h>
#include <sys/ioctl.h>
#include <unistd.h>
#endif

namespace workerd::api {

#if !_WIN32
namespace {
  const uint8_t NSM_IOCTL_MAGIC = 0x0A;
  const uint32_t NSM_REQUEST_MAX_SIZE = 0x1000;
  const uint32_t NSM_RESPONSE_MAX_SIZE = 0x3000;

  struct NsmMessage {
    struct iovec request;
    struct iovec response;
  };
}

kj::Array<kj::byte> NitroSecureModule::request(jsg::Lock& js, kj::Array<kj::byte> request) {
  JSG_REQUIRE(request.size() < NSM_REQUEST_MAX_SIZE, TypeError, "NSM request too large");
  struct iovec req, res;
  req.iov_base = request.begin();
  req.iov_len = request.size();

  auto response = kj::heapArray<kj::byte>(NSM_RESPONSE_MAX_SIZE);
  res.iov_base = response.begin();
  res.iov_len = response.size();
  NsmMessage message = {req, res};
  KJ_SYSCALL(ioctl(fd, _IOR(NSM_IOCTL_MAGIC, 0, NsmMessage), &message));

  return response;
}
#else
kj::Array<kj::byte> NitroSecureModule::request(jsg::Lock& js, kj::Array<kj::byte> request) {
  KJ_FAIL_REQUIRE("NitroSecureModule is not supported on Windows");
}
#endif

}  // namespace workerd::api
