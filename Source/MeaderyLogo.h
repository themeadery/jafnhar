#pragma once

#include <JuceHeader.h>

class MeaderyLogo : public juce::Component
{
public:
    MeaderyLogo()
    {
        setInterceptsMouseClicks(false, false);
    }

    void setColour(juce::Colour c)     { colour = c; repaint(); }
    void setThickness(float t)         { thickness = t; repaint(); }

    void paint(juce::Graphics& g) override
    {
        auto b = getLocalBounds().reduced(juce::roundToInt(getWidth() * 0.08f)).toFloat();
        auto cx = b.getCentreX(), cy = b.getCentreY();
        float size = juce::jmin(b.getWidth(), b.getHeight());
        float s = size / 200.0f;

        juce::Path hex;
        hex.startNewSubPath(100.0f, 20.0f);
        hex.lineTo(169.28f, 60.0f);
        hex.lineTo(169.28f, 140.0f);
        hex.lineTo(100.0f, 180.0f);
        hex.lineTo(30.72f, 140.0f);
        hex.lineTo(30.72f, 60.0f);
        hex.closeSubPath();

        hex.applyTransform(juce::AffineTransform::scale(s, s)
            .translated(cx - 100.0f * s, cy - 100.0f * s));

        g.setColour(colour);
        g.strokePath(hex, juce::PathStrokeType(thickness));
    }

private:
    juce::Colour colour { juce::Colours::grey.darker(0.4f) };
    float thickness = 8.0f;

public:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MeaderyLogo)
};
