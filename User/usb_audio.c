#include "usb_audio.h"
#include "ch32v30x_usbhs_device.h"

#define UAC_SET_CUR                    0x01U
#define UAC_GET_CUR                    0x81U
#define UAC_GET_MIN                    0x82U
#define UAC_GET_MAX                    0x83U
#define UAC_GET_RES                    0x84U

#define UAC_CS_MUTE                    0x01U
#define UAC_CS_VOLUME                  0x02U
#define UAC_CS_SAMPLING_FREQ           0x01U
#define UAC_FEATURE_UNIT_ID            0x02U

#define AUDIO_RING_SAMPLES             4096U
#define AUDIO_RING_MASK                (AUDIO_RING_SAMPLES - 1U)
#define AUDIO_DMA_CHUNK_SAMPLES        128U
#define AUDIO_CONTROL_HZ               100U
#define AUDIO_CORRECTION_LIMIT_PPM     5000L
#define AUDIO_VOLUME_MIN               ((int16_t)-24576) /* -96 dB, 8.8 */
#define AUDIO_VOLUME_MAX               ((int16_t)0)
#define AUDIO_VOLUME_RES               ((int16_t)256)    /* 1 dB, 8.8 */

static const uint32_t audio_supported_rates[] = {
    44100U, 48000U, 88200U, 96000U, 176400U, 192000U
};

static __attribute__((aligned(4))) uint16_t audio_ring[AUDIO_RING_SAMPLES];
static __attribute__((aligned(4))) uint16_t audio_silence[AUDIO_DMA_CHUNK_SAMPLES];

/* Producer owns write_total; the DMA completion ISR owns read_total. */
static volatile uint32_t audio_write_total;
static volatile uint32_t audio_read_total;
static volatile uint32_t audio_overruns;
static volatile uint32_t audio_underruns;

static volatile uint8_t audio_stream_alt;
static volatile uint8_t audio_dma_running;
static volatile uint8_t audio_dma_from_ring;
static volatile uint8_t audio_primed;
static volatile uint16_t audio_dma_samples;

static uint32_t audio_sample_rate = 48000U;
static uint32_t audio_target_fill = 480U;
static uint32_t audio_base_div_q16;
static uint32_t audio_timer_div_q16;
static uint32_t audio_div_fraction;
static uint32_t audio_control_samples;
static int32_t audio_integrator;

static uint8_t audio_mute;
static int16_t audio_volume[3];
static uint8_t audio_control_reply[3];

static uint8_t USB_Audio_RateSupported(uint32_t rate)
{
    uint8_t i;
    for (i = 0U; i < (uint8_t)(sizeof(audio_supported_rates) /
                               sizeof(audio_supported_rates[0])); ++i) {
        if (audio_supported_rates[i] == rate) {
            return 1U;
        }
    }
    return 0U;
}

static uint32_t USB_Audio_FillLevel(void)
{
    return audio_write_total - audio_read_total;
}

static uint16_t USB_Audio_GainQ15(int16_t volume)
{
    static const uint16_t one_db_gain[6] = {
        32767U, 29204U, 26028U, 23198U, 20675U, 18427U
    };
    int32_t db;
    uint32_t attenuation;
    uint32_t shift;
    uint32_t remainder;

    if (volume >= AUDIO_VOLUME_MAX) {
        return 32767U;
    }
    if (volume <= AUDIO_VOLUME_MIN) {
        return 0U;
    }

    db = volume / 256;
    attenuation = (uint32_t)(-db);
    shift = attenuation / 6U;
    remainder = attenuation % 6U;
    if (shift >= 15U) {
        return 0U;
    }
    return (uint16_t)(one_db_gain[remainder] >> shift);
}

