#pragma once

#include "Totem.hpp"
#include "TotemState.hpp"

class WifiConnectingState final : public TotemState
{
    // WiFi symbol configuration
    static constexpr uint8_t CENTER_X = 32; // Center of the display
    static constexpr uint8_t CENTER_Y = 32; // Center of the display
    static constexpr uint8_t DOT_RADIUS = 2; // Radius of the WiFi dot
    static constexpr uint8_t NUM_ARCS = 4; // Number of WiFi signal arcs
    static constexpr uint8_t ARC_SPACING = 5; // Spacing between arcs
    static constexpr uint8_t ARC_THICKNESS = 3; // Thickness of each arc
    static constexpr uint16_t ARC_START_ANGLE = 225; // Start angle in degrees
    static constexpr uint16_t ARC_END_ANGLE = 315; // End angle in degrees

    // Animation configuration
    static constexpr uint8_t DOT_PULSE_CYCLE = 30; // Frames for dot pulse animation
    static constexpr uint8_t ARC_ANIMATION_DELAY = 15; // Frames between each arc animation
    static constexpr uint8_t ARC_ANIMATION_CYCLE = 20; // Frames for arc fade-in

    // Colors
    static constexpr uint8_t WIFI_R = 40; // Base WiFi color
    static constexpr uint8_t WIFI_G = 100;
    static constexpr uint8_t WIFI_B = 255;

    static constexpr uint8_t TEXT_R = 255; // Text color
    static constexpr uint8_t TEXT_G = 255;
    static constexpr uint8_t TEXT_B = 0;

    uint16_t frame_count_ = 0; // Frame counter for animation

    // Calculate animation progress (0-255) for an arc
    uint8_t calculateArcProgress(const uint8_t arc_index) const
    {
        // Delay animation start for each arc based on its index
        const uint16_t arc_start_frame = arc_index * ARC_ANIMATION_DELAY;

        // If animation hasn't started yet for this arc, return 0
        if (frame_count_ < arc_start_frame)
        {
            return 0;
        }

        // Calculate progress within animation cycle
        const uint16_t arc_frame = (frame_count_ - arc_start_frame) % (ARC_ANIMATION_CYCLE * 2);

        // First half of cycle: fade in
        if (arc_frame < ARC_ANIMATION_CYCLE)
        {
            return static_cast<uint8_t>((arc_frame * 255) / ARC_ANIMATION_CYCLE);
        }
        // Second half of cycle: stay visible
        else
        {
            return 255;
        }
    }

    // Calculate dot pulse brightness (0-255)
    uint8_t calculateDotBrightness() const
    {
        const uint16_t pulse_frame = frame_count_ % DOT_PULSE_CYCLE;
        const float pulse_position = static_cast<float>(pulse_frame) / DOT_PULSE_CYCLE;

        // Sinusoidal pulse between 100 and 255
        constexpr float min_brightness = 100.0f / 255.0f;
        return static_cast<uint8_t>(((sinf(pulse_position * 2.0f * M_PI) + 1.0f) / 2.0f) *
            (255.0f - min_brightness * 255.0f) + min_brightness * 255.0f);
    }

    // Draw a filled circle
    static void drawFilledCircle(MatrixGfx& gfx, const uint8_t x, const uint8_t y, const uint8_t radius,
                                 const uint8_t r, const uint8_t g, const uint8_t b)
    {
        for (int16_t dx = -radius; dx <= radius; dx++)
        {
            for (int16_t dy = -radius; dy <= radius; dy++)
            {
                if (dx * dx + dy * dy <= radius * radius)
                {
                    gfx.drawPixel(x + dx, y + dy, r, g, b);
                }
            }
        }
    }

