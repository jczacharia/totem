#pragma once


#include <algorithm>
#include <array>
#include <cmath>
#include <memory>
#include <utility>

#include "esp_log.h"
#include "driver/gpio.h"
#include "esp_private/periph_ctrl.h"
#include "freertos/FreeRTOS.h"
#include "hal/i2s_hal.h"
#include "hal/i2s_ll.h"
#include "hal/i2s_types.h"
#include "util/Singleton.hpp"
#include "util/util.hpp"
#include "rom/lldesc.h"
#include "soc/gpio_sig_map.h"

class LedMatrix final : public util::Singleton<LedMatrix>
{
public:
    static constexpr uint8_t MATRIX_WIDTH = 64;
    static constexpr uint8_t MATRIX_HEIGHT = 64;
    static constexpr uint16_t MATRIX_SIZE = MATRIX_WIDTH * MATRIX_HEIGHT;
    static constexpr uint16_t MATRIX_BUFFER_SIZE = MATRIX_SIZE * sizeof(uint32_t);

private:
    static constexpr auto TAG = "LedMatrix";
    friend class util::Singleton<LedMatrix>;

    static constexpr uint8_t MATRIX_ROWS_PER_FRAME = MATRIX_HEIGHT / 2;
    static constexpr uint8_t MATRIX_PIXELS_PER_ROW = MATRIX_WIDTH;
    static constexpr uint8_t MATRIX_COLOR_DEPTH = 8;

    static constexpr uint32_t PIN_R1 = 27;
    static constexpr uint32_t PIN_G1 = 26;
    static constexpr uint32_t PIN_B1 = 21;
    static constexpr uint32_t PIN_R2 = 12;
    static constexpr uint32_t PIN_G2 = 25;
    static constexpr uint32_t PIN_B2 = 19;
    static constexpr uint32_t PIN_A = 22;
    static constexpr uint32_t PIN_B = 18;
    static constexpr uint32_t PIN_C = 33;
    static constexpr uint32_t PIN_D = 13;
    static constexpr uint32_t PIN_E = 5;
    static constexpr uint32_t PIN_LAT = 2;
    static constexpr uint32_t PIN_OE = 4;
    static constexpr uint32_t PIN_CLK = 0;

    static constexpr std::array<uint32_t, 13> pinsArr = {
        PIN_R1, PIN_G1, PIN_B1, PIN_R2, PIN_G2,
        PIN_B2, PIN_LAT, PIN_OE, PIN_A, PIN_B,
        PIN_C, PIN_D, PIN_E
    };

    static constexpr uint16_t BIT_R1 = 1 << 0;
    static constexpr uint16_t BIT_G1 = 1 << 1;
    static constexpr uint16_t BIT_B1 = 1 << 2;
    static constexpr uint16_t BIT_R2 = 1 << 3;
    static constexpr uint16_t BIT_G2 = 1 << 4;
    static constexpr uint16_t BIT_B2 = 1 << 5;
    static constexpr uint16_t BIT_A = 1 << 8;
    static constexpr uint16_t BIT_B = 1 << 9;
    static constexpr uint16_t BIT_C = 1 << 10;
    static constexpr uint16_t BIT_D = 1 << 11;
    static constexpr uint16_t BIT_E = 1 << 12;
    static constexpr uint16_t BIT_LAT = 1 << 6;
    static constexpr uint16_t BIT_OE = 1 << 7;
    static constexpr uint16_t BIT_CLK = 1 << 13;

    static constexpr uint16_t BITMASK_RGB1 = BIT_R1 | BIT_G1 | BIT_B1;
    static constexpr uint16_t BITMASK_RGB2 = BIT_R2 | BIT_G2 | BIT_B2;
    static constexpr uint16_t BITMASK_RGB1_RBG2 = BITMASK_RGB1 | BITMASK_RGB2;
    static constexpr uint16_t BITMASK_ABCDE = BIT_A | BIT_B | BIT_C | BIT_D | BIT_E;
    static constexpr int BITS_ABCDE_OFFSET = 8;

