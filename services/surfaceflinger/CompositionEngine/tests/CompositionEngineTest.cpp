/*
 * Copyright 2018 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

// TODO(b/129481165): remove the #pragma below and fix conversion issues
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wconversion"

#include <compositionengine/CompositionRefreshArgs.h>
#include <compositionengine/LayerFECompositionState.h>
#include <compositionengine/impl/CompositionEngine.h>
#include <compositionengine/mock/Layer.h>
#include <compositionengine/mock/LayerFE.h>
#include <compositionengine/mock/Output.h>
#include <compositionengine/mock/OutputLayer.h>
#include <gtest/gtest.h>
#include <renderengine/mock/RenderEngine.h>

#include "MockHWComposer.h"
#include "TimeStats/TimeStats.h"

namespace android::compositionengine {
namespace {

using ::testing::_;
using ::testing::InSequence;
using ::testing::Ref;
using ::testing::Return;
using ::testing::ReturnRef;
using ::testing::SaveArg;
using ::testing::StrictMock;

struct CompositionEngineTest : public testing::Test {
    android::mock::HWComposer* mHwc = new StrictMock<android::mock::HWComposer>();
    renderengine::mock::RenderEngine* mRenderEngine =
            new StrictMock<renderengine::mock::RenderEngine>();
    std::shared_ptr<TimeStats> mTimeStats;

    impl::CompositionEngine mEngine;
    CompositionRefreshArgs mRefreshArgs;

    std::shared_ptr<mock::Output> mOutput1{std::make_shared<StrictMock<mock::Output>>()};
    std::shared_ptr<mock::Output> mOutput2{std::make_shared<StrictMock<mock::Output>>()};
    std::shared_ptr<mock::Output> mOutput3{std::make_shared<StrictMock<mock::Output>>()};

    std::shared_ptr<mock::Layer> mLayer1{std::make_shared<StrictMock<mock::Layer>>()};
    std::shared_ptr<mock::Layer> mLayer2{std::make_shared<StrictMock<mock::Layer>>()};
    std::shared_ptr<mock::Layer> mLayer3{std::make_shared<StrictMock<mock::Layer>>()};
    std::shared_ptr<mock::Layer> mLayer4{std::make_shared<StrictMock<mock::Layer>>()};
};

TEST_F(CompositionEngineTest, canInstantiateCompositionEngine) {
    auto engine = impl::createCompositionEngine();
    EXPECT_TRUE(engine.get() != nullptr);
}

TEST_F(CompositionEngineTest, canSetHWComposer) {
    mEngine.setHwComposer(std::unique_ptr<android::HWComposer>(mHwc));

    EXPECT_EQ(mHwc, &mEngine.getHwComposer());
}

TEST_F(CompositionEngineTest, canSetRenderEngine) {
    mEngine.setRenderEngine(std::unique_ptr<renderengine::RenderEngine>(mRenderEngine));

    EXPECT_EQ(mRenderEngine, &mEngine.getRenderEngine());
}

TEST_F(CompositionEngineTest, canSetTimeStats) {
    mEngine.setTimeStats(mTimeStats);

    EXPECT_EQ(mTimeStats.get(), &mEngine.getTimeStats());
}

/*
 * CompositionEngine::present
 */

struct CompositionEnginePresentTest : public CompositionEngineTest {
    struct CompositionEnginePartialMock : public impl::CompositionEngine {
        // These are the overridable functions CompositionEngine::present() may
        // call, and have separate test coverage.
        MOCK_METHOD1(preComposition, void(CompositionRefreshArgs&));
    };

    StrictMock<CompositionEnginePartialMock> mEngine;
};

TEST_F(CompositionEnginePresentTest, worksWithEmptyRequest) {
    // present() always calls preComposition()
    EXPECT_CALL(mEngine, preComposition(Ref(mRefreshArgs)));

    mEngine.present(mRefreshArgs);
}

