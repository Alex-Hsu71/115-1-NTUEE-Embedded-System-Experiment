#include "acc_service.h"

uint16_t accServHandle, accCharHandle, freqCharHandle, smCharHandle;

static const uint8_t SM_CHAR_UUID[16] = {0x1b,0xc5,0xd5,0xa5,0x02,0x00,0xb4,0x9a,0xe1,0x11,0x04,0x00,0x00,0x00,0x00,0x00};
static const uint8_t ACC_SERV_UUID[16] = {0x1b,0xc5,0xd5,0xa5,0x02,0x00,0xb4,0x9a,0xe1,0x11,0x01,0x00,0x00,0x00,0x00,0x00};
static const uint8_t ACC_CHAR_UUID[16] = {0x1b,0xc5,0xd5,0xa5,0x02,0x00,0xb4,0x9a,0xe1,0x11,0x02,0x00,0x00,0x00,0x00,0x00};
static const uint8_t FREQ_CHAR_UUID[16]= {0x1b,0xc5,0xd5,0xa5,0x02,0x00,0xb4,0x9a,0xe1,0x11,0x03,0x00,0x00,0x00,0x00,0x00};

tBleStatus Add_Acc_Service(void)
{
  tBleStatus ret;
  ret = aci_gatt_add_serv(UUID_TYPE_128, ACC_SERV_UUID, PRIMARY_SERVICE, 1 + 3 + 2 + 3, &accServHandle);
  if (ret) return ret;

  ret = aci_gatt_add_char(accServHandle, UUID_TYPE_128, ACC_CHAR_UUID, 6,
                          CHAR_PROP_NOTIFY | CHAR_PROP_READ,
                          ATTR_PERMISSION_NONE, GATT_DONT_NOTIFY_EVENTS,
                          16, 0, &accCharHandle);
  if (ret) return ret;

  ret = aci_gatt_add_char(accServHandle, UUID_TYPE_128, FREQ_CHAR_UUID, 2,
                          CHAR_PROP_WRITE | CHAR_PROP_WRITE_WITHOUT_RESP | CHAR_PROP_READ,
                          ATTR_PERMISSION_NONE, GATT_NOTIFY_ATTRIBUTE_WRITE,
                          16, 0, &freqCharHandle);

  if (ret) return ret;
    ret = aci_gatt_add_char(accServHandle, UUID_TYPE_128, SM_CHAR_UUID, 2,
                            CHAR_PROP_NOTIFY | CHAR_PROP_READ,
                            ATTR_PERMISSION_NONE, GATT_DONT_NOTIFY_EVENTS,
                            16, 0, &smCharHandle);
  return ret;
}
