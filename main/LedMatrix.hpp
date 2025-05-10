#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <memory>
#include <string>
#include <utility>

#include "esp_log.h"
#include "driver/gpio.h"
#include "esp_private/periph_ctrl.h"
#include "freertos/FreeRTOS.h"
#include "hal/i2s_hal.h"
#include "hal/i2s_ll.h"
#include "hal/i2s_types.h"
#include "rom/lldesc.h"
#include "soc/gpio_sig_map.h"

template <size_t N>
static constexpr std::array<uint8_t, N> makeGammaTable()
{
    std::array<uint8_t, N> table{};

    for (size_t i = 0; i < N; i++)
    {
        const double normalized = static_cast<double>(i) / 255.0;
        double result = 0.0;
        if (normalized <= 0.0)
        {
            result = 0.0;
        }
        else if (normalized >= 1.0)
        {
            result = 1.0;
        }
        else
        {
            result = normalized * normalized * (normalized * 0.2 + 0.8);
        }

        table[i] = static_cast<uint8_t>(result * 255.0 + 0.5);
    }

    return table;
}

class LedMatrix
{
public:
    static constexpr uint8_t MATRIX_WIDTH = 64;
    static constexpr uint8_t MATRIX_HEIGHT = 64;
    static constexpr uint16_t MATRIX_SIZE = MATRIX_WIDTH * MATRIX_HEIGHT;
    static constexpr uint16_t MATRIX_BUFFER_SIZE = MATRIX_SIZE * sizeof(uint32_t);

private:
    static constexpr char TAG[] = "LedMatrix";

    static constexpr uint8_t I2S_NUM = 0;

    static constexpr uint8_t MATRIX_ROWS_PER_FRAME = MATRIX_HEIGHT / 2;
    static constexpr uint8_t MATRIX_PIXELS_PER_ROW = MATRIX_WIDTH;
    static constexpr uint8_t MATRIX_COLOR_DEPTH = 8;

    enum class Pin : uint32_t
    {
        R1 = 27,
        G1 = 26,
        B1 = 14,
        R2 = 12,
        G2 = 25,
        B2 = 15,
        LAT = 2,
        OE = 4,
        A = 32,
        B = 18,
        C = 33,
        D = 13,
        E = 5,
        CLK = 0,
    };

    static constexpr std::array<Pin, 13> pinsArr = {
        Pin::R1, Pin::G1, Pin::B1, Pin::R2, Pin::G2,
        Pin::B2, Pin::LAT, Pin::OE, Pin::A, Pin::B,
        Pin::C, Pin::D, Pin::E
    };

    static constexpr uint16_t BIT_R1 = 1 << 0;
    static constexpr uint16_t BIT_G1 = 1 << 1;
    static constexpr uint16_t BIT_B1 = 1 << 2;
    static constexpr uint16_t BIT_R2 = 1 << 3;
    static constexpr uint16_t BIT_G2 = 1 << 4;
    static constexpr uint16_t BIT_B2 = 1 << 5;
    static constexpr uint16_t BIT_LAT = 1 << 6;
    static constexpr uint16_t BIT_OE = 1 << 7;
    static constexpr uint16_t BIT_A = 1 << 8;
    static constexpr uint16_t BIT_B = 1 << 9;
    static constexpr uint16_t BIT_C = 1 << 10;
    static constexpr uint16_t BIT_D = 1 << 11;
    static constexpr uint16_t BIT_E = 1 << 12;
    static constexpr uint16_t BIT_CLK = 1 << 13;

    static constexpr uint16_t BITMASK_RGB1_RBG2 = BIT_R1 | BIT_G1 | BIT_B1 | BIT_R2 | BIT_G2 | BIT_B2;
    static constexpr uint16_t BITMASK_ABCDE = BIT_A | BIT_B | BIT_C | BIT_D | BIT_E;
    static constexpr int BITS_ABCDE_OFFSET = 8;

