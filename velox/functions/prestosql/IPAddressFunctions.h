/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#pragma once

#include "velox/functions/Macros.h"
#include "velox/functions/Registerer.h"
#include "velox/functions/lib/string/StringImpl.h"
#include "velox/functions/prestosql/types/IPPrefixType.h"

namespace facebook::velox::functions {

inline bool isIPV4(int128_t ip) {
  int128_t ipV4 = 0x0000FFFF00000000;
  uint128_t mask = 0xFFFFFFFFFFFFFFFF;
  mask = (mask << 64) | 0xFFFFFFFF00000000;
  return (ip & mask) == ipV4;
}

template <typename T>
struct IPPrefixFunction {
  VELOX_DEFINE_FUNCTION_TYPES(T);

  FOLLY_ALWAYS_INLINE void call(
      out_type<TheIPPrefix>& result,
      const arg_type<IPAddress>& ip,
      const arg_type<int64_t> prefixBits) {
    // Presto stores prefixBits in one signed byte. Cast to unsigned
    uint8_t prefix = (uint8_t)prefixBits;
    folly::ByteArray16 addrBytes;
    memcpy(&addrBytes, &ip, 16);
    bigEndianByteArray(addrBytes);

    // All IPs are stored as V6
    folly::IPAddressV6 v6Addr(addrBytes);

    // For return
    folly::ByteArray16 canonicalBytes;
    int128_t canonicalAddrInt;

    if (v6Addr.isIPv4Mapped()) {
      canonicalBytes =
          v6Addr.createIPv4().mask(prefix).createIPv6().toByteArray();
    } else {
      canonicalBytes = v6Addr.mask(prefix).toByteArray();
    }
    bigEndianByteArray(canonicalBytes);
    memcpy(&canonicalAddrInt, &canonicalBytes, 16);

    result = std::make_shared<IPPrefix>(canonicalAddrInt, prefix);
  }

  FOLLY_ALWAYS_INLINE void call(
      out_type<TheIPPrefix>& result,
      const arg_type<Varchar>& ip,
      const arg_type<int64_t> prefixBits) {
    int128_t intAddr;
    folly::IPAddress addr(ip);
    auto addrBytes = folly::IPAddress::createIPv6(addr).toByteArray();

    bigEndianByteArray(addrBytes);
    memcpy(&intAddr, &addrBytes, 16);

    call(result, intAddr, prefixBits);
  }
};

template <typename T>
struct IPSubnetMinFunction {
  VELOX_DEFINE_FUNCTION_TYPES(T);

  FOLLY_ALWAYS_INLINE void call(
      out_type<IPAddress>& result,
      const arg_type<TheIPPrefix>& ipPrefix) {
    // IPPrefix type should store the smallest(canonical) IP already
    memcpy(&result, &ipPrefix->ip, 16);
  }
};

inline int128_t getIPSubnetMax(int128_t ip, uint8_t prefix) {
  uint128_t mask = 1;
  int128_t result;
  memcpy(&result, &ip, 16);

  if (isIPV4(ip)) {
    result |= (mask << (32 - prefix)) - 1;
  } else {
    // Special case: Overflow to all 0 subtracting 1 does not work.
    if (prefix == 0) {
      result = -1;
    } else {
      result |= (mask << (128 - prefix)) - 1;
    }
  }
  return result;
}

template <typename T>
struct IPSubnetMaxFunction {
  VELOX_DEFINE_FUNCTION_TYPES(T);

  FOLLY_ALWAYS_INLINE void call(
      out_type<IPAddress>& result,
      const arg_type<TheIPPrefix>& ipPrefix) {
    result = getIPSubnetMax(ipPrefix->ip, (uint8_t)ipPrefix->prefix);
  }
};

template <typename T>
struct IPSubnetRangeFunction {
  VELOX_DEFINE_FUNCTION_TYPES(T);

  FOLLY_ALWAYS_INLINE void call(
      out_type<Array<IPAddress>>& result,
      const arg_type<TheIPPrefix>& ipPrefix) {
    result.push_back(ipPrefix->ip);
    result.push_back(getIPSubnetMax(ipPrefix->ip, (uint8_t)ipPrefix->prefix));
  }
};

template <typename T>
struct IPSubnetOfFunction {
  VELOX_DEFINE_FUNCTION_TYPES(T);
  FOLLY_ALWAYS_INLINE void call(
      out_type<bool>& result,
      const arg_type<TheIPPrefix>& ipPrefix,
      const arg_type<IPAddress>& ip) {
    uint128_t mask = 1;
    uint8_t prefix = (uint8_t)ipPrefix->prefix;
    int128_t checkIP = ip;

    if (isIPV4(ipPrefix->ip)) {
      checkIP &= ((mask << (32 - prefix)) - 1) ^ -1;
    } else {
      // Special case: Overflow to all 0 subtracting 1 does not work.
      if (prefix == 0) {
        checkIP = 0;
      } else {
        checkIP &= ((mask << (128 - prefix)) - 1) ^ -1;
      }
    }
    result = (ipPrefix->ip == checkIP);
  }

  FOLLY_ALWAYS_INLINE void call(
      out_type<bool>& result,
      const arg_type<TheIPPrefix>& ipPrefix,
      const arg_type<TheIPPrefix>& ipPrefix2) {
    call(result, ipPrefix, ipPrefix2->ip);
    result = result && (ipPrefix2->prefix >= ipPrefix->prefix);
  }
};

void registerIPAddressFunctions(const std::string& prefix) {
  registerIPAddressType();
  registerIPPrefixType();
  registerFunction<IPPrefixFunction, TheIPPrefix, IPAddress, int64_t>(
      {prefix + "ip_prefix"});
  registerFunction<IPPrefixFunction, TheIPPrefix, Varchar, int64_t>(
      {prefix + "ip_prefix"});
  registerFunction<IPSubnetMinFunction, IPAddress, TheIPPrefix>(
      {prefix + "ip_subnet_min"});
  registerFunction<IPSubnetMaxFunction, IPAddress, TheIPPrefix>(
      {prefix + "ip_subnet_max"});
  registerFunction<IPSubnetRangeFunction, Array<IPAddress>, TheIPPrefix>(
      {prefix + "ip_subnet_range"});
  registerFunction<IPSubnetOfFunction, bool, TheIPPrefix, IPAddress>(
      {prefix + "is_subnet_of"});
  registerFunction<IPSubnetOfFunction, bool, TheIPPrefix, TheIPPrefix>(
      {prefix + "is_subnet_of"});
}

} // namespace facebook::velox::functions
