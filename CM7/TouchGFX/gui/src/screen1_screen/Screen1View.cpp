#include <gui/screen1_screen/Screen1View.hpp>
#include "stm32h747i_discovery_audio.h"
#include "stm32h747i_discovery.h"      // for BSP_JOY_*
#include "stm32h7xx.h"
#include <math.h>
#include <string.h>
#include <touchgfx/Unicode.hpp>

#define SAMPLE_RATE         48000
#define BUFFER_SIZE         4096
#define MIN_NOTE_TICKS      18
#define ATTACK_TICKS   		6      // ~100ms attack — much smoother
#define RELEASE_TICKS   	10     // ~160ms release

#define VOL_STEP            5
#define VOL_MIN             0
#define VOL_MAX             100
#define VOL_BAR_MAX_WIDTH   140    // must match the Box width you set in Designer

static int16_t audioBuffer[BUFFER_SIZE] __attribute__((aligned(32)));
static BSP_AUDIO_Init_t audioOutInit;

static int16_t minTicksRemaining = 0;
static bool    releasePending    = false;
static int     octaveShift       = 0;

enum EnvelopeState { ENV_IDLE, ENV_ATTACK, ENV_SUSTAIN, ENV_RELEASE };
static EnvelopeState envState   = ENV_IDLE;
static int16_t       envCounter = 0;
static uint32_t      currentFrequency = 0;

static uint8_t currentVolume = 80;     // start at 80%
static uint32_t lastJoyState = 0;      // for edge detection (avoid repeats)
static float currentPhase = 0.0f;

extern "C" UART_HandleTypeDef huart1;   // USART1 = the ST-LINK virtual COM port

static uint32_t shiftFreq(uint32_t baseFreq)
{
    if (octaveShift > 0) return baseFreq << octaveShift;
    if (octaveShift < 0) return baseFreq >> (-octaveShift);
    return baseFreq;
}

static void generateSineWithAmplitude(uint32_t frequency, float amplitude)
{
    const int samplesPerChannel = BUFFER_SIZE / 2;

    float idealCycles = (float)frequency * (float)samplesPerChannel / (float)SAMPLE_RATE;
    int   wholeCycles = (int)(idealCycles + 0.5f);
    if (wholeCycles < 1) wholeCycles = 1;

    float phaseIncrement = 2.0f * 3.14159265f * (float)wholeCycles / (float)samplesPerChannel;
    float phase = currentPhase;   // continue from last position

    for (int i = 0; i < BUFFER_SIZE; i += 2)
    {
        int16_t sample = (int16_t)(amplitude * sinf(phase));
        audioBuffer[i]   = sample;
        audioBuffer[i+1] = sample;
        phase += phaseIncrement;
    }

    currentPhase = phase;  // save for next call

    // Keep phase in range to avoid float precision drift over time
    const float TWO_PI = 2.0f * 3.14159265f;
    while (currentPhase > TWO_PI) currentPhase -= TWO_PI;

    SCB_CleanDCache_by_Addr((uint32_t*)audioBuffer, sizeof(audioBuffer));
}

// Take one half of the 48 kHz stereo buffer, shrink to 8 kHz mono 8-bit,
// and push it out the serial port.
//static void streamHalfToUart(int startIndex)
////{
////    static uint8_t txbuf[200];
////    int k = 0;
////    // step 12 int16 = skip 6 stereo frames -> 48000/6 = 8000 Hz; left channel only
////    for (int i = startIndex; i < startIndex + (BUFFER_SIZE / 2); i += 12)
////    {
////        int16_t s = audioBuffer[i];                 // your "sample"
////        txbuf[k++] = (uint8_t)((s >> 8) + 128);     // 16-bit signed -> 0..255
////    }
////    HAL_UART_Transmit_IT(&huart1, txbuf, k);
////}

static void streamHalfToUart(int startIndex)
{
    static uint8_t txbuf[200];
    int k = 0;

    // Make sure we read what's really in RAM, not stale cached values
    SCB_InvalidateDCache_by_Addr((uint32_t*)&audioBuffer[startIndex],
                                 (BUFFER_SIZE / 2) * sizeof(int16_t));

    for (int i = startIndex; i < startIndex + (BUFFER_SIZE / 2); i += 12)
    {
        int16_t s = audioBuffer[i];
        txbuf[k++] = (uint8_t)((s >> 8) + 128);
    }
    HAL_UART_Transmit_IT(&huart1, txbuf, k);
}

extern "C" void BSP_AUDIO_OUT_HalfTransfer_CallBack(uint32_t Instance)
{
    streamHalfToUart(0);                  // first half just finished playing
}

