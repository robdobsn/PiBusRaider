// Bus Raider Machine Serial Terminal
// Rob Dobson 2018

#include "McBase.h"
#include "McManager.h"
#include "../System/logging.h"
#include "../TerminalEmulation/TermEmu.h"

class McTerminal : public McBase
{
private:
    static const char* _logPrefix;
    uint8_t _screenCache[TermEmu::MAX_ROWS * TermEmu::MAX_COLS];
    bool _screenCacheDirty;
    uint32_t _cursorBlinkLastUs;
    uint32_t _cursorBlinkRateMs;
    bool _cursorIsShown;
    TermCursor _cursorInfo;

    static McDescriptorTable _defaultDescriptorTables[];

    // Shifted digits on keyboard
    static const int SHIFT_DIGIT_KEY_MAP_LEN = 10;
    static int _shiftDigitKeyMap[SHIFT_DIGIT_KEY_MAP_LEN];

    // Terminal emulation maintains an in-memory image of the screen
    TermEmu* _pTerminalEmulation;

public:

    McTerminal();

    // Enable machine
    virtual void enable();

    // Disable machine
    virtual void disable();

    // Handle display refresh (called at a rate indicated by the machine's descriptor table)
    virtual void displayRefreshFromMirrorHw();

    // Handle a key press
    virtual void keyHandler(unsigned char ucModifiers, const unsigned char rawKeys[6]);

    // Handle a file
    virtual bool fileHandler(const char* pFileInfo, const uint8_t* pFileData, int fileLen);

    // Handle a request for memory or IO - or possibly something like in interrupt vector in Z80
    virtual void busAccessCallback(uint32_t addr, uint32_t data, uint32_t flags, uint32_t& retVal);

    // Bus action complete callback
    virtual void busActionCompleteCallback(BR_BUS_ACTION actionType);

    // Convert raw USB code to ASCII
    static int convertRawToAscii(unsigned char ucModifiers, const unsigned char rawKeys[6]);
};