TEST_F(CompositionEnginePresentTest, worksAsExpected) {
    // Expect calls to in a certain sequence
    InSequence seq;

    // present() always calls preComposition()
    EXPECT_CALL(mEngine, preComposition(Ref(mRefreshArgs)));

    // The first step in presenting is to make sure all outputs are prepared.
    EXPECT_CALL(*mOutput1, prepare(Ref(mRefreshArgs), _));
    EXPECT_CALL(*mOutput2, prepare(Ref(mRefreshArgs), _));
    EXPECT_CALL(*mOutput3, prepare(Ref(mRefreshArgs), _));

    // The next step in presenting is to make sure all outputs have the latest
    // state from the front-end (SurfaceFlinger).
    EXPECT_CALL(*mOutput1, updateLayerStateFromFE(Ref(mRefreshArgs)));
    EXPECT_CALL(*mOutput2, updateLayerStateFromFE(Ref(mRefreshArgs)));
    EXPECT_CALL(*mOutput3, updateLayerStateFromFE(Ref(mRefreshArgs)));

    // The last step is to actually present each output.
    EXPECT_CALL(*mOutput1, present(Ref(mRefreshArgs)));
    EXPECT_CALL(*mOutput2, present(Ref(mRefreshArgs)));
    EXPECT_CALL(*mOutput3, present(Ref(mRefreshArgs)));

    mRefreshArgs.outputs = {mOutput1, mOutput2, mOutput3};
    mEngine.present(mRefreshArgs);
}

/*
 * CompositionEngine::updateCursorAsync
 */

struct CompositionEngineUpdateCursorAsyncTest : public CompositionEngineTest {
public:
    CompositionEngineUpdateCursorAsyncTest() {
        EXPECT_CALL(*mOutput1, getOutputLayerCount()).WillRepeatedly(Return(0u));
        EXPECT_CALL(*mOutput1, getOutputLayerOrderedByZByIndex(_)).Times(0);

        EXPECT_CALL(*mOutput2, getOutputLayerCount()).WillRepeatedly(Return(1u));
        EXPECT_CALL(*mOutput2, getOutputLayerOrderedByZByIndex(0))
                .WillRepeatedly(Return(&mOutput2OutputLayer1));

        EXPECT_CALL(*mOutput3, getOutputLayerCount()).WillRepeatedly(Return(2u));
        EXPECT_CALL(*mOutput3, getOutputLayerOrderedByZByIndex(0))
                .WillRepeatedly(Return(&mOutput3OutputLayer1));
        EXPECT_CALL(*mOutput3, getOutputLayerOrderedByZByIndex(1))
                .WillRepeatedly(Return(&mOutput3OutputLayer2));

        EXPECT_CALL(mOutput2OutputLayer1, getLayerFE()).WillRepeatedly(ReturnRef(mOutput2Layer1FE));
        EXPECT_CALL(mOutput3OutputLayer1, getLayerFE()).WillRepeatedly(ReturnRef(mOutput3Layer1FE));
        EXPECT_CALL(mOutput3OutputLayer2, getLayerFE()).WillRepeatedly(ReturnRef(mOutput3Layer2FE));

        EXPECT_CALL(mOutput2OutputLayer1, getLayer()).WillRepeatedly(ReturnRef(mOutput2Layer1));
        EXPECT_CALL(mOutput3OutputLayer1, getLayer()).WillRepeatedly(ReturnRef(mOutput3Layer1));
        EXPECT_CALL(mOutput3OutputLayer2, getLayer()).WillRepeatedly(ReturnRef(mOutput3Layer2));

        EXPECT_CALL(mOutput2Layer1, editFEState()).WillRepeatedly(ReturnRef(mOutput2Layer1FEState));
        EXPECT_CALL(mOutput3Layer1, editFEState()).WillRepeatedly(ReturnRef(mOutput3Layer1FEState));
        EXPECT_CALL(mOutput3Layer2, editFEState()).WillRepeatedly(ReturnRef(mOutput3Layer2FEState));
    }

