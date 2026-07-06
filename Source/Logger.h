#pragma once

#include <spdlog/spdlog.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <JuceHeader.h>

namespace logger
{

inline void init()
{
    if (spdlog::get("jafnhar") != nullptr)
        return;

    auto logDir = juce::File::getSpecialLocation(
        juce::File::userApplicationDataDirectory).getChildFile("jafnhar");
    logDir.createDirectory();
    auto logPath = logDir.getChildFile("log.txt").getFullPathName().toStdString();

    auto logger = spdlog::rotating_logger_mt("jafnhar", logPath, 1048576, 3);

#if JUCE_DEBUG
    logger->set_level(spdlog::level::trace);
#else
    logger->set_level(spdlog::level::info);
#endif

    spdlog::set_default_logger(logger);
    spdlog::flush_on(spdlog::level::info);
    spdlog::set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] %v");

    spdlog::info("Logger initialized");
}

inline juce::String getLogPath()
{
    return juce::File::getSpecialLocation(
        juce::File::userApplicationDataDirectory)
        .getChildFile("jafnhar")
        .getChildFile("log.txt")
        .getFullPathName();
}

} // namespace logger
