// Tencent is pleased to support the open source community by making UnLua available.
// 
// Copyright (C) 2019 THL A29 Limited, a Tencent company. All rights reserved.
//
// Licensed under the MIT License (the "License"); 
// you may not use this file except in compliance with the License. You may obtain a copy of the License at
//
// http://opensource.org/licenses/MIT
//
// Unless required by applicable law or agreed to in writing, 
// software distributed under the License is distributed on an "AS IS" BASIS, 
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. 
// See the License for the specific language governing permissions and limitations under the License.

#include "UnLuaTestCommon.h"
#include "UnLuaTestHelpers.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

struct FUnLuaTest_Issue321 : FUnLuaTestBase
{
    virtual bool InstantTest() override
    {
        return true;
    }

    virtual bool SetUp() override
    {
        FUnLuaTestBase::SetUp();

        const auto World = GetWorld();
        UnLua::PushUObject(L, World);
        lua_setglobal(L, "G_World");
        
        const char* Chunk = "\
            local ClassA = UE.UClass.Load('/Game/Tests/Regression/Issue321/A/BP_UnLuaTestActor_Issue321.BP_UnLuaTestActor_Issue321_C')\
            local ClassB = UE.UClass.Load('/Game/Tests/Regression/Issue321/B/BP_UnLuaTestActor_Issue321.BP_UnLuaTestActor_Issue321_C')\
            return ClassA ~= ClassB\
            ";
        UnLua::RunChunk(L, Chunk);
        const auto Result = (bool)lua_toboolean(L, -1);
        RUNNER_TEST_TRUE(Result);

        return true;
    }
};

IMPLEMENT_UNLUA_INSTANT_TEST(FUnLuaTest_Issue321, TEXT("UnLua.Regression.Issue321 同名蓝图加载冲突"))

#endif //WITH_DEV_AUTOMATION_TESTS
