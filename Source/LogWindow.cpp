#include "LogWindow.h"
#include "Logger.h"

LogWindow::LogWindow()
    : DialogWindow("Log", juce::Colours::darkgrey, true, true)
{
    editor.setMultiLine(true);
    editor.setReadOnly(true);
    editor.setFont(juce::Font(11.0f));
    setContentOwned(&editor, false);
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

    auto content = stream.readString().trimEnd();
    auto newlinePos = content.indexOf("\n");
    if (newlinePos > 0)
        content = content.substring(newlinePos + 1);

    editor.setText(content);
}