    static constexpr uint16_t DRAM_ATTR lumTbl[] = {
        0, 27, 56, 84, 113, 141, 170, 198, 227, 255, 284, 312, 340, 369,
        397, 426, 454, 483, 511, 540, 568, 597, 626, 657, 688, 720, 754, 788,
        824, 860, 898, 936, 976, 1017, 1059, 1102, 1146, 1191, 1238, 1286, 1335, 1385,
        1436, 1489, 1543, 1598, 1655, 1713, 1772, 1833, 1895, 1958, 2023, 2089, 2156, 2225,
        2296, 2368, 2441, 2516, 2592, 2670, 2750, 2831, 2914, 2998, 3084, 3171, 3260, 3351,
        3443, 3537, 3633, 3731, 3830, 3931, 4034, 4138, 4245, 4353, 4463, 4574, 4688, 4803,
        4921, 5040, 5161, 5284, 5409, 5536, 5665, 5796, 5929, 6064, 6201, 6340, 6482, 6625,
        6770, 6917, 7067, 7219, 7372, 7528, 7687, 7847, 8010, 8174, 8341, 8511, 8682, 8856,
        9032, 9211, 9392, 9575, 9761, 9949, 10139, 10332, 10527, 10725, 10925, 11127, 11332, 11540,
        11750, 11963, 12178, 12395, 12616, 12839, 13064, 13292, 13523, 13757, 13993, 14231, 14473, 14717,
        14964, 15214, 15466, 15722, 15980, 16240, 16504, 16771, 17040, 17312, 17587, 17865, 18146, 18430,
        18717, 19006, 19299, 19595, 19894, 20195, 20500, 20808, 21119, 21433, 21750, 22070, 22393, 22720,
        23049, 23382, 23718, 24057, 24400, 24745, 25094, 25446, 25802, 26160, 26522, 26888, 27256, 27628,
        28004, 28382, 28765, 29150, 29539, 29932, 30328, 30727, 31130, 31536, 31946, 32360, 32777, 33197,
        33622, 34049, 34481, 34916, 35354, 35797, 36243, 36692, 37146, 37603, 38064, 38528, 38996, 39469,
        39945, 40424, 40908, 41395, 41886, 42382, 42881, 43383, 43890, 44401, 44916, 45434, 45957, 46484,
        47014, 47549, 48088, 48630, 49177, 49728, 50283, 50842, 51406, 51973, 52545, 53120, 53700, 54284,
        54873, 55465, 56062, 56663, 57269, 57878, 58492, 59111, 59733, 60360, 60992, 61627, 62268, 62912,
        63561, 64215, 64873, 65535
    };

    static constexpr uint8_t DRAM_ATTR gammaTbl[] = {
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1,
        1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 3, 3, 3, 3, 4, 4,
        4, 4, 5, 5, 5, 5, 6, 6, 6, 7, 7, 7, 8, 8, 8, 9,
        9, 9, 10, 10, 11, 11, 11, 12, 12, 13, 13, 14, 14, 15, 15, 16,
        16, 17, 17, 18, 18, 19, 19, 20, 20, 21, 21, 22, 23, 23, 24, 24,
        25, 26, 26, 27, 28, 28, 29, 30, 30, 31, 32, 32, 33, 34, 35, 35,
        36, 37, 38, 38, 39, 40, 41, 42, 42, 43, 44, 45, 46, 47, 47, 48,
        49, 50, 51, 52, 53, 54, 55, 56, 56, 57, 58, 59, 60, 61, 62, 63,
        64, 65, 66, 67, 68, 69, 70, 71, 73, 74, 75, 76, 77, 78, 79, 80,
        81, 82, 84, 85, 86, 87, 88, 89, 91, 92, 93, 94, 95, 97, 98, 99,
        100, 102, 103, 104, 105, 107, 108, 109, 111, 112, 113, 115, 116, 117, 119, 120,
        121, 123, 124, 126, 127, 128, 130, 131, 133, 134, 136, 137, 139, 140, 142, 143,
        145, 146, 148, 149, 151, 152, 154, 155, 157, 158, 160, 162, 163, 165, 166, 168,
        170, 171, 173, 175, 176, 178, 180, 181, 183, 185, 186, 188, 190, 192, 193, 195,
        197, 199, 200, 202, 204, 206, 207, 209, 211, 213, 215, 217, 218, 220, 222, 224,
        226, 228, 230, 232, 233, 235, 237, 239, 241, 243, 245, 247, 249, 251, 253, 255,
    };

    lldesc_t* dma_desc = nullptr;