    StrictMock<mock::OutputLayer> mOutput2OutputLayer1;
    StrictMock<mock::OutputLayer> mOutput3OutputLayer1;
    StrictMock<mock::OutputLayer> mOutput3OutputLayer2;

    StrictMock<mock::LayerFE> mOutput2Layer1FE;
    StrictMock<mock::LayerFE> mOutput3Layer1FE;
    StrictMock<mock::LayerFE> mOutput3Layer2FE;

    StrictMock<mock::Layer> mOutput2Layer1;
    StrictMock<mock::Layer> mOutput3Layer1;
    StrictMock<mock::Layer> mOutput3Layer2;

    LayerFECompositionState mOutput2Layer1FEState;
    LayerFECompositionState mOutput3Layer1FEState;
    LayerFECompositionState mOutput3Layer2FEState;
};

TEST_F(CompositionEngineUpdateCursorAsyncTest, handlesNoOutputs) {
    mEngine.updateCursorAsync(mRefreshArgs);
}

TEST_F(CompositionEngineUpdateCursorAsyncTest, handlesNoLayersBeingCursorLayers) {
    EXPECT_CALL(mOutput2OutputLayer1, isHardwareCursor()).WillRepeatedly(Return(false));
    EXPECT_CALL(mOutput3OutputLayer1, isHardwareCursor()).WillRepeatedly(Return(false));
    EXPECT_CALL(mOutput3OutputLayer2, isHardwareCursor()).WillRepeatedly(Return(false));

    mRefreshArgs.outputs = {mOutput1, mOutput2, mOutput3};

    mEngine.updateCursorAsync(mRefreshArgs);
}

TEST_F(CompositionEngineUpdateCursorAsyncTest, handlesMultipleLayersBeingCursorLayers) {
    {
        InSequence seq;
        EXPECT_CALL(mOutput2OutputLayer1, isHardwareCursor()).WillRepeatedly(Return(true));
        EXPECT_CALL(mOutput2Layer1FE, latchCursorCompositionState(Ref(mOutput2Layer1FEState)));
        EXPECT_CALL(mOutput2OutputLayer1, writeCursorPositionToHWC());
    }

    {
        InSequence seq;
        EXPECT_CALL(mOutput3OutputLayer1, isHardwareCursor()).WillRepeatedly(Return(true));
        EXPECT_CALL(mOutput3Layer1FE, latchCursorCompositionState(Ref(mOutput3Layer1FEState)));
        EXPECT_CALL(mOutput3OutputLayer1, writeCursorPositionToHWC());
    }

    {
        InSequence seq;
        EXPECT_CALL(mOutput3OutputLayer2, isHardwareCursor()).WillRepeatedly(Return(true));
        EXPECT_CALL(mOutput3Layer2FE, latchCursorCompositionState(Ref(mOutput3Layer2FEState)));
        EXPECT_CALL(mOutput3OutputLayer2, writeCursorPositionToHWC());
    }

    mRefreshArgs.outputs = {mOutput1, mOutput2, mOutput3};

    mEngine.updateCursorAsync(mRefreshArgs);
}

/*
 * CompositionEngine::preComposition
 */

struct CompositionTestPreComposition : public CompositionEngineTest {
    CompositionTestPreComposition() {
        EXPECT_CALL(*mLayer1, getLayerFE()).WillRepeatedly(Return(mLayer1FE));
        EXPECT_CALL(*mLayer2, getLayerFE()).WillRepeatedly(Return(mLayer2FE));
        EXPECT_CALL(*mLayer3, getLayerFE()).WillRepeatedly(Return(mLayer3FE));
        // getLayerFE() can return nullptr. Ensure that this is handled.
        EXPECT_CALL(*mLayer4, getLayerFE()).WillRepeatedly(Return(nullptr));
    }

