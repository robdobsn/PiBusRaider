// Bus Raider
// Rob Dobson 2018-2020

#include "BusControl.h"
#include "PiWiring.h"
#include <circle/interrupt.h>
#include "lowlib.h"
#include <circle/bcm2835.h>
#include "logging.h"
#include "circle/timer.h"

// Module name
static const char MODULE_PREFIX[] = "BusControl";

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Constructor
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

BusControl::BusControl()
    : _targetControl(*this), 
      _busSocketManager(*this), 
      _memoryController(*this),
      _busRawAccess(_clockGenerator)
{
    // Not init yet
    _isInitialized = false;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Initialise the hardware
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void BusControl::init()
{
    if (!_isInitialized)
    {
        // Clock
        _clockGenerator.setup();
        _clockGenerator.setFreqHz(1000000);
        _clockGenerator.enable(true);
        
        // Handlers
        _busRawAccess.init();
        _targetControl.init();
        _busSocketManager.init();

        // Now initialized
        _isInitialized = true;
    }
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Service
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void BusControl::service()
{
    // Service raw access
    _busRawAccess.service();

    // Service target controller
    _targetControl.service();
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Machine changes
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void BusControl::machineChangeInit()
{
    // Suspend socket manager
    _busSocketManager.suspend(true, true);

    // Clear the target controller
    _targetControl.clear();
}

void BusControl::machineChangeComplete()
{
    // Resume socket manager
    _busSocketManager.suspend(false, true);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Raw access start/end
// Used by self-test to suspend normal bus operation (wait handling etc)
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void BusControl::rawAccessStart()
{
    _busSocketManager.suspend(true, true);
    _targetControl.suspend(true);
}

void BusControl::rawAccessEnd()
{
    _busSocketManager.suspend(false, true);
    _targetControl.suspend(false);
}
