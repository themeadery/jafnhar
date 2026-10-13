#include "LogWindow.h"
#include "Logger.h"

LogWindow::LogContent::LogContent()
{
    auto schemeBg = getLookAndFeel().findColour(juce::TextEditor::backgroundColourId);

    setOpaque(true);

    watermark.setColour(juce::Colours::white.withAlpha(0.25f));
    watermark.setThickness(14.0f);
    addAndMakeVisible(watermark);

    editor.setMultiLine(true);
    editor.setReadOnly(true);
    editor.setOpaque(false);
    editor.setFont(juce::Font(juce::FontOptions(11.0f)));
    editor.setColour(juce::TextEditor::backgroundColourId, schemeBg.withAlpha(0.6f));
    editor.setColour(juce::TextEditor::outlineColourId, juce::Colours::transparentBlack);
    addAndMakeVisible(editor);
}

void LogWindow::LogContent::paint(juce::Graphics& g)
{
    g.fillAll(getLookAndFeel().findColour(juce::TextEditor::backgroundColourId));
}

void LogWindow::LogContent::resized()
{
    editor.setBounds(getLocalBounds());

    int size = juce::jmin(getWidth(), getHeight()) * 3 / 4;
    watermark.setBounds(getLocalBounds().withSizeKeepingCentre(size, size));
}

LogWindow::LogWindow()
    : juce::DialogWindow("Log", juce::Colours::darkgrey, true, true)
{
    content = new LogContent();
    setContentOwned(content, false);
    setResizable(true, false);
    setSize(700, 400);
    centreAroundComponent(nullptr, getWidth(), getHeight());
    startTimer(500);
    setVisible(true);
}

void LogWindow::closeButtonPressed()
{
    setVisible(false);
}

void LogWindow::timerCallback()
{
    auto f = juce::File(logger::getLogPath());
    if (!f.exists())
        return;

    juce::FileInputStream stream(f);
    if (!stream.openedOk())
        return;

    auto fileSize = stream.getTotalLength();
    auto readSize = juce::jmin(8192LL, fileSize);
    stream.setPosition(fileSize - readSize);

    auto text = stream.readString().trimEnd();
    auto newlinePos = text.indexOf("\n");
    if (newlinePos > 0)
        text = text.substring(newlinePos + 1);

    if (content != nullptr)
        content->editor.setText(text);
}
