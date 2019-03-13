// Bus Raider Machine Base Class
// Rob Dobson 2018

#pragma once
#include <stdint.h>

static const int MC_WINDOW_NUMBER = 0;

struct WgfxFont;

class McDescriptorTable
{
public:
    enum PROCESSOR_TYPE { PROCESSOR_Z80 };

public:
    // Name
    const char* machineName;
    // Processor type
    PROCESSOR_TYPE processorType;
    // Display
    int displayRefreshRatePerSec;
    int displayPixelsX;
    int displayPixelsY;
    int displayCellX;
    int displayCellY;
    int pixelScaleX;
    int pixelScaleY;
    WgfxFont* pFont;
    int displayForeground;
    int displayBackground;
    // Clock
    uint32_t clockFrequencyHz;
    // Interrupt rate per second
    uint32_t irqRate;
    // Bus monitor modes
    bool monitorIORQ;
    bool monitorMREQ;
    bool emulatedRAM;
    uint32_t emulatedRAMStart;
    uint32_t emulatedRAMLen;
    bool setRegistersByInjection;
    uint32_t setRegistersCodeAddr;
};

class McBase
{
public:

    McBase();

    // Enable machine
    virtual void enable(int subType) = 0;

    // Disable machine
    virtual void disable() = 0;

    // Get number of descriptor tables for machine
    virtual int getDescriptorTableCount()
    {
        return 1;
    }

    // Get descriptor table for the machine (-1 for current subType)
    virtual McDescriptorTable* getDescriptorTable(int subType = -1) = 0;

    // Handle display refresh (called at a rate indicated by the machine's descriptor table)
    virtual void displayRefresh() = 0;

    // Handle reset for the machine - if false returned then the bus raider will issue a hardware reset
    virtual bool reset([[maybe_unused]] bool holdInReset = false)
    {
        return false;
    }

    // Handle a key press
    virtual void keyHandler(unsigned char ucModifiers, const unsigned char rawKeys[6]) = 0;

    // Handle a file
    virtual void fileHandler(const char* pFileInfo, const uint8_t* pFileData, int fileLen) = 0;

    // Handle debugger command
    virtual bool debuggerCommand(char* pCommand, char* pResponse, int maxResponseLen);
};