    static constexpr uint16_t DRAM_ATTR lumConvTab[] = {
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

    static constexpr auto GAMMA_TABLE = makeGammaTable<256>();

    lldesc_t* dma_descriptors = nullptr;

    static void gpioInit(Pin pin)
    {
        esp_rom_gpio_pad_select_gpio(static_cast<uint32_t>(pin));
        gpio_set_direction(static_cast<gpio_num_t>(pin), GPIO_MODE_OUTPUT);
        gpio_set_drive_capability(static_cast<gpio_num_t>(pin), static_cast<gpio_drive_cap_t>(3));
    }

    [[nodiscard]] static int xCoord(const int x_coord) noexcept
    {
        return x_coord & 1U ? x_coord - 1 : x_coord + 1;
    }

    void cleanup()
    {
        if (dma_descriptors)
        {
            ESP_LOGI(TAG, "Freeing DMA descriptors");
            heap_caps_free(dma_descriptors);
            dma_descriptors = nullptr;
        }
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
                // calculate the OE disable period by brightness, and also blanking
                int brightness_in_x_pixels = ((_width - _blank) * brightness) >> (7 + rightshift);
                brightness_in_x_pixels = (brightness_in_x_pixels >> 1) | (brightness_in_x_pixels & 1);

                // switch pointer to a row for a specific color index
                const auto row = (volatile uint16_t*)dma_descriptors[row_idx * MATRIX_COLOR_DEPTH + color_idx].buf;

                // define range of Output Enable on the center of the row
                const int x_coord_max = (_width + brightness_in_x_pixels + 1) >> 1;
                const int x_coord_min = (_width - brightness_in_x_pixels + 0) >> 1;
                int x_coord = _width;
                do
                {
                    --x_coord;

                    // (the check is already including "blanking" )
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

    void loadFromBuffer(const uint32_t* buffer) const
    {
        if (buffer == nullptr)
        {
            ESP_LOGE(TAG, "buffer is nullptr!");
            return;
        }

        for (int r = 0; r < MATRIX_ROWS_PER_FRAME; r++)
        {
            for (int d = 0; d < MATRIX_COLOR_DEPTH; d++)
            {
                const auto dma_row_buffer = (volatile uint16_t*)dma_descriptors[r * MATRIX_COLOR_DEPTH + d].buf;

                for (int c = 0; c < MATRIX_PIXELS_PER_ROW; c++)
                {
                    const uint32_t top_pixel = buffer[r * MATRIX_PIXELS_PER_ROW + c];
                    const uint32_t bot_pixel = buffer[(r + MATRIX_ROWS_PER_FRAME) * MATRIX_PIXELS_PER_ROW + c];
                    const uint16_t mask = 1 << (d + MATRIX_COLOR_DEPTH);

                    dma_row_buffer[xCoord(c)] &= ~BITMASK_RGB1_RBG2;
                    dma_row_buffer[xCoord(c)] |= lumConvTab[GAMMA_TABLE[top_pixel >> 16 & 0xFF]] & mask ? BIT_R1 : 0;
                    dma_row_buffer[xCoord(c)] |= lumConvTab[GAMMA_TABLE[top_pixel >> 8 & 0xFF]] & mask ? BIT_G1 : 0;
                    dma_row_buffer[xCoord(c)] |= lumConvTab[GAMMA_TABLE[top_pixel & 0xFF]] & mask ? BIT_B1 : 0;
                    dma_row_buffer[xCoord(c)] |= lumConvTab[GAMMA_TABLE[bot_pixel >> 16 & 0xFF]] & mask ? BIT_R2 : 0;
                    dma_row_buffer[xCoord(c)] |= lumConvTab[GAMMA_TABLE[bot_pixel >> 8 & 0xFF]] & mask ? BIT_G2 : 0;
                    dma_row_buffer[xCoord(c)] |= lumConvTab[GAMMA_TABLE[bot_pixel & 0xFF]] & mask ? BIT_B2 : 0;
                }
            }
        }
    }

    void fillRgb(const uint8_t r_val, const uint8_t g_val, const uint8_t b_val) const
    {
        for (int r = 0; r < MATRIX_ROWS_PER_FRAME; r++)
        {
            for (int d = 0; d < MATRIX_COLOR_DEPTH; d++)
            {
                const auto dma_row_buffer = (volatile uint16_t*)dma_descriptors[r * MATRIX_COLOR_DEPTH + d].buf;
                const uint16_t mask = 1 << (d + MATRIX_COLOR_DEPTH);

                for (int c = 0; c < MATRIX_PIXELS_PER_ROW; c++)
                {
                    dma_row_buffer[xCoord(c)] &= ~BITMASK_RGB1_RBG2;

                    // Set R1, G1, B1 for top half of the matrix
                    dma_row_buffer[xCoord(c)] |= lumConvTab[r_val] & mask ? BIT_R1 : 0;
                    dma_row_buffer[xCoord(c)] |= lumConvTab[g_val] & mask ? BIT_G1 : 0;
                    dma_row_buffer[xCoord(c)] |= lumConvTab[b_val] & mask ? BIT_B1 : 0;

                    // Set R2, G2, B2 for bottom half of the matrix
                    dma_row_buffer[xCoord(c)] |= lumConvTab[r_val] & mask ? BIT_R2 : 0;
                    dma_row_buffer[xCoord(c)] |= lumConvTab[g_val] & mask ? BIT_G2 : 0;
                    dma_row_buffer[xCoord(c)] |= lumConvTab[b_val] & mask ? BIT_B2 : 0;
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
                const auto dma_row_buffer = (volatile uint16_t*)dma_descriptors[r * MATRIX_COLOR_DEPTH + d].buf;
                for (int c = 0; c < MATRIX_PIXELS_PER_ROW; c++)
                {
                    dma_row_buffer[xCoord(c)] &= ~BITMASK_RGB1_RBG2;
                }
            }
        }
    }

    LedMatrix()
    {
        constexpr periph_module_t i2s_mod = I2S_NUM == 0 ? PERIPH_I2S0_MODULE : PERIPH_I2S1_MODULE;
        periph_module_reset(i2s_mod);
        periph_module_enable(i2s_mod);

        constexpr int iomux_signal_base = I2S_NUM == 0 ? I2S0O_DATA_OUT8_IDX : I2S1O_DATA_OUT8_IDX;
        for (size_t i = 0; i < pinsArr.size(); i++)
        {
            gpioInit(pinsArr[i]);
            esp_rom_gpio_connect_out_signal(static_cast<uint32_t>(pinsArr[i]), iomux_signal_base + i, false, false);
        }

        gpioInit(Pin::CLK);
        constexpr int iomux_clock = I2S_NUM == 0 ? I2S0O_WS_OUT_IDX : I2S1O_WS_OUT_IDX;
        esp_rom_gpio_connect_out_signal(static_cast<uint32_t>(Pin::CLK), iomux_clock, false, false);

        if ((dma_descriptors = static_cast<lldesc_t*>(heap_caps_malloc(
            sizeof(lldesc_t) * MATRIX_ROWS_PER_FRAME * MATRIX_COLOR_DEPTH, MALLOC_CAP_DMA))) == nullptr)
        {
            ESP_LOGE(TAG, "Failed to allocate memory for DMA descriptors");
            return;
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
                    return;
                }

                for (size_t p = 0; p < MATRIX_PIXELS_PER_ROW; p++)
                {
                    if (d == 0)
                    {
                        // depth[0] (LSB) x_pixels must be "marked" with a previous's row address, 'cause  it
                        // is used to display
                        //  previous row while we pump in LSB's for a new row
                        row_buf[xCoord(p)] = (static_cast<uint16_t>(r) - 1) << BITS_ABCDE_OFFSET;
                    }
                    else
                    {
                        row_buf[xCoord(p)] = abcde;
                    }
                }

                row_buf[xCoord(MATRIX_PIXELS_PER_ROW - 1)] |= BIT_LAT;

                lldesc_t* row_desc = &dma_descriptors[r * MATRIX_COLOR_DEPTH + d];
                const bool is_last = r == MATRIX_ROWS_PER_FRAME - 1 && d == MATRIX_COLOR_DEPTH - 1;

                row_desc->size = MATRIX_PIXELS_PER_ROW * sizeof(uint16_t);
                row_desc->length = MATRIX_PIXELS_PER_ROW * sizeof(uint16_t);
                row_desc->buf = reinterpret_cast<const volatile uint8_t*>(row_buf);
                row_desc->eof = is_last;
                row_desc->sosf = 0;
                row_desc->owner = 1;
                row_desc->qe.stqe_next =
                    is_last ? &dma_descriptors[0] : &dma_descriptors[r * MATRIX_COLOR_DEPTH + d + 1];
                row_desc->offset = 0;
            }
        }

        setBrightness(255);

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

        ////////////////////////////// END CLOCK CONFIGURATION /////////////////////////////////

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

        // Mode 1, single 16-bit channel, load 16 bit sample(*) into fifo and pad to 32 bit with zeros
        // *Actually a 32 bit read where two samples are read at once. Length of fifo must thus still be
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
        dev->out_link.addr = reinterpret_cast<uint32_t>(dma_descriptors);

        // Start DMA operation
        dev->out_link.stop = 0;
        dev->out_link.start = 1;

        dev->conf.tx_start = 1;
    }

    LedMatrix(LedMatrix&& other) noexcept
        : dma_descriptors(std::exchange(other.dma_descriptors, nullptr))
    {
    }

    // Move assignment operator
    LedMatrix& operator=(LedMatrix&& other) noexcept
    {
        if (this != &other)
        {
            cleanup();
            dma_descriptors = std::exchange(other.dma_descriptors, nullptr);
        }
        return *this;
    }

    ~LedMatrix()
    {
        cleanup();
    }

    LedMatrix(const LedMatrix&) = delete;
    LedMatrix& operator=(const LedMatrix&) = delete;
};