extern "C" void BSP_AUDIO_OUT_TransferComplete_CallBack(uint32_t Instance)
{
    streamHalfToUart(BUFFER_SIZE / 2);    // second half just finished playing
}

static void silenceAudio()
{
    memset(audioBuffer, 0, sizeof(audioBuffer));
    SCB_CleanDCache_by_Addr((uint32_t*)audioBuffer, sizeof(audioBuffer));
}

void Screen1View::playNote(uint32_t frequency, const char* noteName)
{
    currentFrequency = shiftFreq(frequency);

    // Generate at full amplitude ONCE; envelope happens via codec volume
    generateSineWithAmplitude(currentFrequency, 28000.0f);

    envState   = ENV_ATTACK;
    envCounter = 0;
    minTicksRemaining = MIN_NOTE_TICKS;
    releasePending    = false;

    updateNoteAndFreqDisplay(noteName, currentFrequency);
}

Screen1View::Screen1View() {}

void Screen1View::setupScreen()
{
    Screen1ViewBase::setupScreen();

    audioOutInit.Device        = AUDIO_OUT_DEVICE_HEADPHONE;
    audioOutInit.ChannelsNbr   = 2;
    audioOutInit.SampleRate    = AUDIO_FREQUENCY_48K;
    audioOutInit.BitsPerSample = AUDIO_RESOLUTION_16B;
    audioOutInit.Volume        = currentVolume;
    BSP_AUDIO_OUT_Init(0, &audioOutInit);

    // Fill buffer with silence and start DMA running once.
    // After this, DMA loops forever; we change tones by rewriting the buffer.
    memset(audioBuffer, 0, sizeof(audioBuffer));
    SCB_CleanDCache_by_Addr((uint32_t*)audioBuffer, sizeof(audioBuffer));
    BSP_AUDIO_OUT_Play(0, (uint8_t*)audioBuffer, BUFFER_SIZE * 2);

    BSP_JOY_Init(JOY1, JOY_MODE_GPIO, JOY_ALL);

    updateOctaveDisplay();

    noteNameTextBuffer[0] = 0;
    noteNameText.invalidate();
    freqText_1Buffer[0] = 0;
    freqText_1.invalidate();

    Unicode::snprintf(volumeTextBuffer, VOLUMETEXT_SIZE, "%u", (unsigned int)currentVolume);
    volumeText.invalidate();
}

void Screen1View::tearDownScreen()
{
    BSP_AUDIO_OUT_Stop(0);
    BSP_AUDIO_OUT_DeInit(0);
    Screen1ViewBase::tearDownScreen();
}

void Screen1View::handleClickEvent(const touchgfx::ClickEvent& event)
{
    Screen1ViewBase::handleClickEvent(event);

    if (event.getType() == touchgfx::ClickEvent::RELEASED ||
        event.getType() == touchgfx::ClickEvent::CANCEL)
    {
        if (minTicksRemaining > 0)
        {
            releasePending = true;
        }
        else
        {
            envState   = ENV_RELEASE;
            envCounter = 0;
        }
    }
}

void Screen1View::handleTickEvent()
{
    Screen1ViewBase::handleTickEvent();

    // Poll joystick on every tick (~60Hz, plenty for human input)
    pollJoystick();

    if (minTicksRemaining > 0)
    {
        minTicksRemaining--;
        if (minTicksRemaining == 0 && releasePending)
        {
            envState   = ENV_RELEASE;
            envCounter = 0;
            releasePending = false;
        }
    }

    switch (envState)
    {
        case ENV_ATTACK:
        {
            envCounter++;
            // Ramp codec volume from 0 → currentVolume over ATTACK_TICKS
            uint8_t v = (uint8_t)((envCounter * currentVolume) / ATTACK_TICKS);
            if (envCounter >= ATTACK_TICKS) {
                v = currentVolume;
                envState = ENV_SUSTAIN;
            }
            BSP_AUDIO_OUT_SetVolume(0, v);
            break;
        }
        case ENV_RELEASE:
        {
            envCounter++;
            // Ramp codec volume from currentVolume → 0 over RELEASE_TICKS
            uint8_t v;
            if (envCounter >= RELEASE_TICKS) {
                v = 0;
                silenceAudio();   // buffer to zero so when we restore volume, no leftover sine
                envState = ENV_IDLE;
            } else {
                v = (uint8_t)(currentVolume - (envCounter * currentVolume) / RELEASE_TICKS);
            }
            BSP_AUDIO_OUT_SetVolume(0, v);
            break;
        }
        case ENV_SUSTAIN:
        case ENV_IDLE:
        default:
            break;
    }
}