    volatile uint16_t* dmaDescAt(const size_t idx) const
    {
        return reinterpret_cast<volatile uint16_t*>(const_cast<uint8_t*>(dma_desc[idx].buf));
    }

    static void gpioInit(uint32_t pin)
    {
        esp_rom_gpio_pad_select_gpio(pin);
        gpio_set_direction(static_cast<gpio_num_t>(pin), GPIO_MODE_OUTPUT);
        gpio_set_drive_capability(static_cast<gpio_num_t>(pin), static_cast<gpio_drive_cap_t>(3));
    }

    [[nodiscard]] static int xCoord(const int x_coord) noexcept
    {
        return x_coord & 1U ? x_coord - 1 : x_coord + 1;
    }

    LedMatrix()
    {
        ESP_LOGI(TAG, "Initializing...");

        constexpr periph_module_t i2s_mod = PERIPH_I2S0_MODULE;
        periph_module_reset(i2s_mod);
        periph_module_enable(i2s_mod);

        for (size_t i = 0; i < pinsArr.size(); i++)
        {
            constexpr int iomux_signal_base = I2S0O_DATA_OUT8_IDX;
            gpioInit(pinsArr[i]);
            esp_rom_gpio_connect_out_signal(pinsArr[i], iomux_signal_base + i, false, false);
        }

        gpioInit(PIN_CLK);
        constexpr int iomux_clock = I2S0O_WS_OUT_IDX;
        esp_rom_gpio_connect_out_signal(PIN_CLK, iomux_clock, false, false);

        if ((dma_desc = static_cast<lldesc_t*>(heap_caps_malloc(
            sizeof(lldesc_t) * MATRIX_ROWS_PER_FRAME * MATRIX_COLOR_DEPTH, MALLOC_CAP_DMA))) == nullptr)
        {
            ESP_LOGE(TAG, "Failed to allocate memory for DMA descriptors");
            abort();
        }

        for (size_t r = 0; r < MATRIX_ROWS_PER_FRAME; r++)
        {
            uint16_t abcde = static_cast<uint16_t>(r);
            abcde <<= BITS_ABCDE_OFFSET;

            for (size_t d = 0; d < MATRIX_COLOR_DEPTH; d++)
            {
                const auto row_buf = static_cast<volatile uint16_t*>(
                    heap_caps_malloc(MATRIX_PIXELS_PER_ROW * sizeof(uint16_t), MALLOC_CAP_DMA));

                if (row_buf == nullptr)
                {
                    ESP_LOGE(TAG, "Failed to allocate memory for row buffer");
                    abort();
                }

                for (size_t p = 0; p < MATRIX_PIXELS_PER_ROW; p++)
                {
                    if (d == 0)
                    {
                        // depth[0] (LSB) x_pixels must be "marked" with the previous's row address,
                        // because it is used to display the previous row while we pump in LSB's for a new row
                        row_buf[xCoord(p)] = (static_cast<uint16_t>(r) - 1) << BITS_ABCDE_OFFSET;
                    }
                    else
                    {
                        row_buf[xCoord(p)] = abcde;
                    }
                }

                row_buf[xCoord(MATRIX_PIXELS_PER_ROW - 1)] |= BIT_LAT;

                lldesc_t* row_desc = &dma_desc[r * MATRIX_COLOR_DEPTH + d];
                const bool is_last = r == MATRIX_ROWS_PER_FRAME - 1 && d == MATRIX_COLOR_DEPTH - 1;

                row_desc->size = MATRIX_PIXELS_PER_ROW * sizeof(uint16_t);
                row_desc->length = MATRIX_PIXELS_PER_ROW * sizeof(uint16_t);
                row_desc->buf = reinterpret_cast<const volatile uint8_t*>(row_buf);
                row_desc->eof = is_last;
                row_desc->sosf = 0;
                row_desc->owner = 1;
                row_desc->qe.stqe_next = is_last ? &dma_desc[0] : &dma_desc[r * MATRIX_COLOR_DEPTH + d + 1];
                row_desc->offset = 0;
            }
        }

        i2s_dev_t* dev = &I2S0;

        dev->clkm_conf.clka_en = 1; // Use the 80mhz system clock (PLL_D2_CLK) when '0'
        dev->clkm_conf.clkm_div_a = 1; // Clock denominator
        dev->clkm_conf.clkm_div_b = 0; // Clock numerator
        dev->clkm_conf.clkm_div_num = 16;

        dev->sample_rate_conf.val = 0;

        // Third stage config, width of data to be written to IO (I think this should always be the actual
        // data width?)
        dev->sample_rate_conf.rx_bits_mod = 16;
        dev->sample_rate_conf.tx_bits_mod = 16;

        // Serial clock
        // ESP32 and ESP32-S2 TRM clearly say that "Note that I2S_TX_BCK_DIV_NUM[5:0] must not be configured
        // as 1."
        dev->sample_rate_conf.rx_bck_div_num = 2;
        dev->sample_rate_conf.tx_bck_div_num = 2;

        // I2S conf2 reg
        dev->conf2.val = 0;
        dev->conf2.lcd_en = 1;
        dev->conf2.lcd_tx_wrx2_en = 0;
        dev->conf2.lcd_tx_sdx2_en = 0;

        // Now start setting up DMA FIFO
        dev->fifo_conf.val = 0;
        dev->fifo_conf.rx_data_num = 32; // Thresholds.
        dev->fifo_conf.tx_data_num = 32;
        dev->fifo_conf.dscr_en = 1;

        // I2S conf reg
        dev->conf.val = 0;

        // Mode 1, a single 16-bit channel, load 16 bit sample(*) into fifo and pad to 32 bit with zeros
        // *Actually a 32-bit read where two samples are read at once. Length of fifo must thus still be
        // word-aligned
        dev->fifo_conf.tx_fifo_mod = 1;

        // Dictated by ESP32 datasheet
        dev->fifo_conf.rx_fifo_mod_force_en = 1;
        dev->fifo_conf.tx_fifo_mod_force_en = 1;

        // Second stage config
        dev->conf_chan.val = 0;

        // 16-bit single channel data
        dev->conf_chan.tx_chan_mod = 1;
        dev->conf_chan.rx_chan_mod = 1;

        // Reset FIFO
        dev->conf.rx_fifo_reset = 1;

        dev->conf.rx_fifo_reset = 0;
        dev->conf.tx_fifo_reset = 1;

        dev->conf.tx_fifo_reset = 0;

        // Reset DMA
        dev->lc_conf.in_rst = 1;
        dev->lc_conf.in_rst = 0;
        dev->lc_conf.out_rst = 1;
        dev->lc_conf.out_rst = 0;

        dev->lc_conf.ahbm_rst = 1;
        dev->lc_conf.ahbm_rst = 0;

        dev->in_link.val = 0;
        dev->out_link.val = 0;

        // Device reset
        dev->conf.rx_reset = 1;
        dev->conf.tx_reset = 1;
        dev->conf.rx_reset = 0;
        dev->conf.tx_reset = 0;

        dev->conf1.val = 0;
        dev->conf1.tx_stop_en = 0;
        dev->timing.val = 0;

        dev->lc_conf.val = I2S_OUT_DATA_BURST_EN | I2S_OUTDSCR_BURST_EN;

        // Should be last dma_descriptor
        dev->out_link.addr = reinterpret_cast<uint32_t>(dma_desc);

        // Start DMA operation
        dev->out_link.stop = 0;
        dev->out_link.start = 1;

        dev->conf.tx_start = 1;

        ESP_LOGI(TAG, "Running...");
    }

