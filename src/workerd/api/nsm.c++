// Copyright (c) 2017-2022 Cloudflare, Inc.
// Licensed under the Apache 2.0 license found in the LICENSE file or at:
//     https://opensource.org/licenses/Apache-2.0

#include "nsm.h"

#if __linux__
#include <sys/ioctl.h>
#include <sys/uio.h>
#include <unistd.h>
#endif

namespace workerd::api {

#if __linux__
namespace {
  const uint8_t NSM_IOCTL_MAGIC = 0x0A;
  const uint32_t NSM_REQUEST_MAX_SIZE = 0x1000;
  const uint32_t NSM_RESPONSE_MAX_SIZE = 0x3000;

  struct NsmMessage {
    struct iovec request;
    struct iovec response;
  };
}

NitroSecureModule::NitroSecureModule() {
  KJ_SYSCALL(fd = open("/dev/nsm", O_RDWR));
}

NitroSecureModule::~NitroSecureModule() {
  KJ_SYSCALL(close(fd));
}

kj::Array<kj::byte> NitroSecureModule::request(jsg::Lock& js, kj::Array<kj::byte> request) {
  JSG_REQUIRE(request.size() < NSM_REQUEST_MAX_SIZE, TypeError, "NSM request too large");
  struct iovec req, res;
  req.iov_base = request.begin();
  req.iov_len = request.size();

  auto response = kj::heapArray<kj::byte>(NSM_RESPONSE_MAX_SIZE);
  memset(response.begin(), 0, response.size());
  res.iov_base = response.begin();
  res.iov_len = response.size();
  NsmMessage message = {req, res};
  KJ_SYSCALL(ioctl(fd, _IOWR(NSM_IOCTL_MAGIC, 0, NsmMessage), &message));

  return response;
}
#else
NitroSecureModule::NitroSecureModule() {}
NitroSecureModule::~NitroSecureModule() {}

kj::Array<kj::byte> NitroSecureModule::request(jsg::Lock& js, kj::Array<kj::byte> request) {
  KJ_FAIL_REQUIRE("NitroSecureModule is not supported on this platform");
}
#endif

}  // namespace workerd::api
