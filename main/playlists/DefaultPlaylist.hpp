#pragma once

#include "Playlist.hpp"

#include "patterns/AudioSpectrumPattern.hpp"
#include "patterns/FirePattern.hpp"
#include "patterns/WifiConnectingPattern.hpp"

class DefaultPlaylist final : public Playlist
{
public:
    static constexpr auto NAME = "DefaultPlaylist";

    explicit DefaultPlaylist() : Playlist(NAME)
    {
        set_total_time(60000);
        set_start_time(10000);
        add_pattern<AudioSpectrumPattern>(0, 40000, 10000);
        add_pattern<FirePattern>(10000, 30000, 10000);
        add_pattern<WifiConnectingPattern>(20000, 60000, 10000);
    }
};
