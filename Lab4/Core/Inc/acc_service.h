#ifndef ACC_SERVICE_H
#define ACC_SERVICE_H

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "bluenrg_def.h"
#include "gatt_db.h"
#include "bluenrg_conf.h"
#include "bluenrg_gatt_aci.h"

extern uint16_t accServHandle, accCharHandle, freqCharHandle, smCharHandle;

tBleStatus Add_Acc_Service(void);
#endif