static void USB_Audio_UpdateBaseDivider(void)
{
    uint32_t integer_part;
    uint32_t remainder;

    integer_part = SystemCoreClock / audio_sample_rate;
    remainder = SystemCoreClock % audio_sample_rate;
    audio_base_div_q16 = (integer_part << 16) +
                         (((remainder * 4096U) / audio_sample_rate) << 4);
    audio_timer_div_q16 = audio_base_div_q16;
    audio_div_fraction = 0U;

    audio_target_fill = audio_sample_rate / 100U; /* approximately 10 ms */
    if (audio_target_fill < 256U) {
        audio_target_fill = 256U;
    }
    if (audio_target_fill > (AUDIO_RING_SAMPLES / 2U)) {
        audio_target_fill = AUDIO_RING_SAMPLES / 2U;
    }
}

static void USB_Audio_ApplyTimerDivider(void)
{
    uint32_t divider = audio_timer_div_q16 >> 16;
    uint32_t fraction = audio_timer_div_q16 & 0xffffU;

    audio_div_fraction += fraction;
    if (audio_div_fraction >= 0x10000U) {
        audio_div_fraction -= 0x10000U;
        ++divider;
    }
    if (divider < 2U) {
        divider = 2U;
    }
    TIM7->ATRLR = divider - 1U;
}

static void USB_Audio_UpdateAdaptiveRate(void)
{
    int32_t fill = (int32_t)USB_Audio_FillLevel();
    int32_t error = fill - (int32_t)audio_target_fill;
    int32_t normalized_error;
    int32_t correction_ppm;
    int32_t divider_delta;

    normalized_error = (error * 1024L) / (int32_t)audio_target_fill;
    audio_integrator += normalized_error;
    if (audio_integrator > 80000L) {
        audio_integrator = 80000L;
    } else if (audio_integrator < -80000L) {
        audio_integrator = -80000L;
    }

    correction_ppm = (normalized_error * 2L) + (audio_integrator / 16L);
    if (correction_ppm > AUDIO_CORRECTION_LIMIT_PPM) {
        correction_ppm = AUDIO_CORRECTION_LIMIT_PPM;
    } else if (correction_ppm < -AUDIO_CORRECTION_LIMIT_PPM) {
        correction_ppm = -AUDIO_CORRECTION_LIMIT_PPM;
    }

    /* Avoid 64-bit math in the DMA ISR. The two /1000 operations retain
       substantially finer resolution than one TIM7 clock tick. */
    divider_delta = ((int32_t)(audio_base_div_q16 / 1000U) *
                     correction_ppm) / 1000L;
    audio_timer_div_q16 = (uint32_t)((int32_t)audio_base_div_q16 -
                                     divider_delta);
}

static void USB_Audio_ScheduleDma(void)
{
    uint32_t fill = USB_Audio_FillLevel();
    uint32_t read_index;
    uint32_t samples;

    if (audio_stream_alt == 0U) {
        return;
    }

    if ((audio_primed == 0U) && (fill >= audio_target_fill)) {
        audio_primed = 1U;
        audio_integrator = 0;
        audio_timer_div_q16 = audio_base_div_q16;
    }

    DMA2_Channel4->CFGR &= (uint16_t)~DMA_CFGR1_EN;
    DMA_ClearITPendingBit(DMA2_IT_GL4);

    if ((audio_primed != 0U) && (fill != 0U)) {
        read_index = audio_read_total & AUDIO_RING_MASK;
        samples = AUDIO_RING_SAMPLES - read_index;
        if (samples > fill) {
            samples = fill;
        }
        if (samples > AUDIO_DMA_CHUNK_SAMPLES) {
            samples = AUDIO_DMA_CHUNK_SAMPLES;
        }
        DMA2_Channel4->MADDR = (uint32_t)&audio_ring[read_index];
        audio_dma_from_ring = 1U;
    } else {
        samples = AUDIO_DMA_CHUNK_SAMPLES;
        DMA2_Channel4->MADDR = (uint32_t)audio_silence;
        audio_dma_from_ring = 0U;
        if (audio_dma_running != 0U) {
            ++audio_underruns;
        }
    }

    audio_dma_samples = (uint16_t)samples;
    DMA2_Channel4->CNTR = (uint16_t)samples;
    USB_Audio_ApplyTimerDivider();
    DMA2_Channel4->CFGR |= DMA_CFGR1_EN;

    if (audio_dma_running == 0U) {
        TIM7->CNT = 0U;
        TIM_Cmd(TIM7, ENABLE);
        audio_dma_running = 1U;
    }
}