    // Draw an arc
    static void drawArc(MatrixGfx& gfx, const uint8_t x, const uint8_t y, const uint8_t radius, const uint8_t thickness,
                        const uint16_t start_angle, const uint16_t end_angle, const uint8_t r, const uint8_t g,
                        const uint8_t b)
    {
        // Convert angles to radians
        const float start_rad = start_angle * M_PI / 180.0f;
        const float end_rad = end_angle * M_PI / 180.0f;

        // For each potential pixel position in the bounding box
        for (int16_t dx = -radius - thickness; dx <= radius + thickness; dx++)
        {
            for (int16_t dy = -radius - thickness; dy <= radius + thickness; dy++)
            {
                // Calculate distance from center
                const float dist = sqrtf(dx * dx + dy * dy);

                // Check if within the arc's radius range
                if (dist >= radius - thickness / 2.0f && dist <= radius + thickness / 2.0f)
                {
                    // Calculate angle of this pixel
                    float angle = atan2f(dy, dx);
                    if (angle < 0) angle += 2.0f * M_PI;

                    // Check if angle is within arc's angle range
                    if (angle >= start_rad && angle <= end_rad)
                    {
                        gfx.drawPixel(x + dx, y + dy, r, g, b);
                    }
                }
            }
        }
    }

    // Draw the WiFi symbol with animation
    void drawWiFiSymbol(MatrixGfx& gfx)
    {
        const uint8_t global_brightness = getBrightness();

        // Draw each arc with animation
        for (uint8_t i = 0; i < NUM_ARCS; i++)
        {
            // Calculate radius for this arc
            const uint8_t radius = DOT_RADIUS + (i + 1) * ARC_SPACING;

            // Calculate animation progress
            const uint8_t progress = calculateArcProgress(NUM_ARCS - i - 1);

            // Calculate color with animation progress and global brightness
            const uint8_t r = static_cast<uint8_t>((static_cast<uint16_t>(WIFI_R) * progress / 255) * global_brightness
                / 255);
            const uint8_t g = static_cast<uint8_t>((static_cast<uint16_t>(WIFI_G) * progress / 255) * global_brightness
                / 255);
            const uint8_t b = static_cast<uint8_t>((static_cast<uint16_t>(WIFI_B) * progress / 255) * global_brightness
                / 255);

            // Draw the arc
            drawArc(gfx, CENTER_X, CENTER_Y, radius, ARC_THICKNESS,
                    ARC_START_ANGLE, ARC_END_ANGLE, r, g, b);
        }

        // Draw the WiFi dot with pulsing animation
        const uint8_t dot_brightness = calculateDotBrightness();
        const uint8_t r = static_cast<uint8_t>(
            (static_cast<uint16_t>(WIFI_R) * dot_brightness / 255) * global_brightness / 255);

        const uint8_t g = static_cast<uint8_t>(
            (static_cast<uint16_t>(WIFI_G) * dot_brightness / 255) * global_brightness / 255);

        const uint8_t b = static_cast<uint8_t>(
            (static_cast<uint16_t>(WIFI_B) * dot_brightness / 255) * global_brightness / 255);

        drawFilledCircle(gfx, CENTER_X, CENTER_Y, DOT_RADIUS, r, g, b);

        // Increment frame counter
        frame_count_++;

        // Reset frame counter after a complete cycle
        if (frame_count_ > NUM_ARCS * ARC_ANIMATION_DELAY + ARC_ANIMATION_CYCLE * 2)
        {
            frame_count_ = NUM_ARCS * ARC_ANIMATION_DELAY; // Reset to keep all arcs visible
        }
    }

public:
    WifiConnectingState()
    {
        // Set default render speed for smooth animation
        setRenderTick(pdMS_TO_TICKS(33)); // ~30fps
    }

    void render(MatrixGfx& gfx) override
    {
        // Clear the display
        gfx.clear();

        // Draw the WiFi symbol with animation
        drawWiFiSymbol(gfx);

        // Draw "Connecting" text at the bottom
        gfx.setCursor(3, LedMatrix::MATRIX_HEIGHT - (LedMatrix::MATRIX_HEIGHT / 3));
        gfx.print("Connecting", TEXT_R, TEXT_G, TEXT_B);

        // Update the display
        gfx.flush();
    }
};
