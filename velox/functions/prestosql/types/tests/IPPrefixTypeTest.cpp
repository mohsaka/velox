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
#include "velox/functions/prestosql/types/IPPrefixType.h"
#include "velox/functions/prestosql/types/tests/TypeTestBase.h"

namespace facebook::velox::test {

class IPPrefixTypeTest : public testing::Test, public TypeTestBase {
 public:
  IPPrefixTypeTest() {
    registerIPPrefixType();
  }
};

TEST_F(IPPrefixTypeTest, basic) {
  ASSERT_EQ(IPPREFIX()->name(), "IPPREFIX");
  ASSERT_TRUE(IPPREFIX()->parameters().empty());
  ASSERT_EQ(IPPREFIX()->toString(), "IPPREFIX");
  ASSERT_EQ(IPADDRESS()->kindName(), "VARBINARY");

  ASSERT_TRUE(hasType("IPPREFIX"));
  ASSERT_EQ(*getType("IPPREFIX", {}), *IPPREFIX());
}

TEST_F(IPPrefixTypeTest, serde) {
  testTypeSerde(IPPREFIX());
}
} // namespace facebook::velox::test