#pragma once

#include "CoreMinimal.h"
#include "Framework/Application/IInputProcessor.h"

class FStarflightInputPreprocessor : public IInputProcessor
{
public:
    virtual ~FStarflightInputPreprocessor() override = default;

    virtual void Tick(const float DeltaTime, FSlateApplication& SlateApp, TSharedRef<ICursor> Cursor) override {}

    virtual bool HandleKeyDownEvent(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent) override;
    virtual bool HandleKeyUpEvent(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent) override;
};


