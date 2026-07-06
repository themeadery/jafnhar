#pragma once

#include <JuceHeader.h>

//==============================================================================
/**
    Fire-and-forget telemetry ping.
    Called once at app launch — sends OS, version, locale to the telemetry server.
    Runs on a detached thread with a 5-second timeout. Never blocks startup.
*/
struct Telemetry
{
    static void sendPing (const juce::String& installId)
    {
        std::thread([installId]
        {
            auto os      = juce::SystemStats::getOperatingSystemName();
            auto version = juce::String (ProjectInfo::versionString);
            auto locale  = juce::SystemStats::getUserLanguage();

            juce::var json (new juce::DynamicObject());
            json.getDynamicObject()->setProperty ("install_id",  installId);
            json.getDynamicObject()->setProperty ("os",          os);
            json.getDynamicObject()->setProperty ("app_version", version);
            if (locale.isNotEmpty())
                json.getDynamicObject()->setProperty ("locale", locale);

            auto body = juce::JSON::toString (json);

            juce::StringPairArray responseHeaders;
            auto options = juce::URL::InputStreamOptions (juce::URL::ParameterHandling::inPostData)
                               .withExtraHeaders ("Content-Type: application/json")
                               .withConnectionTimeoutMs (5000)
                               .withResponseHeaders (&responseHeaders);

            juce::URL url ("https://www.themeadery.buzz/api/telemetry");
            url = url.withPOSTData (body);
            url.createInputStream (options);
        }).detach();
    }
};
