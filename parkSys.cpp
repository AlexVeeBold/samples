///////////////////////////////////////////////////////////////
//
// gtaParkSys: GTA parking system
//
//
//   10.07.2016 19:07 - created (moved from gssMain)
//   12.07.2016 21:44 - moving park system into a class (and refactoring a little)
//

// (partial source) //

void SSPSX::parkSysUpdateRegistry(void)
{
    DWORD iPark;
    PARKINGLOT* pPark;
    for(iPark = 0; iPark < this->numParks; iPark++)
    {
        pPark = &this->park[iPark];
        memcpy(pPark->pVehInfoRegistry, pPark->vehLicense, pPark->vehInfoRegistrySize);
    }
    fileStore(this->srcDataPtr, this->strDataFileName.c_str(), this->srcDataSize);
}

void SSPSX::parkingSpaceTestVehicle(PARKSPACEINFO* pSpace, Vehicle hVehicle)
{
    DWORD model;
    BYTE vehTypes;
    model = VehicleGetModel(hVehicle);
    vehTypes = VehicleGetTypes(model);
    if((vehTypes & pSpace->vehTypesAllowed) != 0)
    {
        pSpace->hVehicleSaving = hVehicle;  // temporary save player vehicle [no release need]
        pSpace->state = PSS_STORING_ALLOWED;
    }
    else
    {
        pSpace->state = PSS_STORING_DENIED;
    }
}

void SSPSX::parkingSpaceScanVehicle(PARKSPACEINFO* pSpace, VEHLIC31* pVehLicense)
{
    VehicleLicenseFill(pVehLicense, pSpace->hVehicleSaving);
    this->parkSysUpdateRegistry();
    if(VehicleModelHasAlarm(pVehLicense->model) != FALSE)
    {
        VehicleDoAlarmShort(pSpace->hVehicleSaving);
    }
    pSpace->hVehicleSaving = NULL;
    pSpace->hVehicleStored = NULL;  // no need to release -> no need to save handle
    pSpace->state = PSS_RESERVED;
}

void SSPSX::parkingSpaceClear(PARKSPACEINFO* pSpace, VEHLIC31* pVehLicense)
{
    VehicleSafeRelease(&pSpace->hVehicleStored);    // release vehicle if it was created by parking lot (NULL value is okay)
    if(pVehLicense != NULL)
    {
        VehicleLicenseClear(pVehLicense);
        this->parkSysUpdateRegistry();
    }
    pSpace->hVehicleSaving = NULL;
    pSpace->state = PSS_EMPTY;
}