static void USB_Audio_StopStream(void)
{
    TIM_Cmd(TIM7, DISABLE);
    DMA2_Channel4->CFGR &= (uint16_t)~DMA_CFGR1_EN;
    DMA_ClearITPendingBit(DMA2_IT_GL4);
    audio_dma_running = 0U;
    audio_dma_from_ring = 0U;
    audio_dma_samples = 0U;
    audio_primed = 0U;
    audio_write_total = 0U;
    audio_read_total = 0U;
    audio_integrator = 0;
    audio_control_samples = 0U;
    audio_timer_div_q16 = audio_base_div_q16;
    audio_div_fraction = 0U;
    DAC->L12BDHR2 = 0x8000U;
}

static void USB_Audio_SetSampleRate(uint32_t rate)
{
    uint8_t was_streaming;

    if (USB_Audio_RateSupported(rate) == 0U) {
        return;
    }
    was_streaming = audio_stream_alt;
    USB_Audio_StopStream();
    audio_sample_rate = rate;
    USB_Audio_UpdateBaseDivider();
    if (was_streaming != 0U) {
        audio_stream_alt = 1U;
    }
}

void USB_Audio_Init(void)
{
    GPIO_InitTypeDef gpio = {0};
    DMA_InitTypeDef dma = {0};
    DAC_InitTypeDef dac = {0};
    TIM_TimeBaseInitTypeDef timer = {0};
    NVIC_InitTypeDef nvic = {0};
    uint16_t i;

    for (i = 0U; i < AUDIO_DMA_CHUNK_SAMPLES; ++i) {
        audio_silence[i] = 0x8000U;
    }
    audio_volume[0] = AUDIO_VOLUME_MAX;
    audio_volume[1] = AUDIO_VOLUME_MAX;
    audio_volume[2] = AUDIO_VOLUME_MAX;
    audio_mute = 0U;

    RCC_AHBPeriphClockCmd(RCC_AHBPeriph_DMA2, ENABLE);
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_DAC | RCC_APB1Periph_TIM7, ENABLE);
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA |
                           RCC_APB2Periph_GPIOC, ENABLE);

    gpio.GPIO_Pin = GPIO_Pin_5;
    gpio.GPIO_Speed = GPIO_Speed_50MHz;
    gpio.GPIO_Mode = GPIO_Mode_AIN;
    GPIO_Init(GPIOA, &gpio);

    /* The original board audio path uses PC8 as an active-high mute/shutdown
       gate. Keep it driven low; leaving this pin floating makes the amplifier
       output only a very faint signal. */
    GPIO_ResetBits(GPIOC, GPIO_Pin_8);
    gpio.GPIO_Pin = GPIO_Pin_8;
    gpio.GPIO_Speed = GPIO_Speed_50MHz;
    gpio.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_Init(GPIOC, &gpio);

    DMA_DeInit(DMA2_Channel4);
    dma.DMA_PeripheralBaseAddr = (uint32_t)&DAC->L12BDHR2;
    dma.DMA_MemoryBaseAddr = (uint32_t)audio_silence;
    dma.DMA_DIR = DMA_DIR_PeripheralDST;
    dma.DMA_BufferSize = AUDIO_DMA_CHUNK_SAMPLES;
    dma.DMA_PeripheralInc = DMA_PeripheralInc_Disable;
    dma.DMA_MemoryInc = DMA_MemoryInc_Enable;
    dma.DMA_PeripheralDataSize = DMA_PeripheralDataSize_HalfWord;
    dma.DMA_MemoryDataSize = DMA_MemoryDataSize_HalfWord;
    dma.DMA_Mode = DMA_Mode_Normal;
    dma.DMA_Priority = DMA_Priority_High;
    dma.DMA_M2M = DMA_M2M_Disable;
    DMA_Init(DMA2_Channel4, &dma);
    DMA_ITConfig(DMA2_Channel4, DMA_IT_TC | DMA_IT_TE, ENABLE);

    dac.DAC_Trigger = DAC_Trigger_T7_TRGO;
    dac.DAC_WaveGeneration = DAC_WaveGeneration_None;
    dac.DAC_LFSRUnmask_TriangleAmplitude = DAC_LFSRUnmask_Bits1_0;
    dac.DAC_OutputBuffer = DAC_OutputBuffer_Enable;
    DAC_Init(DAC_Channel_2, &dac);
    DAC_DMACmd(DAC_Channel_2, ENABLE);
    DAC_Cmd(DAC_Channel_2, ENABLE);
    DAC->L12BDHR2 = 0x8000U;

    timer.TIM_Prescaler = 0U;
    timer.TIM_CounterMode = TIM_CounterMode_Up;
    timer.TIM_Period = (SystemCoreClock / audio_sample_rate) - 1U;
    timer.TIM_ClockDivision = TIM_CKD_DIV1;
    timer.TIM_RepetitionCounter = 0U;
    TIM_TimeBaseInit(TIM7, &timer);
    TIM_SelectOutputTrigger(TIM7, TIM_TRGOSource_Update);
    TIM_Cmd(TIM7, DISABLE);

    nvic.NVIC_IRQChannel = DMA2_Channel4_IRQn;
    nvic.NVIC_IRQChannelPreemptionPriority = 1U;
    nvic.NVIC_IRQChannelSubPriority = 0U;
    nvic.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&nvic);

    USB_Audio_UpdateBaseDivider();
    USB_Audio_Reset();
}

