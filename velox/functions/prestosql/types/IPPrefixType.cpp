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

#include <folly/IPAddress.h>
#include <folly/small_vector.h>

#include "velox/expression/CastExpr.h"
#include "velox/functions/prestosql/types/IPAddressType.h"
#include "velox/functions/prestosql/types/IPPrefixType.h"

static constexpr int kIPAddressBytes = 16;
static constexpr int kIPPrefixBytes = 17;
static constexpr uint8_t kIPV4Bits = 32;
static constexpr uint8_t kIPV6Bits = 128;


namespace facebook::velox {

namespace {

class IPPrefixCastOperator : public exec::CastOperator {
 public:
  bool isSupportedFromType(const TypePtr& other) const override {
    switch (other->kind()) {
      case TypeKind::VARCHAR:
        return true;
      case TypeKind::HUGEINT:
        if (isIPAddressType(other)) {
          return true;
        }
      default:
        return false;
    }
  }

  bool isSupportedToType(const TypePtr& other) const override {
    switch (other->kind()) {
      case TypeKind::VARCHAR:
        return true;
      case TypeKind::HUGEINT:
        if (isIPAddressType(other)) {
          return true;
        }
      default:
        return false;
    }
  }

  void castTo(
      const BaseVector& input,
      exec::EvalCtx& context,
      const SelectivityVector& rows,
      const TypePtr& resultType,
      VectorPtr& result) const override {
    context.ensureWritable(rows, resultType, result);

    if (input.typeKind() == TypeKind::VARCHAR) {
      castFromString(input, context, rows, *result);
    } else {
      VELOX_NYI(
          "Cast from {} to IPPrefix not yet supported",
          input.type()->toString());
    }
  }

  void castFrom(
      const BaseVector& input,
      exec::EvalCtx& context,
      const SelectivityVector& rows,
      const TypePtr& resultType,
      VectorPtr& result) const override {
    context.ensureWritable(rows, resultType, result);

    if (resultType->kind() == TypeKind::VARCHAR) {
      castToString(input, context, rows, *result);
    } else {
      VELOX_NYI(
          "Cast from IPPrefix to {} not yet supported", resultType->toString());
    }
  }

 private:
  static void castToString(
      const BaseVector& input,
      exec::EvalCtx& context,
      const SelectivityVector& rows,
      BaseVector& result) {
    auto* flatResult = result.as<FlatVector<StringView>>();
    //const auto* ipaddresses = input.as<RowVector>();
    const auto* ipaddresses = input.as<SimpleVector<StringView>>();

    context.applyToSelectedNoThrow(rows, [&](auto row) {
      const auto intAddr = ipaddresses->valueAt(row);
      folly::ByteArray16 addrBytes;

      memcpy(&addrBytes, intAddr.data(), kIPAddressBytes);
      folly::IPAddressV6 v6Addr(addrBytes);

      exec::StringWriter<false> resultWriter(flatResult, row);
      if (v6Addr.isIPv4Mapped()) {
        resultWriter.append(fmt::format(
            "{}/{}",
            v6Addr.createIPv4().str(),
            (uint8_t)intAddr.data()[kIPAddressBytes]));
      } else {
        resultWriter.append(fmt::format(
            "{}/{}", v6Addr.str(), (uint8_t)intAddr.data()[kIPAddressBytes]));
      }
      resultWriter.finalize();
    });
  }

  static folly::small_vector<folly::StringPiece, 2> splitIpSlashCidr(
      const folly::StringPiece& ipSlashCidr) {
    folly::small_vector<folly::StringPiece, 2> vec;
    folly::split('/', ipSlashCidr, vec);
    return vec;
  }