void Screen1View::onVolumeSliderChanged(int value)
{
    currentVolume = (uint8_t)value;
    BSP_AUDIO_OUT_SetVolume(0, currentVolume);

    Unicode::snprintf(volumeTextBuffer, VOLUMETEXT_SIZE, "%u", (unsigned int)currentVolume);
    volumeText.invalidate();
}

void Screen1View::pollJoystick()
{
    int32_t state = BSP_JOY_GetState(JOY1, JOY_ALL);

    bool upPressed   = (state & JOY_UP)   != 0;
    bool downPressed = (state & JOY_DOWN) != 0;

    uint32_t currentState = (upPressed ? 1u : 0u) | (downPressed ? 2u : 0u);

    if (currentState == lastJoyState) return;
    lastJoyState = currentState;

    if (upPressed)
    {
        if (currentVolume <= VOL_MAX - VOL_STEP) currentVolume += VOL_STEP;
        else                                     currentVolume = VOL_MAX;
    }
    else if (downPressed)
    {
        if (currentVolume >= VOL_MIN + VOL_STEP) currentVolume -= VOL_STEP;
        else                                     currentVolume = VOL_MIN;
    }
    else
    {
        return;
    }

    BSP_AUDIO_OUT_SetVolume(0, currentVolume);

    // Move the slider visually to match
    volumeSlider.setValue(currentVolume);

    Unicode::snprintf(volumeTextBuffer, VOLUMETEXT_SIZE, "%u", (unsigned int)currentVolume);
    volumeText.invalidate();
}

void Screen1View::onOctaveUp()
{
    if (octaveShift < 2)
    {
        octaveShift++;
        updateOctaveDisplay();
    }
}

void Screen1View::onOctaveDown()
{
    if (octaveShift > -2)
    {
        octaveShift--;
        updateOctaveDisplay();
    }
}

void Screen1View::updateOctaveDisplay()
{
    int displayedOctave = 4 + octaveShift;
    Unicode::snprintf(octaveText_1Buffer, OCTAVETEXT_1_SIZE, "%d", displayedOctave);
    octaveText_1.invalidate();
}

void Screen1View::updateNoteAndFreqDisplay(const char* noteName, uint32_t freq)
{
    int displayedOctave = 4 + octaveShift;

    int i = 0;
    while (noteName[i] != '\0' && i < NOTENAMETEXT_SIZE - 2)
    {
        noteNameTextBuffer[i] = (touchgfx::Unicode::UnicodeChar)noteName[i];
        i++;
    }
    noteNameTextBuffer[i] = (touchgfx::Unicode::UnicodeChar)('0' + displayedOctave);
    i++;
    noteNameTextBuffer[i] = 0;

    noteNameText.invalidate();

    Unicode::snprintf(freqText_1Buffer, FREQTEXT_1_SIZE, "%u", (unsigned int)freq);
    freqText_1.invalidate();
}

// White keys octave 4
void Screen1View::onKeyPressed_C4()  { playNote(262, "C");  }
void Screen1View::onKeyPressed_D4()  { playNote(293, "D");  }
void Screen1View::onKeyPressed_E4()  { playNote(330, "E");  }
void Screen1View::onKeyPressed_F4()  { playNote(349, "F");  }
void Screen1View::onKeyPressed_G4()  { playNote(392, "G");  }
void Screen1View::onKeyPressed_A4()  { playNote(440, "A");  }
void Screen1View::onKeyPressed_B4()  { playNote(494, "B");  }

void Screen1View::onKeyPressed_C5()  { playNote(523, "C");  }
void Screen1View::onKeyPressed_D5()  { playNote(587, "D");  }
void Screen1View::onKeyPressed_E5()  { playNote(659, "E");  }
void Screen1View::onKeyPressed_F5()  { playNote(698, "F");  }
void Screen1View::onKeyPressed_G5()  { playNote(784, "G");  }
void Screen1View::onKeyPressed_A5()  { playNote(880, "A");  }
void Screen1View::onKeyPressed_B5()  { playNote(988, "B");  }

void Screen1View::onKeyPressed_Cs4() { playNote(277, "C#"); }
void Screen1View::onKeyPressed_Ds4() { playNote(311, "D#"); }
void Screen1View::onKeyPressed_Fs4() { playNote(370, "F#"); }
void Screen1View::onKeyPressed_Gs4() { playNote(415, "G#"); }
void Screen1View::onKeyPressed_As4() { playNote(466, "A#"); }

void Screen1View::onKeyPressed_Cs5() { playNote(554, "C#"); }
void Screen1View::onKeyPressed_Ds5() { playNote(622, "D#"); }
void Screen1View::onKeyPressed_Fs5() { playNote(740, "F#"); }
void Screen1View::onKeyPressed_Gs5() { playNote(831, "G#"); }
void Screen1View::onKeyPressed_As5() { playNote(932, "A#"); }