    sp<StrictMock<mock::LayerFE>> mLayer1FE{new StrictMock<mock::LayerFE>()};
    sp<StrictMock<mock::LayerFE>> mLayer2FE{new StrictMock<mock::LayerFE>()};
    sp<StrictMock<mock::LayerFE>> mLayer3FE{new StrictMock<mock::LayerFE>()};
};

TEST_F(CompositionTestPreComposition, preCompositionSetsFrameTimestamp) {
    const nsecs_t before = systemTime(SYSTEM_TIME_MONOTONIC);
    mEngine.preComposition(mRefreshArgs);
    const nsecs_t after = systemTime(SYSTEM_TIME_MONOTONIC);

    // The frame timestamp should be between the before and after timestamps
    EXPECT_GE(mEngine.getLastFrameRefreshTimestamp(), before);
    EXPECT_LE(mEngine.getLastFrameRefreshTimestamp(), after);
}

TEST_F(CompositionTestPreComposition, preCompositionInvokesLayerPreCompositionWithFrameTimestamp) {
    nsecs_t ts1 = 0;
    nsecs_t ts2 = 0;
    nsecs_t ts3 = 0;
    EXPECT_CALL(*mLayer1FE, onPreComposition(_)).WillOnce(DoAll(SaveArg<0>(&ts1), Return(false)));
    EXPECT_CALL(*mLayer2FE, onPreComposition(_)).WillOnce(DoAll(SaveArg<0>(&ts2), Return(false)));
    EXPECT_CALL(*mLayer3FE, onPreComposition(_)).WillOnce(DoAll(SaveArg<0>(&ts3), Return(false)));

    mRefreshArgs.outputs = {mOutput1};
    mRefreshArgs.layers = {mLayer1, mLayer2, mLayer3, mLayer4};

    mEngine.preComposition(mRefreshArgs);

    // Each of the onPreComposition calls should used the same refresh timestamp
    EXPECT_EQ(ts1, mEngine.getLastFrameRefreshTimestamp());
    EXPECT_EQ(ts2, mEngine.getLastFrameRefreshTimestamp());
    EXPECT_EQ(ts3, mEngine.getLastFrameRefreshTimestamp());
}

TEST_F(CompositionTestPreComposition, preCompositionDefaultsToNoUpdateNeeded) {
    EXPECT_CALL(*mLayer1FE, onPreComposition(_)).WillOnce(Return(false));
    EXPECT_CALL(*mLayer2FE, onPreComposition(_)).WillOnce(Return(false));
    EXPECT_CALL(*mLayer3FE, onPreComposition(_)).WillOnce(Return(false));

    mEngine.setNeedsAnotherUpdateForTest(true);

    mRefreshArgs.outputs = {mOutput1};
    mRefreshArgs.layers = {mLayer1, mLayer2, mLayer3, mLayer4};

    mEngine.preComposition(mRefreshArgs);

    // The call should have cleared the needsAnotherUpdate flag
    EXPECT_FALSE(mEngine.needsAnotherUpdate());
}

TEST_F(CompositionTestPreComposition,
       preCompositionSetsNeedsAnotherUpdateIfAtLeastOneLayerRequestsIt) {
    EXPECT_CALL(*mLayer1FE, onPreComposition(_)).WillOnce(Return(true));
    EXPECT_CALL(*mLayer2FE, onPreComposition(_)).WillOnce(Return(false));
    EXPECT_CALL(*mLayer3FE, onPreComposition(_)).WillOnce(Return(false));

    mRefreshArgs.outputs = {mOutput1};
    mRefreshArgs.layers = {mLayer1, mLayer2, mLayer3, mLayer4};

    mEngine.preComposition(mRefreshArgs);

    EXPECT_TRUE(mEngine.needsAnotherUpdate());
}

} // namespace
} // namespace android::compositionengine

// TODO(b/129481165): remove the #pragma below and fix conversion issues
#pragma clang diagnostic pop // ignored "-Wconversion"
