// Bus Raider
// Rob Dobson 2019

#pragma once

#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdlib.h>

#include "../TargetBus/TargetCPU.h"

class CommandHandler;
class BusAccess;
class HwManager;

class BusControlAPI
{
public:
    static const int MAX_MEM_BLOCK_READ_WRITE = 1024;

    BusControlAPI(CommandHandler& commandHandler, HwManager& hwManager, BusAccess& busAccess);
    void init();

    // Service
    void service();

    // Bus access to data
    BR_RETURN_TYPE blockAccessSync(uint32_t addr, uint8_t* pData, uint32_t len, bool iorq, bool write);

private:

    // Singleton instance
    static BusControlAPI* _pThisInstance;

    // Command handler
    CommandHandler& _commandHandler;

    // HwManager
    HwManager& _hwManager;

    // Bus access
    BusAccess& _busAccess;

//#define USE_TARGET_TRACKER
#ifdef USE_TARGET_TRACKER
    // Target tracker
    TargetTracker& _targetTracker;
#endif

    // Bus socket we're attached to and setup info
    int _busSocketId;

    // Comms socket we're attached to and setup info
    int _commsSocketId;

    // Handle messages (telling us to start/stop)
    static bool handleRxMsgStatic(void* pObject, const char* pCmdJson, const uint8_t* pParams, unsigned paramsLen,
                    char* pRespJson, unsigned maxRespLen);
    bool handleRxMsg(const char* pCmdJson, const uint8_t* pParams, unsigned paramsLen,
                    char* pRespJson, unsigned maxRespLen);

    // Bus action complete callback
    static void busActionCompleteStatic(void* pObject, BR_BUS_ACTION actionType, BR_BUS_ACTION_REASON reason);
    void busActionComplete(BR_BUS_ACTION actionType, BR_BUS_ACTION_REASON reason);

    // Wait interrupt handler
    static void handleWaitInterruptStatic(void* pObject, uint32_t addr, uint32_t data, 
            uint32_t flags, uint32_t& retVal);
    void handleWaitInterrupt(uint32_t addr, uint32_t data, 
            uint32_t flags, uint32_t& retVal);

    // Helpers
    bool busLineHandler(const char* pCmdJson);
    bool muxLineHandler(const char* pCmdJson);
    void busLinesRead(char* pRespJson, int maxRespLen);
    bool getArg(const char* argName, int argNum, const char* pCmdJson, 
                uint32_t& value, char* pOutStr = NULL, uint32_t maxOutStrLen = 0,
                bool forceDecimal = false);

    // Synchronous bus access
    uint8_t _memAccessDataBuf[MAX_MEM_BLOCK_READ_WRITE];
    bool _memAccessPending;
    bool _memAccessWrite;
    uint32_t _memAccessDataLen;
    uint32_t _memAccessAddr;
    bool _memAccessIo;
    uint32_t _memAccessRdWrErrCount;
    static const int MAX_RDWR_ERR_STR_LEN = 200;
    char _memAccessRdWrErrStr[MAX_RDWR_ERR_STR_LEN];
    bool _memAccessRdWrTest;

    // Comms
    bool _stepCompletionPending;
    bool _targetTrackerResetPending;
};