void USB_Audio_Reset(void)
{
    audio_stream_alt = 0U;
    USB_Audio_StopStream();
}

uint8_t USB_Audio_SetInterface(uint8_t interface_number,
                               uint8_t alternate_setting)
{
    if ((interface_number == 0U) ||
        (interface_number == USB_AUDIO_CONTROL_INTERFACE)) {
        return (alternate_setting == 0U) ? 1U : 0U;
    }
    if (interface_number != USB_AUDIO_STREAM_INTERFACE) {
        return 0U;
    }
    if (alternate_setting > 1U) {
        return 0U;
    }

    USB_Audio_StopStream();
    audio_stream_alt = alternate_setting;
    if (alternate_setting != 0U) {
        USBHSD->UEP3_RX_CTRL = (USBHSD->UEP3_RX_CTRL &
                                ~(USBHS_UEP_R_RES_MASK |
                                  USBHS_UEP_R_TOG_MASK)) |
                               USBHS_UEP_R_TOG_AUTO |
                               USBHS_UEP_R_RES_ACK |
                               USBHS_UEP_R_TOG_DATA0;
    } else {
        USBHSD->UEP3_RX_CTRL = (USBHSD->UEP3_RX_CTRL &
                                ~USBHS_UEP_R_RES_MASK) |
                               USBHS_UEP_R_RES_NAK;
    }
    return 1U;
}

uint8_t USB_Audio_GetInterface(uint8_t interface_number,
                               uint8_t *alternate_setting)
{
    if ((interface_number == 0U) ||
        (interface_number == USB_AUDIO_CONTROL_INTERFACE)) {
        *alternate_setting = 0U;
        return 1U;
    }
    if (interface_number == USB_AUDIO_STREAM_INTERFACE) {
        *alternate_setting = audio_stream_alt;
        return 1U;
    }
    return 0U;
}

