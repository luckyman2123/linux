#ifndef _ODM_MSG_H_
#define _ODM_MSG_H_
/*
 *********************************************************************************
 *                          constants define
 *********************************************************************************
 */

typedef enum{
    ODM_MSG_ID_BEGIN = 0x00,
    ODM_MSG_ID_WDISABLE,
//<!-- [BugID]566 for SLM630V20 feature caogang@2015-07-13
    ODM_MSG_ID_USB_SUSPEND,
    ODM_MSG_ID_USB_RESUME,
    ODM_MSG_ID_GPIO_SUSPEND_SWITCH, 
    ODM_MSG_ID_GPIO_RESUME_SWITCH,
//end-->
//<!-- [BugID]566 for SLM630V1.31 led sleep caogang@2015-08-13
    ODM_MSG_ID_GPIO_APREADY_SWITCH,
//end-->
    ODM_MSG_ID_POWEROFF,
//<--[MEIG][AUTOSLEEP]-yanglonglong-20170829-manage wakeup source about module
    ODM_MSG_ID_WAKE_EVENTS,
//end-->
    /* Begin 14494 wangguanzhi for 720 mbim switch debug mode */
    ODM_MSG_ID_USB_SWITCH_DEBUG,
    ODM_MSG_ID_USB_SWITCH_DEBUG1,
    ODM_MSG_ID_USB_SWITCH_DEBUG2,
    /* End 14494 wangguanzhi for 720 mbim switch debug mode */
    ODM_MSG_ID_RTC_ALARM, // Added by lishihong for BugID 19741 on 2018-08-30
    ODM_MSG_ID_APREADY, //Adeed by wangguanzhi for BugID26296 750TE
    ODM_MSG_ID_UART_RI, //Adeed by wangguanzhi for BugID26296 750TE

    ODM_MSG_ID_END = 0xff
}odm_msg_id_type;

//<--[MEIG][AUTOSLEEP]-yanglonglong-20170830-manage wakeup source about module
/*event from qmi ,ODM_MSG_ID_QMI_MSG's sub msgs*/
typedef enum{
	SUB_QMI_WMS_NEW_SMS,	  /*there is an new short_msg comming*/
	SUB_QMI_VOICE_NEW_CALL,   /*the voice call coming*/
	SUB_QMI_VOICE_CALL_END,   /*the voice call end*/
	SUB_QMI_WDS_NEW_DATA,	  /*the tcp/udp data received*/
	SUB_WAKEIND_DONE,
	SUB_QMI_MAX
}sub_msg_qmi_id_type;
//end-->

/*
 *********************************************************************************
 *                          data types define
 *********************************************************************************
 */
typedef struct {
    unsigned char id; 	
    unsigned char val; 
	unsigned char data[62];
}odm_msg_type;
/*
 *********************************************************************************
 *                             functions 
 *********************************************************************************
 */
void odm_notify(odm_msg_type *msg, unsigned int len);
#endif