    ~LedMatrix() override
    {
        ESP_LOGI(TAG, "Destroying...");
        if (dma_desc) heap_caps_free(dma_desc);
        ESP_LOGI(TAG, "Destroyed");
    }

public:
    void setBrightness(const uint8_t brightness) const
    {
        constexpr uint8_t _depth = MATRIX_COLOR_DEPTH;
        constexpr uint16_t _width = MATRIX_PIXELS_PER_ROW;

        // start with iterating all rows in dma_buff structure
        int row_idx = MATRIX_ROWS_PER_FRAME;
        do
        {
            --row_idx;

            // let's set OE control bits for specific pixels in each color_index subrows
            uint8_t color_idx = _depth;
            do
            {
                constexpr uint8_t _blank = 2;
                --color_idx;

                const char bitplane = (2 * _depth - color_idx) % _depth;
                constexpr char bitshift = (_depth - 2 - 1) >> 1;

                const char rightshift = std::max(bitplane - bitshift - 2, 0);
                // calculate the OE disable period by brightness and also blanking
                int brightness_in_x_pixels = ((_width - _blank) * brightness) >> (7 + rightshift);
                brightness_in_x_pixels = (brightness_in_x_pixels >> 1) | (brightness_in_x_pixels & 1);

                // switch a pointer to a row for a specific color index
                const auto row = dmaDescAt(row_idx * MATRIX_COLOR_DEPTH + color_idx);

                // define the range of Output Enable in the center of the row
                const int x_coord_max = (_width + brightness_in_x_pixels + 1) >> 1;
                const int x_coord_min = (_width - brightness_in_x_pixels + 0) >> 1;
                int x_coord = _width;
                do
                {
                    --x_coord;

                    // (the check is already including "blanking")
                    if (x_coord >= x_coord_min && x_coord < x_coord_max)
                    {
                        row[xCoord(x_coord)] &= ~BIT_OE;
                    }
                    else
                    {
                        row[xCoord(x_coord)] |= BIT_OE; // Disable output after this point.
                    }
                }
                while (x_coord);
            }
            while (color_idx);
        }
        while (row_idx);
    }