uint8_t USB_Audio_ClassIn(uint8_t request, uint16_t value, uint16_t index,
                          const uint8_t **data, uint16_t *length)
{
    uint8_t control = (uint8_t)(value >> 8);
    uint8_t channel = (uint8_t)value;
    uint8_t entity = (uint8_t)(index >> 8);
    uint8_t endpoint = (uint8_t)index;
    int16_t reply16;
    uint32_t reply24;

    if ((endpoint == USB_AUDIO_OUT_ENDPOINT) &&
        (control == UAC_CS_SAMPLING_FREQ)) {
        switch (request) {
            case UAC_GET_CUR: reply24 = audio_sample_rate; break;
            case UAC_GET_MIN: reply24 = audio_supported_rates[0]; break;
            case UAC_GET_MAX:
                reply24 = audio_supported_rates[(sizeof(audio_supported_rates) /
                                                  sizeof(audio_supported_rates[0])) - 1U];
                break;
            case UAC_GET_RES: reply24 = 1U; break;
            default: return 0U;
        }
        audio_control_reply[0] = (uint8_t)reply24;
        audio_control_reply[1] = (uint8_t)(reply24 >> 8);
        audio_control_reply[2] = (uint8_t)(reply24 >> 16);
        *data = audio_control_reply;
        *length = 3U;
        return 1U;
    }

    if ((entity != UAC_FEATURE_UNIT_ID) || (channel > 2U)) {
        return 0U;
    }
    if ((control == UAC_CS_MUTE) && (channel == 0U) &&
        (request == UAC_GET_CUR)) {
        audio_control_reply[0] = audio_mute;
        *data = audio_control_reply;
        *length = 1U;
        return 1U;
    }
    if (control != UAC_CS_VOLUME) {
        return 0U;
    }
    switch (request) {
        case UAC_GET_CUR: reply16 = audio_volume[channel]; break;
        case UAC_GET_MIN: reply16 = AUDIO_VOLUME_MIN; break;
        case UAC_GET_MAX: reply16 = AUDIO_VOLUME_MAX; break;
        case UAC_GET_RES: reply16 = AUDIO_VOLUME_RES; break;
        default: return 0U;
    }
    audio_control_reply[0] = (uint8_t)reply16;
    audio_control_reply[1] = (uint8_t)((uint16_t)reply16 >> 8);
    *data = audio_control_reply;
    *length = 2U;
    return 1U;
}

uint8_t USB_Audio_ClassOutSetup(uint8_t request, uint16_t value,
                                uint16_t index, uint16_t length)
{
    uint8_t control = (uint8_t)(value >> 8);
    uint8_t channel = (uint8_t)value;
    uint8_t entity = (uint8_t)(index >> 8);
    uint8_t endpoint = (uint8_t)index;

    if (request != UAC_SET_CUR) {
        return 0U;
    }
    if ((endpoint == USB_AUDIO_OUT_ENDPOINT) &&
        (control == UAC_CS_SAMPLING_FREQ) && (length == 3U)) {
        return 1U;
    }
    if ((entity == UAC_FEATURE_UNIT_ID) && (channel <= 2U)) {
        if ((control == UAC_CS_MUTE) && (channel == 0U) &&
            (length == 1U)) {
            return 1U;
        }
        if ((control == UAC_CS_VOLUME) && (length == 2U)) {
            return 1U;
        }
    }
    return 0U;
}

