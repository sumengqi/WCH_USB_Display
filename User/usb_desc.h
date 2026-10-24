#ifndef USER_USB_DESC_H_
#define USER_USB_DESC_H_

#include "debug.h"
#include "project_config.h"

#define DEF_USB_VID                  USB_DISPLAY_VID
#define DEF_USB_PID                  USB_DISPLAY_PID
#define DEF_IC_PRG_VER               USB_DISPLAY_DEVICE_RELEASE

#define DEF_USBD_UEP0_SIZE           64
#define DEF_USBD_HS_PACK_SIZE        512
#define DEF_USBD_FS_PACK_SIZE        64

#define DEF_USB_EP1_HS_SIZE          DEF_USBD_HS_PACK_SIZE
#define DEF_USB_EP2_HS_SIZE          DEF_USBD_HS_PACK_SIZE
#define DEF_USB_EP3_HS_SIZE          800
#define DEF_USB_EP4_HS_SIZE          DEF_USBD_HS_PACK_SIZE
#define DEF_USB_EP5_HS_SIZE          DEF_USBD_HS_PACK_SIZE
#define DEF_USB_EP6_HS_SIZE          DEF_USBD_HS_PACK_SIZE
#define DEF_USB_EP1_FS_SIZE          DEF_USBD_FS_PACK_SIZE
#define DEF_USB_EP2_FS_SIZE          DEF_USBD_FS_PACK_SIZE
#define DEF_USB_EP3_FS_SIZE          800
#define DEF_USB_EP4_FS_SIZE          DEF_USBD_FS_PACK_SIZE
#define DEF_USB_EP5_FS_SIZE          DEF_USBD_FS_PACK_SIZE
#define DEF_USB_EP6_FS_SIZE          DEF_USBD_FS_PACK_SIZE

#define DEF_USBD_DEVICE_DESC_LEN     ((uint8_t)MyDevDescr[0])
#define DEF_USBD_CONFIG_FS_DESC_LEN  ((uint16_t)MyCfgDescr_FS[2] | ((uint16_t)MyCfgDescr_FS[3] << 8))
#define DEF_USBD_CONFIG_HS_DESC_LEN  ((uint16_t)MyCfgDescr_HS[2] | ((uint16_t)MyCfgDescr_HS[3] << 8))
#define DEF_USBD_REPORT_DESC_LEN     0
#define DEF_USBD_LANG_DESC_LEN       ((uint16_t)MyLangDescr[0])
#define DEF_USBD_MANU_DESC_LEN       ((uint16_t)MyManuInfo[0])
#define DEF_USBD_PROD_DESC_LEN       ((uint16_t)MyProdInfo[0])
#define DEF_USBD_SN_DESC_LEN         ((uint16_t)MySerNumInfo[0])
#define DEF_USBD_QUALFY_DESC_LEN     ((uint16_t)MyQuaDesc[0])
#define DEF_USBD_BOS_DESC_LEN        0
#define DEF_USBD_FS_OTH_DESC_LEN     DEF_USBD_CONFIG_HS_DESC_LEN
#define DEF_USBD_HS_OTH_DESC_LEN     DEF_USBD_CONFIG_FS_DESC_LEN

extern const uint8_t MyDevDescr[];
extern const uint8_t MyCfgDescr_FS[];
extern const uint8_t MyCfgDescr_HS[];
extern const uint8_t MyLangDescr[];
extern const uint8_t MyManuInfo[];
extern const uint8_t MyProdInfo[];
extern uint8_t MySerNumInfo[];
extern const uint8_t MyQuaDesc[];
extern uint8_t TAB_USB_FS_OSC_DESC[];
extern uint8_t TAB_USB_HS_OSC_DESC[];

void USB_Display_SetSerial(const char *serial);

#endif
