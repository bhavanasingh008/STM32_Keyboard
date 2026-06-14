#ifndef SCREEN1VIEW_HPP
#define SCREEN1VIEW_HPP

#include <gui_generated/screen1_screen/Screen1ViewBase.hpp>
#include <gui/screen1_screen/Screen1Presenter.hpp>

class Screen1View : public Screen1ViewBase
{
public:
    Screen1View();
    virtual ~Screen1View() {}
    virtual void setupScreen();
    virtual void tearDownScreen();

    virtual void handleClickEvent(const touchgfx::ClickEvent& event);
    virtual void handleTickEvent();

    virtual void onOctaveUp();
    virtual void onOctaveDown();

    void pollJoystick();
    void updateVolumeBar();

    // White keys octave 4
    virtual void onKeyPressed_C4();
    virtual void onKeyPressed_D4();
    virtual void onKeyPressed_E4();
    virtual void onKeyPressed_F4();
    virtual void onKeyPressed_G4();
    virtual void onKeyPressed_A4();
    virtual void onKeyPressed_B4();

    // White keys octave 5
    virtual void onKeyPressed_C5();
    virtual void onKeyPressed_D5();
    virtual void onKeyPressed_E5();
    virtual void onKeyPressed_F5();
    virtual void onKeyPressed_G5();
    virtual void onKeyPressed_A5();
    virtual void onKeyPressed_B5();

    // Black keys octave 4
    virtual void onKeyPressed_Cs4();
    virtual void onKeyPressed_Ds4();
    virtual void onKeyPressed_Fs4();
    virtual void onKeyPressed_Gs4();
    virtual void onKeyPressed_As4();

    // Black keys octave 5
    virtual void onKeyPressed_Cs5();
    virtual void onKeyPressed_Ds5();
    virtual void onKeyPressed_Fs5();
    virtual void onKeyPressed_Gs5();
    virtual void onKeyPressed_As5();

    virtual void onVolumeSliderChanged(int value);

protected:

private:
    void playNote(uint32_t frequency, const char* noteName);
    void updateOctaveDisplay();
    void updateNoteAndFreqDisplay(const char* noteName, uint32_t freq);
};

#endif // SCREEN1VIEW_HPP