    void loadFromBuffer(const volatile uint32_t* buffer) const
    {
        if (buffer == nullptr)
        {
            return;
        }

        for (int r = 0; r < MATRIX_ROWS_PER_FRAME; r++)
        {
            for (int d = 0; d < MATRIX_COLOR_DEPTH; d++)
            {
                const auto dma_row_buffer = dmaDescAt(r * MATRIX_COLOR_DEPTH + d);

                for (int c = 0; c < MATRIX_PIXELS_PER_ROW; c++)
                {
                    const uint32_t top_pixel = buffer[r * MATRIX_PIXELS_PER_ROW + c];
                    const uint32_t bot_pixel = buffer[(r + MATRIX_ROWS_PER_FRAME) * MATRIX_PIXELS_PER_ROW + c];
                    const uint16_t mask = 1 << (d + MATRIX_COLOR_DEPTH);

                    const int xc = xCoord(c);
                    dma_row_buffer[xc] &= ~BITMASK_RGB1_RBG2;
                    dma_row_buffer[xc] |= lumTbl[gammaTbl[top_pixel >> 16 & 0xFF]] & mask ? BIT_R1 : 0;
                    dma_row_buffer[xc] |= lumTbl[gammaTbl[top_pixel >> 8 & 0xFF]] & mask ? BIT_G1 : 0;
                    dma_row_buffer[xc] |= lumTbl[gammaTbl[top_pixel & 0xFF]] & mask ? BIT_B1 : 0;
                    dma_row_buffer[xc] |= lumTbl[gammaTbl[bot_pixel >> 16 & 0xFF]] & mask ? BIT_R2 : 0;
                    dma_row_buffer[xc] |= lumTbl[gammaTbl[bot_pixel >> 8 & 0xFF]] & mask ? BIT_G2 : 0;
                    dma_row_buffer[xc] |= lumTbl[gammaTbl[bot_pixel & 0xFF]] & mask ? BIT_B2 : 0;
                }
            }
        }
    }

    void fillRgb(const uint8_t red, const uint8_t green, const uint8_t blue) const
    {
        for (int r = 0; r < MATRIX_ROWS_PER_FRAME; r++)
        {
            for (int d = 0; d < MATRIX_COLOR_DEPTH; d++)
            {
                const auto dma_row_buffer = dmaDescAt(r * MATRIX_COLOR_DEPTH + d);
                const uint16_t mask = 1 << (d + MATRIX_COLOR_DEPTH);

                for (int c = 0; c < MATRIX_PIXELS_PER_ROW; c++)
                {
                    const int xc = xCoord(c);

                    dma_row_buffer[xc] &= ~BITMASK_RGB1_RBG2;

                    // Set R1, G1, B1 for the top half of the matrix
                    dma_row_buffer[xc] |= lumTbl[red] & mask ? BIT_R1 : 0;
                    dma_row_buffer[xc] |= lumTbl[green] & mask ? BIT_G1 : 0;
                    dma_row_buffer[xc] |= lumTbl[blue] & mask ? BIT_B1 : 0;

                    // Set R2, G2, B2 for the bottom half of the matrix
                    dma_row_buffer[xc] |= lumTbl[red] & mask ? BIT_R2 : 0;
                    dma_row_buffer[xc] |= lumTbl[green] & mask ? BIT_G2 : 0;
                    dma_row_buffer[xc] |= lumTbl[blue] & mask ? BIT_B2 : 0;
                }
            }
        }
    }