void SSPSX::parkingSpaceFSM(AStringStream& display, DWORD iSpace, GtaPlayerState& playerState)
{
    PARKSPACEINFO* pSpace;
    VEHLIC31* pVehLicense;
    FLOAT dist;
    FLOAT parkSpaceRadius;// = 1.875f;
    BOOL bCancelClear = TRUE;
    //
    pSpace = &this->pParkActive->space[iSpace];
    pVehLicense = &this->pParkActive->vehLicense[iSpace];
    //
    parkSpaceRadius = 1.0f * pSpace->markerRadiusMultiplier;
    //
    switch(pSpace->state)
    {
    case PSS_EMPTY:
        if(playerState.bIsInCar)
        {
            // if player arrived to marker in a vehicle...
            parkingDrawMarker(PSM_AVAILABLE, pSpace->markerRadiusMultiplier, iSpace, &pSpace->pos);
            dist = distance3d(pSpace->pos, playerState.fvPosVehicle);
            if(dist < parkSpaceRadius)
            {
                // can it be stor'd heer?
                this->parkingSpaceTestVehicle(pSpace, playerState.vehiclePlayer);
            }
        }
        break;
    case PSS_STORING_ALLOWED:
        if(playerState.bIsInCar)
        {
            // if player leaved our marker in a vehicle...
            parkingDrawMarker(PSM_VEH_APPROVED, pSpace->markerRadiusMultiplier, iSpace, &pSpace->pos);
            dist = distance3d(pSpace->pos, playerState.fvPosVehicle);
            if(dist >= parkSpaceRadius)
            {
                this->parkingSpaceClear(pSpace, NULL);
            }
        }
        else    // ...or he got out and leaved vehicle for us to scan?
        {
            this->parkingSpaceScanVehicle(pSpace, pVehLicense);
        }
        break;
    case PSS_STORING_DENIED:
        if(playerState.bIsInCar)
        {
            // player is in inappropriate vehicle... is he leaved (in a vehicle)?
            parkingDrawMarker(PSM_VEH_REJECTED, pSpace->markerRadiusMultiplier, iSpace, &pSpace->pos);
            dist = distance3d(pSpace->pos, playerState.fvPosVehicle);
            if(dist >= parkSpaceRadius)
            {
                this->parkingSpaceClear(pSpace, NULL);
            }
        }
        else    // ...or he got out and we won't scan this definitely inappropriate vehicle?
        {
            this->parkingSpaceClear(pSpace, NULL);
        }
        break;
    case PSS_RESERVED:
        if(playerState.bIsInCar)
        {
            // the player is in car on a place that has a vehicle scanned... player wants to clear it?
            dist = distance3d(pSpace->pos, playerState.fvPosVehicle);
            if(dist < parkSpaceRadius)
            {
                parkingDrawMarker(PSM_CLEAR_START, pSpace->markerRadiusMultiplier, iSpace, &pSpace->pos);
                if(gtaIsPlayerPressingHorn() != FALSE)
                {
                    pSpace->state = PSS_CLEARING1;
                }
            }
        }
        break;
    case PSS_CLEARING1:
        if(playerState.bIsInCar)
        {
            // the player is still in car on a place that has a vehicle scanned... player wants to clear it for sure?
            dist = distance3d(pSpace->pos, playerState.fvPosVehicle);
            if(dist < parkSpaceRadius)
            {
                bCancelClear = FALSE;
                if(gtaIsPlayerPressingHorn() == FALSE)
                {
                    pSpace->state = PSS_CLEARING2;
                }
            }
        }
        if(bCancelClear != FALSE)   // ...or he changed his mind?
        {
            pSpace->state = PSS_RESERVED;
        }
        break;
    case PSS_CLEARING2:
        if(playerState.bIsInCar)
        {
            // the player is still in car on a place that has a vehicle scanned & wants to clear it...
            dist = distance3d(pSpace->pos, playerState.fvPosVehicle);
            if(dist < parkSpaceRadius)
            {
                bCancelClear = FALSE;
                parkingDrawMarker(PSM_CLEAR_CONFIRM, pSpace->markerRadiusMultiplier, iSpace, &pSpace->pos);
                if(gtaIsPlayerPressingHorn() != FALSE)
                {
                    this->parkingSpaceClear(pSpace, pVehLicense);
                }
            }
        }
        if(bCancelClear != FALSE)   // ...or he changed his mind in a last second?
        {
            pSpace->state = PSS_RESERVED;
        }
        break;
    }
}

void SSPSX::parkingLotInitializeActivation(void)
{
    // request all models
    PARKINGLOT* pPark;
    DWORD iSpace;
    PARKSPACEINFO* pSpace;
    DWORD model;
    DWORD spaceState;
//    pps->iParkActive = iParkingLot;
//    pps->mdlQueue.mqLength = 0;
    lss << UL::DEBUG << L("plInit: mqInit") << UL::ENDL;
    mqInit(&this->mdlQueue);
    lss << UL::DEBUG << L("plInit: clearArea") << UL::ENDL;
    this->parkingLotClearArea();
    lss << UL::DEBUG << L("plInit: queue models") << UL::ENDL;
    pPark = &this->park[this->iParkActive];
    for(iSpace = 0; iSpace < pPark->numSpaces; iSpace++)
    {
        pSpace = &pPark->space[iSpace];
        model = pPark->vehLicense[iSpace].model;
        spaceState = PSS_EMPTY;
        if(model != 0)
        {
            lss << UL::DEBUG << L("plInit: ") << iSpace << " : " << ulhex(model) << UL::ENDL;
            spaceState = PSS_RESERVED;
            pSpace->mqIndex = mqEnqueueModel(&this->mdlQueue, model);
        }
        pSpace->state = spaceState;
        pSpace->hVehicleSaving = NULL;
        pSpace->hVehicleStored = NULL;
    }
    lss << UL::DEBUG << L("plInit: done") << UL::ENDL;
    this->pParkActive = pPark;
    this->state = PLS_ACTIVATING;
}