  static void castFromString(
      const BaseVector& input,
      exec::EvalCtx& context,
      const SelectivityVector& rows,
      BaseVector& result) {
    int rowIndex = 0;
    int128_t intAddr;
    auto* rowResult = result.as<RowVector>();
    const auto* ipAddressStrings = input.as<SimpleVector<StringView>>();

    auto ipNulls = allocateNulls(input.size(), context.pool(), bits::kNull);
    auto ip = std::make_shared<FlatVector<int128_t>>(context.pool(), HUGEINT(), ipNulls, input.size(), nullptr, std::vector<BufferPtr>{});

    auto prefixNulls = allocateNulls(input.size(), context.pool(), bits::kNull);
    auto prefix = std::make_shared<FlatVector<int8_t>>(context.pool(), TINYINT(), prefixNulls, input.size(), nullptr, std::vector<BufferPtr>{});

    RowVectorPtr rowResultVector = std::make_shared<RowVector>(context.pool(), IPPREFIX(), nullptr, input.size(), std::vector<VectorPtr>{ip, prefix});

    context.applyToSelectedNoThrow(rows, [&](auto row) {
      auto ipAddressString = ipAddressStrings->valueAt(row);

      // Folly allows for creation of networks without a "/" so check to make
      // sure that we have one.
      if (ipAddressString.str().find('/') == std::string::npos) {
        context.setStatus(
            row,
            threadSkipErrorDetails()
                ? Status::UserError()
                : Status::UserError(
                      "Invalid CIDR IP address specified. Expected IP/PREFIX format, got '{}'",
                      ipAddressString.str()));
        return;
      }

      folly::ByteArray16 addrBytes;
      auto const maybeNet =
          folly::IPAddress::tryCreateNetwork(ipAddressString, -1, false);

      if (maybeNet.hasError()) {
        if (threadSkipErrorDetails()) {
          context.setStatus(row, Status::UserError());
        } else {
          switch (maybeNet.error()) {
            case folly::CIDRNetworkError::INVALID_DEFAULT_CIDR:
              context.setStatus(
                  row, Status::UserError("defaultCidr must be <= UINT8_MAX"));
              break;
            case folly::CIDRNetworkError::INVALID_IP_SLASH_CIDR:
              context.setStatus(
                  row,
                  Status::UserError(
                      "Invalid CIDR IP address specified. Expected IP/PREFIX format, got '{}'",
                      ipAddressString.str()));
              break;
            case folly::CIDRNetworkError::INVALID_IP: {
              auto const vec = splitIpSlashCidr(ipAddressString);
              context.setStatus(
                  row,
                  Status::UserError(
                      "Invalid IP address '{}'",
                      vec.size() > 0 ? vec.at(0) : ""));
              break;
            }
            case folly::CIDRNetworkError::INVALID_CIDR: {
              auto const vec = splitIpSlashCidr(ipAddressString);
              context.setStatus(
                  row,
                  Status::UserError(
                      "Mask value '{}' not a valid mask",
                      vec.size() > 1 ? vec.at(1) : ""));
              break;
            }
            case folly::CIDRNetworkError::CIDR_MISMATCH: {
              auto const vec = splitIpSlashCidr(ipAddressString);
              auto const subnet =
                  folly::IPAddress::tryFromString(vec.at(0)).value();
              context.setStatus(
                  row,
                  Status::UserError(
                      "CIDR value '{}' is > network bit count '{}'",
                      vec.size() == 2
                          ? vec.at(1)
                          : folly::to<std::string>(
                                subnet.isV4() ? kIPV4Bits : kIPV6Bits),
                      subnet.bitCount()));
              break;
            }
            default:
              context.setStatus(row, Status::UserError());
              break;
          }
        }
        return;
      }

      auto net = maybeNet.value();
      if (net.first.isIPv4Mapped() || net.first.isV4()) {
        if (net.second > kIPV4Bits) {
          context.setStatus(
              row,
              threadSkipErrorDetails()
                  ? Status::UserError()
                  : Status::UserError(
                        "CIDR value '{}' is > network bit count '{}'",
                        net.second,
                        kIPV4Bits));
          return;
        }
        addrBytes = folly::IPAddress::createIPv4(net.first)
                        .mask(net.second)
                        .createIPv6()
                        .toByteArray();
      } else {
        if (net.second > kIPV6Bits) {
          context.setStatus(
              row,
              threadSkipErrorDetails()
                  ? Status::UserError()
                  : Status::UserError(
                        "CIDR value '{}' is > network bit count '{}'",
                        net.second,
                        kIPV6Bits));
          return;
        }
        addrBytes = folly::IPAddress::createIPv6(net.first)
                        .mask(net.second)
                        .toByteArray();
      }

      std::reverse(addrBytes.begin(), addrBytes.end());
      memcpy(&intAddr, &addrBytes, kIPAddressBytes);
      ip->set(rowIndex, intAddr);
      prefix->set(rowIndex, net.second);
      rowIndex++;
    });

    result = rowResultVector;
  }
};

class IPPrefixTypeFactories : public CustomTypeFactories {
 public:
  TypePtr getType() const override {
    return IPPrefixType::get();
  }

  exec::CastOperatorPtr getCastOperator() const override {
    return std::make_shared<IPPrefixCastOperator>();
  }
};

} // namespace

void registerIPPrefixType() {
  registerCustomType(
      "ipprefix", std::make_unique<const IPPrefixTypeFactories>());
}

} // namespace facebook::velox