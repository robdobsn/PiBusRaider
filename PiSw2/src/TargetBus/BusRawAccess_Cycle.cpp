// Bus Raider
// Rob Dobson 2018-2019

#include "BusRawAccess.h"
#include "PiWiring.h"
#include "lowlev.h"
#include "lowlib.h"
#include <circle/bcm2835.h>
#include "BusSocketInfo.h"

// Module name
static const char MODULE_PREFIX[] = "BusRawCycle";

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Cycle clear
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void BusRawAccess::cycleClear()
{
    _cycleReqInfo = NULL;
    _cycleReqCompleteCB = NULL;
    _cycleReqPObject = NULL;
    _cycleReqSlotIdx = 0;
    _cycleReqState = CYCLE_REQ_STATE_NONE;
    _cycleReqActionType = BR_BUS_ACTION_NONE;
    _cycleReqUs = 0;
    _cycleReqAssertedUs = 0;
    _cycleReqMaxUs = 0;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Req cycle action
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void BusRawAccess::cycleReqAction(BusSocketInfo& busSocketInfo, 
            uint32_t maxDurationUs,
            BusCompleteCBFnType* cycleCompleteCB, 
            void* pObject,
            uint32_t slotIdx)
{
    _cycleReqInfo = &busSocketInfo;
    _cycleReqCompleteCB = cycleCompleteCB;
    _cycleReqPObject = pObject;
    _cycleReqSlotIdx = slotIdx;
    _cycleReqState = CYCLE_REQ_STATE_PENDING;
    _cycleReqActionType = busSocketInfo.getType();
    _cycleReqUs = micros();
    _cycleReqAssertedUs = 0;
    _cycleReqMaxUs = maxDurationUs; 
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Cycle service
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void BusRawAccess::cycleService()
{
    // Check for cycle requests pending
    if (_cycleReqState == CYCLE_REQ_STATE_PENDING)
    {
        // TODO 2020 - check if we can assert the bus action - maybe need more conditions
        if (!_waitIsActive)
        {
            // Initiate the cycle action
            cycleSetSignal(_cycleReqActionType, true);
            _cycleReqAssertedUs = micros();
            _cycleReqState = CYCLE_REQ_STATE_ASSERTED;
        }
        else
        {
            // Check for timeout
            if (isTimeout(micros(), _cycleReqUs, MAX_WAIT_FOR_PENDING_ACTION_US))
            {
                // Cancel the request
                // LogWrite(MODULE_PREFIX, LOG_DEBUG, 
                //          "Timeout on bus action %u type %d _waitIsActive %d", 
                //          micros(), _cycleReqActionType, _waitIsActive);
                cycleReqCompleted(BR_NOT_HANDLED);
            }
        }
    }
    else if (_cycleReqState == CYCLE_REQ_STATE_ASSERTED)
    {
        if (_cycleReqActionType == BR_BUS_ACTION_BUSRQ)
            cycleReqAssertedBusRq();
        else if (_cycleReqActionType == BR_BUS_ACTION_IRQ)
            cycleReqAssertedIRQ();
        else
            cycleReqAssertedOther();
    }
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Cycle helpers
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void BusRawAccess::cycleReqCompleted(BR_RETURN_TYPE result)
{
    _cycleReqState = CYCLE_REQ_STATE_NONE;
    if (_cycleReqCompleteCB)
    {
        _cycleReqCompleteCB(_cycleReqPObject, _cycleReqSlotIdx, result);
    }
}

void BusRawAccess::cycleReqAssertedBusRq()
{
    // Check for BUSACK
    if (rawBUSAKActive())
    {
        // Take control of bus
        busReqTakeControl();

        // Callback on completion now so that any new action raised by the callback
        // such as a reset, etc can be asserted before BUSRQ is released
        cycleReqCompleted(BR_OK);

        // Release bus
        busReqRelease();
    }
    else
    {
        // Check for timeout on BUSACK
        if (isTimeout(micros(), _cycleReqAssertedUs, _cycleReqMaxUs))
        {
            // For bus requests a timeout means failure
            cycleReqCompleted(BR_NOT_HANDLED);
            cycleSetSignal(BR_BUS_ACTION_BUSRQ, false);
        }
    }
}

void BusRawAccess::cycleReqAssertedIRQ()
{

}

void BusRawAccess::cycleReqAssertedOther()
{

}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Cycle set signal
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void BusRawAccess::cycleSetSignal(BR_BUS_ACTION busAction, bool assert)
{
    switch (busAction)
    {
        case BR_BUS_ACTION_RESET: 
            assert ? muxSet(BR_MUX_RESET_Z80_BAR_LOW) : muxClear();
            // LogWrite(MODULE_PREFIX, LOG_DEBUG, "RESET"); 
            // ISR_VALUE(ISR_ASSERT_CODE_DEBUG_E, assert);
            break;
        case BR_BUS_ACTION_NMI: 
            assert ? muxSet(BR_MUX_NMI_BAR_LOW) : muxClear(); 
            // ISR_VALUE(ISR_ASSERT_CODE_DEBUG_F, assert);
            break;
        case BR_BUS_ACTION_IRQ: 
            assert ? muxSet(BR_MUX_IRQ_BAR_LOW) : muxClear(); 
            // LogWrite(MODULE_PREFIX, LOG_DEBUG, "IRQ"); 
            // ISR_VALUE(ISR_ASSERT_CODE_DEBUG_I, assert);
            break;
        case BR_BUS_ACTION_BUSRQ: 
            // ISR_VALUE(ISR_ASSERT_CODE_DEBUG_B, assert);
            assert ? busReqStart() : busReqRelease(); 
            break;
        default: break;
    }
}