BOOL SSPSX::parkingLotFinalizeActivation(void)
{
    PARKINGLOT* pPark;
    DWORD iSpace;
    PARKSPACEINFO* pSpace;
    VEHLIC31* pVehLicense;
    BOOL bFinishedLoading;
    lss << UL::DEBUG << L("plFinInit: 1") << UL::ENDL;
    pPark = this->pParkActive;
    bFinishedLoading = (mqAreModelsStillLoading(&this->mdlQueue) == FALSE);
    for(iSpace = 0; iSpace < pPark->numSpaces; iSpace++)
    {
        pSpace = &pPark->space[iSpace];
        pVehLicense = &pPark->vehLicense[iSpace];
        if(pSpace->state == PSS_RESERVED)
        {
            if(this->mdlQueue.mqEntry[pSpace->mqIndex].state == MDS_LOADED)
            {
                pSpace->hVehicleStored = VehicleCreate(pVehLicense, &pSpace->pos, pSpace->angle);
            }
        }
    }
    lss << UL::DEBUG << L("plFinInit: 2") << UL::ENDL;
    if(bFinishedLoading == FALSE)
    {
        mqReleaseLoadedModels(&this->mdlQueue);
    }
    else
    {
        this->state = PLS_ACTIVE;
    }
    return bFinishedLoading;
}

void SSPSX::parkingLotDeactivate(void)
{
    PARKINGLOT* pPark;
    DWORD iSpace;
    PARKSPACEINFO* pSpace;
    pPark = this->pParkActive;
    for(iSpace = 0; iSpace < pPark->numSpaces; iSpace++)
    {
        pSpace = &pPark->space[iSpace];
        switch(pSpace->state)
        {
        case PSS_RESERVED:
            this->parkingSpaceClear(pSpace, NULL);      // release all created vehicles, so they can be destroyed by the game
            break;
        }
    }
    this->pParkActive = NULL;
    this->iParkActive = 0;
    this->state = PLS_INACTIVE;
}

void SSPSX::parkFSM(AStringStream& display, GtaPlayerState& playerState)
{
    DWORD iPark;
    DWORD iSpace;
    PARKINGLOT* pPark;
    DWORD numParkSpaces;
    switch(this->state)
    {
    case PLS_INACTIVE:
        display << " inactive";
        for(iPark = 0; iPark < this->numParks; iPark++)
        {
            pPark = &this->park[iPark];
            if(zobbInside3d(playerState.fvPosChar, pPark->bbEnter))
            {
                lss << UL::INFO << L("player entered spawn zone [") << iPark << L("]") << UL::ENDL;
                this->iParkActive = iPark;
                this->parkingLotInitializeActivation();
            }
        }
        break;
    case PLS_ACTIVATING:
        if(this->parkingLotFinalizeActivation() != FALSE)
        {
            lss << UL::INFO << L("spawn finished") << UL::ENDL;
        }
        break;
    case PLS_ACTIVE:
        display << " active(" << this->iParkActive << ")";
        pPark = this->pParkActive;
        if(zobbInside3d(playerState.fvPosChar, pPark->bbLeave))
        {
            numParkSpaces = pPark->numSpaces;
            for(iSpace = 0; iSpace < numParkSpaces; iSpace++)
            {
                this->parkingSpaceFSM(display, iSpace, playerState);
            }
        }
        else
        {
            lss << UL::INFO << L("player left park zone") << UL::ENDL;
            this->parkingLotDeactivate();
        }
        break;
    }
}