uint8_t USB_Audio_ClassOutData(uint8_t request, uint16_t value,
                               uint16_t index, const uint8_t *data,
                               uint16_t length)
{
    uint8_t control = (uint8_t)(value >> 8);
    uint8_t channel = (uint8_t)value;
    uint8_t entity = (uint8_t)(index >> 8);
    uint8_t endpoint = (uint8_t)index;
    uint32_t rate;
    int16_t volume;

    if (USB_Audio_ClassOutSetup(request, value, index, length) == 0U) {
        return 0U;
    }
    if (endpoint == USB_AUDIO_OUT_ENDPOINT) {
        rate = (uint32_t)data[0] | ((uint32_t)data[1] << 8) |
               ((uint32_t)data[2] << 16);
        if (USB_Audio_RateSupported(rate) == 0U) {
            return 0U;
        }
        USB_Audio_SetSampleRate(rate);
        return 1U;
    }
    if ((entity == UAC_FEATURE_UNIT_ID) && (control == UAC_CS_MUTE)) {
        audio_mute = (data[0] != 0U) ? 1U : 0U;
        return 1U;
    }
    if ((entity == UAC_FEATURE_UNIT_ID) && (control == UAC_CS_VOLUME)) {
        volume = (int16_t)((uint16_t)data[0] | ((uint16_t)data[1] << 8));
        if (volume < AUDIO_VOLUME_MIN) {
            volume = AUDIO_VOLUME_MIN;
        } else if (volume > AUDIO_VOLUME_MAX) {
            volume = AUDIO_VOLUME_MAX;
        }
        audio_volume[channel] = volume;
        if (channel == 0U) {
            audio_volume[1] = volume;
            audio_volume[2] = volume;
        }
        return 1U;
    }
    return 0U;
}

uint8_t USB_Audio_RxPacketISR(const uint8_t *packet, uint16_t length)
{
    const int16_t *pcm = (const int16_t *)packet;
    uint32_t frames = length / 4U;
    uint32_t write_total;
    uint32_t free_samples;
    uint32_t i;
    uint16_t gain_left;
    uint16_t gain_right;

    if ((audio_stream_alt == 0U) || (frames == 0U)) {
        return 0U;
    }

    write_total = audio_write_total;
    free_samples = AUDIO_RING_SAMPLES - (write_total - audio_read_total);
    if (frames > free_samples) {
        audio_overruns += frames - free_samples;
        frames = free_samples;
    }

    if (audio_mute != 0U) {
        gain_left = 0U;
        gain_right = 0U;
    } else {
        gain_left = USB_Audio_GainQ15(audio_volume[1]);
        gain_right = USB_Audio_GainQ15(audio_volume[2]);
    }

    for (i = 0U; i < frames; ++i) {
        int32_t mixed = ((int32_t)pcm[i * 2U] * gain_left) +
                        ((int32_t)pcm[(i * 2U) + 1U] * gain_right);
        mixed >>= 16;
        audio_ring[(write_total + i) & AUDIO_RING_MASK] =
            (uint16_t)(mixed + 32768L);
    }
    audio_write_total = write_total + frames;

    if ((audio_dma_running == 0U) &&
        (USB_Audio_FillLevel() >= audio_target_fill)) {
        USB_Audio_ScheduleDma();
    }
    return 1U;
}

void DMA2_Channel4_IRQHandler(void) __attribute__((interrupt("WCH-Interrupt-fast")));
void DMA2_Channel4_IRQHandler(void)
{
    uint16_t completed_samples = audio_dma_samples;

    if (DMA_GetITStatus(DMA2_IT_TE4) != RESET) {
        DMA_ClearITPendingBit(DMA2_IT_GL4);
        TIM_Cmd(TIM7, DISABLE);
        DMA2_Channel4->CFGR &= (uint16_t)~DMA_CFGR1_EN;
        audio_dma_running = 0U;
        audio_primed = 0U;
        DAC->L12BDHR2 = 0x8000U;
        return;
    }
    if (DMA_GetITStatus(DMA2_IT_TC4) == RESET) {
        return;
    }

    DMA_ClearITPendingBit(DMA2_IT_GL4);
    if (audio_dma_from_ring != 0U) {
        audio_read_total += completed_samples;
    }

    if (USB_Audio_FillLevel() == 0U) {
        audio_primed = 0U;
    }

    audio_control_samples += completed_samples;
    if (audio_control_samples >= (audio_sample_rate / AUDIO_CONTROL_HZ)) {
        audio_control_samples -= audio_sample_rate / AUDIO_CONTROL_HZ;
        USB_Audio_UpdateAdaptiveRate();
    }
    USB_Audio_ScheduleDma();
}