    void clear() const
    {
        for (int r = 0; r < MATRIX_ROWS_PER_FRAME; r++)
        {
            for (int d = 0; d < MATRIX_COLOR_DEPTH; d++)
            {
                const auto dma_row_buffer = dmaDescAt(r * MATRIX_COLOR_DEPTH + d);
                for (int c = 0; c < MATRIX_PIXELS_PER_ROW; c++)
                {
                    dma_row_buffer[xCoord(c)] &= ~BITMASK_RGB1_RBG2;
                }
            }
        }
    }

    void drawPixel(const uint8_t x, const uint8_t y, const uint8_t r, const uint8_t g, const uint8_t b) const
    {
        if (x >= MATRIX_WIDTH || y >= MATRIX_HEIGHT)
        {
            return;
        }

        const bool isTopHalf = y < MATRIX_ROWS_PER_FRAME;
        const int row = isTopHalf ? y : y - MATRIX_ROWS_PER_FRAME;

        for (int d = 0; d < MATRIX_COLOR_DEPTH; d++)
        {
            const auto dma_row_buffer = dmaDescAt(row * MATRIX_COLOR_DEPTH + d);
            const uint16_t mask = 1 << (d + MATRIX_COLOR_DEPTH);

            const int xc = xCoord(x);

            if (isTopHalf)
            {
                dma_row_buffer[xc] &= ~BITMASK_RGB1;
                dma_row_buffer[xc] |= lumTbl[r] & mask ? BIT_R1 : 0;
                dma_row_buffer[xc] |= lumTbl[g] & mask ? BIT_G1 : 0;
                dma_row_buffer[xc] |= lumTbl[b] & mask ? BIT_B1 : 0;
            }
            else
            {
                dma_row_buffer[xc] &= ~BITMASK_RGB2;
                dma_row_buffer[xc] |= lumTbl[r] & mask ? BIT_R2 : 0;
                dma_row_buffer[xc] |= lumTbl[g] & mask ? BIT_G2 : 0;
                dma_row_buffer[xc] |= lumTbl[b] & mask ? BIT_B2 : 0;
            }
        }
    }

    void getPixel(const uint8_t x, const uint8_t y, uint8_t& r, uint8_t& g, uint8_t& b) const
    {
        // Initialize output values to zero (black) in case pixel is out of bounds
        r = g = b = 0;

        // Check if coordinates are within matrix bounds
        if (x >= MATRIX_WIDTH || y >= MATRIX_HEIGHT)
        {
            return;
        }

        // Determine if this pixel is in the top or bottom half of the matrix
        const bool isTopHalf = y < MATRIX_ROWS_PER_FRAME;
        const int row = isTopHalf ? y : y - MATRIX_ROWS_PER_FRAME;
        const int xc = xCoord(x);

        // We'll use bit contribution from each color depth plane to reconstruct the color intensity
        uint16_t rValue = 0;
        uint16_t gValue = 0;
        uint16_t bValue = 0;

        const uint16_t rBit = isTopHalf ? BIT_R1 : BIT_R2;
        const uint16_t gBit = isTopHalf ? BIT_G1 : BIT_G2;
        const uint16_t bBit = isTopHalf ? BIT_B1 : BIT_B2;

        // Check each bit plane and accumulate the color intensity
        for (int d = 0; d < MATRIX_COLOR_DEPTH; d++)
        {
            const auto dma_buffer = dmaDescAt(row * MATRIX_COLOR_DEPTH + d);
            // Each bit plane contributes a power of 2 to the final color intensity
            const uint16_t bit_value = 1 << d;

            if (dma_buffer[xc] & rBit) rValue |= bit_value;
            if (dma_buffer[xc] & gBit) gValue |= bit_value;
            if (dma_buffer[xc] & bBit) bValue |= bit_value;
        }

        // Scale the 8-bit values to the 0-255 range
        // Assuming MATRIX_COLOR_DEPTH is 8, this gives a full 8-bit color range
        constexpr float scale = 255.0f / ((1 << MATRIX_COLOR_DEPTH) - 1);

        r = static_cast<uint8_t>(rValue * scale);
        g = static_cast<uint8_t>(gValue * scale);
        b = static_cast<uint8_t>(bValue * scale);
    }
};
