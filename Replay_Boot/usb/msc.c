#include "msc.h"
#include "msc_descriptor.h"
#include "usb_hardware.h"
#include "messaging.h"

#include "card.h"

void UsbPacketReceived(uint8_t *data, int len);

// The buffer used to store packets received over USB while we are in the
// process of receiving them.
static uint8_t UsbBuffer[512] __attribute__((aligned(8)));

// The number of characters received in the USB buffer so far.
static int  UsbSoFarCount;

static uint8_t CurrentConfiguration;

static void HandleRxdSetupData(UsbSetupPacket* data);
static void HandleRxdData(uint8_t ep, uint8_t* packet, uint32_t length);

static void msc_reset();
static void msc_packet_recv(uint8_t* packet, uint32_t length);


void msc_recv(uint8_t ep, uint8_t* packet, uint32_t length)
{
;;//    DEBUG(1,"ptp_recv (ep = %d; packet = %08x, length = %d", ep, packet, length);
	if (ep == 0) {
		if (length != sizeof(UsbSetupPacket)) {
	        DEBUG(1,"Setup packet length error!");
		}
		UsbSetupPacket* data = (UsbSetupPacket*)packet;
		HandleRxdSetupData(data);
	} else if (ep == 2) {
		HandleRxdData(ep, packet, length);
    } else {
        ERROR("unknown endpoint!");
    }
}

void msc_send(uint8_t* packet, uint32_t length)
{
	usb_send(1, 64, packet, length, length);
}

void MSC_Start(void)
{
    msc_reset();
	usb_connect(msc_recv);
}
void MSC_Stop(void)
{
	usb_disconnect();
}
uint8_t MSC_Poll(void)
{
	return usb_poll();
}

static void HandleRxdSetupData(UsbSetupPacket* data)
{
    UsbSetupPacket usd;
    memcpy(&usd, data, sizeof(usd));

    DEBUG(1,"--- RXSETUP:");
    DEBUG(1,"usd.bmRequestType = %02X", usd.bmRequestType);
    DEBUG(1,"usd.bRequest      = %02X", usd.bRequest);
    DEBUG(1,"usd.wValue        = %04X", usd.wValue);
    DEBUG(1,"usd.wIndex        = %04X", usd.wIndex);
    DEBUG(1,"usd.wLength       = %04X", usd.wLength);

    switch(usd.bRequest) {
        case USB_REQUEST_GET_DESCRIPTOR:
            DEBUG(1,"\tUSB_REQUEST_GET_DESCRIPTOR");
            if((usd.wValue >> 8) == USB_DESCRIPTOR_TYPE_DEVICE) {
                DEBUG(1,"\t\tUSB_DESCRIPTOR_TYPE_DEVICE");
                usb_send_ep0((uint8_t *)&DeviceDescriptor,
                    sizeof(DeviceDescriptor), usd.wLength);
            } else if((usd.wValue >> 8) == USB_DESCRIPTOR_TYPE_CONFIGURATION) {
                DEBUG(1,"\t\tUSB_DESCRIPTOR_TYPE_CONFIGURATION");
                usb_send_ep0((uint8_t *)&ConfigurationDescriptor,
                    sizeof(ConfigurationDescriptor), usd.wLength);
//            } else if((usd.wValue >> 8) == USB_DESCRIPTOR_TYPE_DEVICE_QUALIFIER) {
//                DEBUG(1,"\t\tUSB_DESCRIPTOR_TYPE_DEVICE_QUALIFIER");
//                usb_send_ep0((uint8_t *)&DeviceQualifierDescriptor,
//                    sizeof(DeviceQualifierDescriptor), usd.wLength);
            } else if((usd.wValue >> 8) == USB_DESCRIPTOR_TYPE_STRING) {
                DEBUG(1,"\t\tUSB_DESCRIPTOR_TYPE_STRING");
                const uint8_t *s = StringDescriptors[usd.wValue & 0xff];
                if ((usd.wValue & 0xff) == 0xee)
                    usb_send_ep0(StringDescriptorEE, StringDescriptorEE[0], usd.wLength);
                else if ((usd.wValue & 0xff) < sizeof(StringDescriptors) / sizeof(StringDescriptors[0]))
                    usb_send_ep0(s, s[0], usd.wLength);
                else
                    usb_send_ep0_stall();
            } else {
                DEBUG(1,"\t\tUSB_DESCRIPTOR_TYPE = UNKOWN!");
                usb_send_ep0_stall();
            }
            break;

        case USB_REQUEST_SET_ADDRESS:
            DEBUG(1,"\tUSB_REQUEST_SET_ADDRESS");
            usb_send_ep0_zlp();
            usb_setup_faddr(usd.wValue);
            break;

        case USB_REQUEST_GET_CONFIGURATION:
            DEBUG(1,"\tUSB_REQUEST_GET_CONFIGURATION");
            usb_send_ep0(&CurrentConfiguration, sizeof(CurrentConfiguration), usd.wLength);
            break;

        case USB_REQUEST_GET_STATUS: {
            DEBUG(1,"\tUSB_REQUEST_GET_STATUS");
            if(usd.bmRequestType & 0x80) {
                uint16_t w = 0x01; // self-powered
                usb_send_ep0((uint8_t *)&w, sizeof(w), usd.wLength);
            }
            break;
        }
        case USB_REQUEST_SET_CONFIGURATION:
            DEBUG(1,"\tUSB_REQUEST_SET_CONFIGURATION");
            CurrentConfiguration = usd.wValue;
            uint32_t eps[2] = { EPTYPE_BULK_IN, EPTYPE_BULK_OUT };
            if(CurrentConfiguration) {
                usb_setup_endpoints(eps, 2);
            } else {
                usb_setup_endpoints(NULL, 0);
            }
            usb_send_ep0_zlp();
            break;

        case USB_REQUEST_GET_INTERFACE: {
            DEBUG(1,"\tUSB_REQUEST_GET_INTERFACE");
            uint8_t b = 0;
            usb_send_ep0(&b, sizeof(b), usd.wLength);
            break;
        }

        case USB_REQUEST_SET_INTERFACE:
            DEBUG(1,"\tUSB_REQUEST_SET_INTERFACE");
            usb_send_ep0_zlp();
            break;
            
        case USB_REQUEST_CLEAR_FEATURE:
            if(usd.bmRequestType == 0xc0 || usd.bmRequestType == 0xc1) {
                DEBUG(1,"\tUSB_REQUEST_VENDOR_FEATURE");
                if(usd.wIndex == 4 || usd.wIndex == 5) {
                    usb_send_ep0((uint8_t *)&RecipientDevice,
                        sizeof(RecipientDevice), usd.wLength);
                } else {
                    usb_send_ep0_stall();
                }
            } else
                usb_send_ep0_stall();
            break;        
        case USB_REQUEST_SET_FEATURE:
            DEBUG(1,"\tUSB_REQUEST_SET_FEATURE");
            usb_send_ep0_stall();
            break;
        case USB_REQUEST_SET_DESCRIPTOR:
            DEBUG(1,"\tUSB_REQUEST_SET_DESCRIPTOR");
            usb_send_ep0_stall();
            break;
        case USB_REQUEST_SYNC_FRAME:
            DEBUG(1,"\tUSB_REQUEST_SYNC_FRAME");
            usb_send_ep0_stall();
            break;

        case 0x67: {
            DEBUG(1,"\tUSB_REQUEST_WEIRD!!");
            if(usd.bmRequestType & 0x80) {
                uint16_t w = 0;
                usb_send_ep0((uint8_t *)&w, sizeof(w), usd.wLength);
            }
            break;
        }
        case USB_REQUEST_GET_MAX_LUN:
            DEBUG(1,"\tUSB_REQUEST_GET_MAX_LUN");
            uint16_t w = 0x00;
            usb_send_ep0((uint8_t *)&w, sizeof(w), usd.wLength);
            break;
        default:
            WARNING("\tUSB_REQUEST_UNKNOWN!!");
            usb_send_ep0_stall();
            break;
    }
}

static void HandleRxdData(uint8_t ep, uint8_t* packet, uint32_t length)
{
    int i;
    for(i = 0; i < length; i++) {
        UsbBuffer[UsbSoFarCount] = *packet++;
        UsbSoFarCount++;
    }
    if ((UsbSoFarCount & 0x3f) != 0 || UsbSoFarCount == 0 || UsbSoFarCount == sizeof(UsbBuffer))
    {
        DEBUG(1, "msc_packet_recv(UsbBuffer, UsbSoFarCount) = %d bytes", UsbSoFarCount);
        msc_packet_recv(UsbBuffer, UsbSoFarCount);

        UsbSoFarCount = 0;
    }
}

typedef struct {
    uint32_t    dCBWSignature;      // 'USBC' 43425355h (little endian)
    uint32_t    dCBWTag;            // must match dCSWTag
    uint32_t    dCBWDataTransferLength;
    struct {
        uint8_t reserved:6;
        uint8_t obsolete:1;
        uint8_t direction:1;        // 0 = Data-Out from host to the device, 1 = Data-In from the device to the host
    } bmCBWFlags;
    uint8_t     bCBWLUN:4;          // 4 bits
    uint8_t     reserved0:4;        // (upper 4 bits reserved)
    uint8_t     bCBWCBLength:5;     // 5 bits
    uint8_t     reserved1:3;        // (upper 3 bits reserved)
    uint8_t     CBWCB[16];
} __attribute__ ((packed)) CommandBlockWrapper;

typedef struct {
    uint32_t    dCSWSignature;      // 'USBS' 53425355h (little endian)
    uint32_t    dCSWTag;            // must match dCBWTag
    uint32_t    dCSWDataResidue;
    uint8_t     bCSWStatus;
} __attribute__ ((packed)) CommandStatusWrapper;

typedef enum {
    CommandPassed = 0x00,
    CommandFailed = 0x01,
    PhaseError = 0x02,
    Unsupported = 0xff
} CSWStatus;

// See Universal Serial Bus Mass Storage Class (Bulk-Only Transport) - USB.org
// http://www.usb.org/developers/docs/devclass_docs/usbmassbulk_10.pdf
//
// Host Expectation
// Hn Host expects no data transfers
// Hi Host expects to receive data from the device
// Ho Host expects to send data to the device
// 
// Device Intent
// Dn Device intends to transfer no data
// Di Device intends to send data to the host
// Do Device intends to receive data from the host
//
typedef enum {
    Illegal,
    Hn_eq_Dn,   // (1)      expects no data transfers / intends to transfer no data -> result, residue = 0
    Hn_lt_Di,   // (2)      expects no data transfers / intends to send data        -> phase error, residue = 0
    Hn_lt_Do,   // (3)      expects no data transfers / intends to receive data     -> phase error, residue = 0

    Hi_gt_Dn,   // (4)      expects to receive data / intends to transfer no data   -> stall_IN, residue = hlength
    Hi_gt_Di,   // (5)      expects to receive data / intends to send data          -> stall_IN, residue = hlength - dlength
    Hi_eq_Di,   // (6)      expects to receive data / intends to send data          -> result, residue = 0 (hlength - dlength)
    Hi_lt_Di,   // (7)      expects to receive data / intends to send data          -> phase error, residue = hlength
    Hi_ne_Do,   // (8)      expects to receive data / intends to receive data       -> stall_IN + phase error, residue = 0

    Ho_gt_Dn,   // (9)      expects to send data / intends to transfer no data      -> stall_OUT, residue = hlength
    Ho_ne_Di,   // (10)     expects to send data / intends to send data             -> stall_OUT + phase error, residue = 0
    Ho_gt_Do,   // (11)     expects to send data / intends to receive data          -> stall_OUT, residue = hlength - dlength
    Ho_eq_Do,   // (12)     expects to send data / intends to receive data          -> result, residue = 0
    Ho_lt_Do,   // (13)     expects to send data / intends to receive data          -> phase error, residue = hlength
} HostDeviceDataTransferMode;

typedef enum {
    NoTransfer,
    DeviceToHost,
    HostToDevice,
} HostDeviceTransferDirection;


static struct {
    CommandBlockWrapper         cbw;
    CommandStatusWrapper        csw;
    uint32_t                    hostLength;
    uint32_t                    deviceLength;
    HostDeviceDataTransferMode  transferMode;
} s_ProcessState;

static uint8_t process_transfer_mode();
static CSWStatus process_command();
static void process_status(CSWStatus status);

static void msc_reset()
{
    memset(&s_ProcessState, 0x00, sizeof(s_ProcessState));
}

static void msc_packet_recv(uint8_t* packet, uint32_t length)
{
    DEBUG(1, "------------------------ NEW PACKET -------------------------");

    memcpy(&s_ProcessState.cbw, packet, sizeof(CommandBlockWrapper));
    DumpBuffer((uint8_t*)&s_ProcessState.cbw, sizeof(CommandBlockWrapper));

    if (length != sizeof(CommandBlockWrapper) ||
        s_ProcessState.cbw.dCBWSignature != 0x43425355) {
    
        DEBUG(1, "Error reading CBW");

        // error - stall both endpoints
        usb_send_stall(1);
        usb_send_stall(2);
        return;
    }

    uint8_t valid = process_transfer_mode();

    if (!valid) {
        DEBUG(1,"process_transfer_mode() failed");
//        return; // stall endpoints?
    }

    DEBUG(1, "hostLength   = %d", s_ProcessState.hostLength);
    DEBUG(1, "deviceLength = %d", s_ProcessState.deviceLength);
    DEBUG(1, "transferMode = %d", s_ProcessState.transferMode);


    CSWStatus result = process_command();

    DEBUG(1, "MSC Command result = %d", result);

    process_status(result);
}



// SCSI Commands Reference Manual (Rev. A) - Seagate
// http://www.seagate.com/staticfiles/support/disc/manuals/scsi/100293068a.pdf
// https://www.seagate.com/files/staticfiles/support/docs/manual/Interface%20manuals/100293068j.pdf

#define OPERATIONCODE_TEST_UNIT_READY 0x00


typedef struct {
    uint8_t     OPERATIONCODE;
    uint8_t     EVPD:1;
    uint8_t     CMDDT:1;
    uint8_t     Reserved:6;
    uint8_t     PAGECODE;
    uint8_t     ALLOCATIONLENGTH[2];
    uint8_t     CONTROL;
} __attribute__ ((packed)) INQUIRY;
#define OPERATIONCODE_INQUIRY 0x12

static const struct {
    // byte 0
    uint8_t     PERIPHERALDEVICETYPE:5;
    uint8_t     PERIPHERALQUALIFIER:3;
    // byte 1
    uint8_t     reserved0:7;
    uint8_t     RMB:1;
    // byte 2
    uint8_t     VERSION;
    // byte 3
    uint8_t     RESPONSEDATAFORMAT:4;
    uint8_t     HISUP:1;
    uint8_t     NORMACA:1;
    uint8_t     obsolete0:2;
    // byte 4
    uint8_t     ADDITIONALLENGTH;
    // byte 5
    uint8_t     PROTECT:1;
    uint8_t     reserved1:2;
    uint8_t     TPC:1;
    uint8_t     TPGS:2;
    uint8_t     ACC:1;
    uint8_t     SCCS:1;
    // byte 6
    uint8_t     obsolete1:4;
    uint8_t     MULTIP:1;
    uint8_t     VS1:1;
    uint8_t     ENCSERV:1;
    uint8_t     BQUE:1;
    // byte 7
    uint8_t     VS2:1;
    uint8_t     CMDQUE:1;
    uint8_t     obsolete2:6;

    // byte 8-15
    uint8_t     T10VENDORIDENTIFICATION[8];

    // byte 16-31
    uint8_t      PRODUCTIDENTIFICATION[16];

    // byte 32-35
    uint8_t     PRODUCTREVISIONLEVEL[4];
/*
    // byte 36-43
    uint8_t     DRIVESERIALNUMBER[8];
    // byte 44-55
    uint8_t     VendorUnique[12];  // Seagate fills this field with 00h.
    // byte 56
    uint8_t     reserved2;
    // byte 57
    uint8_t     reserved3;
    // byte 58-59
    uint8_t      VERSIONDESCRIPTOR1[2];
    // byte 60-61
    uint8_t      VERSIONDESCRIPTOR2[2];
    // byte 62-63
    uint8_t      VERSIONDESCRIPTOR3[2];
    // byte 64-65
    uint8_t      VERSIONDESCRIPTOR4[2];
    // byte 66-67
    uint8_t      VERSIONDESCRIPTOR5[2];
    // byte 68-69
    uint8_t      VERSIONDESCRIPTOR6[2];
    // byte 70-71
    uint8_t      VERSIONDESCRIPTOR7[2];
    // byte 72-73
    uint8_t      VERSIONDESCRIPTOR8[2];
    // byte 74-95
    uint8_t     reserved4[22];
*/    
} __attribute__ ((packed)) s_INQUIRYdata = {
    // byte 0
    0x00,           // Direct Access Device (0x00)
    0x00,           // Device type is connected to logical unit
    // byte 1
    0,              // reserved
    0x01,           // This is a REMOVABLE device
    // byte 2
    0x02,           // 0x02 = "Obsolete", 0x06 = The device complies to ANSI INCITS 513-2015 (SPC-4)
    // byte 3
    0x02,           // Response Data Format: SPC-2/SPC-3/SPC-4 (2)
    0x00,           // Hierarchical addressing mode is NOT supported
    0x00,           // Normaca is NOT supported
    0,              // obsolete
    // byte 4
    sizeof(s_INQUIRYdata)-5, // The ADDITIONAL LENGTH field indicates the length in bytes of the remaining standard INQUIRY data
    // byte 5
    0x00,           // Protection information NOT supported
    0,              // reserved
    0x00,           // Third party copy is NOT supported
    0x00,           // Asymmetric LU Access not supported
    0x00,           // Access control coordinator NOT supported
    0x00,           // Scc is NOT supported
    // byte 6
    0,              // obsolete
    0x00,           // This is NOT a multiport device
    0x00,           // vendor specific
    0x00,           // Enclosed services is NOT supported
    0x00,           // Bque is NOT supported
    // byte 7
    0x00,           // vendor specific
    0x00,           // Command queuing is NOT supported
    0,              // obsolete
    // byte 8-15
    {'A','r','c','a','d','e',' ',' '},  // Vendor ID
    // byte 16-31
    {'F','P','G','A','A','r','c','a','d','e','R','e','p','l','a','y'},  // Product ID
    // byte 32-35
    {'1','.','0','0'},                  // Revision level
/*
    // byte 36-43
    {'0','1','2','3','4','5','6','7'},  // Drive Serial
    // byte 43-55
    {  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0},  // Vendor Unique
    // byte 56
    0,              // reserved
    // byte 57
    0,              // reserved
    // byte 58-73
    {0x04, 0xC0},   // SBC-3
    {0x00, 0x00},
    {0x00, 0x00},
    {0x00, 0x00},
    {0x00, 0x00},
    {0x00, 0x00},
    {0x00, 0x00},
    {0x00, 0x00},
    // byte 74-95
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}    // reserved
*/    
};


static const struct {
    uint8_t PERIPHERALDEVICETYPE:5;
    uint8_t PERIPHERALQUALIFIER:3;
    uint8_t PAGECODE;
    uint8_t Reserved;
    uint8_t PAGELENGTH;
    uint8_t ProductSerialNumber[8];
} __attribute__ ((packed)) s_UNIT_SERIAL_NUMBERdata = {
    0,0,
    0x80,
    0,
    0x08,
    {'0','1','2','3','4','5','6','7'}
};

typedef struct {
    uint8_t     OPERATIONCODE;
    uint8_t     Reserved:5;
    uint8_t     LUN:3;
    uint8_t     Reserved1[5];
    uint8_t     ALLOCATIONLENGTH[2];
    uint8_t     Reserved2[3];
} __attribute__ ((packed)) READ_FORMAT_CAPACITY;
#define OPERATIONCODE_READ_FORMAT_CAPACITY 0x23

typedef struct {
    uint8_t     reserved[3];        // drive 0
    uint8_t     ADDITIONALLENGTH;
    struct CapacityDescriptor
    {
        uint8_t NUM_BLOCKS[4];
        uint8_t DESC_CODE:2;
        uint8_t reserved:6;
        uint8_t BLOCK_LENGTH[3];   
    }           CAP_DESC;
} __attribute__ ((packed)) READ_FORMAT_CAPACITYdata;


typedef struct {
    uint8_t     OPERATIONCODE;
    uint8_t     Obsolete:1;
    uint8_t     Reserved:7;
    uint8_t     LBA[4];
    uint8_t     Reserved1[2];
    uint8_t     PMI:1;
    uint8_t     Reserved2:7;
    uint8_t     CONTROL;
} __attribute__ ((packed)) READ_CAPACITY;
#define OPERATIONCODE_READ_CAPACITY 0x25

typedef struct {
    // byte 0-3
    uint8_t     LBA[4];
    // byte 4-7
    uint8_t     NUMBYTESPERBLOCK[4];
} __attribute__ ((packed)) READ_CAPACITYdata;

typedef struct {
    uint8_t     OPERATIONCODE;
    uint8_t     Reserved0:3;
    uint8_t     DBD:1;
    uint8_t     Reserved1:4;
    uint8_t     PAGECODE:6;
    uint8_t     PC:2;
    uint8_t     SUBPAGECODE;
    uint8_t     ALLOCATIONLENGTH;
    uint8_t     CONTROL;
} __attribute__ ((packed)) MODE_SENSE_6;
#define OPERATIONCODE_MODE_SENSE_6 0x1a

static const struct {
    uint8_t     MODEDATALENGTH;
    uint8_t     MEDIUMTYPE;
    uint8_t     DEVICESPECIFIC;
    uint8_t     BLOCKDESCRIPTORLENGTH;
} __attribute__ ((packed)) s_MODE_PARAMETER_HEADERdata = {
    sizeof(s_MODE_PARAMETER_HEADERdata) - 1,
    0x00,               // Direct Access Device (0x00)
    0x00,               // Device Specific.. ?
    0x00                // No block descriptors
};

typedef struct {
    uint8_t     OPERATIONCODE;
    uint8_t     Obsolete:1;
    uint8_t     FUA_NV:1;
    uint8_t     Reserved:1;
    uint8_t     FUA:1;
    uint8_t     DPO:1;
    uint8_t     RDPROTECT:3;
    uint8_t     LBA[4];
    uint8_t     GROUPNUMBER:5;
    uint8_t     Reserved1:3;
    uint8_t     TRANSFERLENGTH[2];
    uint8_t     CONTROL;
} __attribute__ ((packed)) READ_10;
#define OPERATIONCODE_READ_10 0x28

typedef union {
    uint8_t         OPERATIONCODE;
    INQUIRY         inquiry;
    READ_CAPACITY   readCapacity;
    READ_FORMAT_CAPACITY    readFormatCapacity;
    MODE_SENSE_6    modeSense6;
    READ_10         read10;
} CommandDescriptorBlock;




#define READ_BE_16B(x)  ((uint16_t) ((x[0] << 8) | x[1]))
#define READ_BE_32B(x)  ((uint32_t) ((x[0] << 24) | (x[1] << 16) | (x[2] << 8) | x[3]))

#define WRITE_BE_32B(x,val) \
    x[0] = (uint8_t) (((val) >> 24) & 0xff); \
    x[1] = (uint8_t) (((val) >> 16) & 0xff); \
    x[2] = (uint8_t) (((val)  >> 8) & 0xff); \
    x[3] = (uint8_t)  ((val)        & 0xff);


static uint8_t process_transfer_mode()
{
    CommandBlockWrapper* cbw = &s_ProcessState.cbw;
//    CommandStatusWrapper* csw = &s_ProcessState.csw;

    const uint32_t hostLength = cbw->dCBWDataTransferLength;
    const HostDeviceTransferDirection hostTransferType = hostLength == 0 ? NoTransfer : cbw->bmCBWFlags.direction ? DeviceToHost : HostToDevice;

    uint32_t deviceLength = 0;
    HostDeviceTransferDirection deviceTransferType = NoTransfer;

    CommandDescriptorBlock* cdb = (CommandDescriptorBlock*)cbw->CBWCB;
    switch (cdb->OPERATIONCODE) {
        case OPERATIONCODE_INQUIRY:
            if (cdb->inquiry.EVPD) {
                if (cdb->inquiry.PAGECODE == 0x80) {
                    deviceLength = sizeof(s_UNIT_SERIAL_NUMBERdata);
                }
                else {
                    WARNING("Unknown EVPD %02x!", cdb->inquiry.PAGECODE);
                }
            } else {
                deviceLength = READ_BE_16B(cdb->inquiry.ALLOCATIONLENGTH);
            }
            deviceTransferType = DeviceToHost;
            break;
        case OPERATIONCODE_TEST_UNIT_READY:
            deviceTransferType = NoTransfer;
            break;
        case OPERATIONCODE_READ_FORMAT_CAPACITY:
            deviceLength = sizeof(READ_FORMAT_CAPACITYdata);
            deviceTransferType = DeviceToHost;
            break;
        case OPERATIONCODE_READ_CAPACITY:
            deviceLength = sizeof(READ_CAPACITYdata);
            deviceTransferType = DeviceToHost;
            break;
        case OPERATIONCODE_MODE_SENSE_6:
            if (cdb->modeSense6.ALLOCATIONLENGTH >= sizeof(s_MODE_PARAMETER_HEADERdata))
                deviceLength = sizeof(s_MODE_PARAMETER_HEADERdata);
            else
                deviceLength = cdb->modeSense6.ALLOCATIONLENGTH;
            deviceTransferType = DeviceToHost;
            if (cdb->modeSense6.PAGECODE != 0x3f)
                WARNING("Unknown bad pagecode!");
            break;
        case OPERATIONCODE_READ_10:
            deviceLength = READ_BE_16B(cdb->read10.TRANSFERLENGTH) * 512;
            deviceTransferType = DeviceToHost;
            break;
        default:
            WARNING("Unknown operation code!");
            s_ProcessState.hostLength = hostLength;
            s_ProcessState.deviceLength = 0;
            s_ProcessState.transferMode = Illegal;
            return FALSE;
    }

    HostDeviceDataTransferMode transferMode = Illegal;

    if (hostTransferType == NoTransfer) {
        if (deviceTransferType == NoTransfer) {
            transferMode = Hn_eq_Dn;                                // (1)
        } else if (deviceTransferType == DeviceToHost) {
            transferMode = Hn_lt_Di;                                // (2)
        } else { // deviceTransferType == HostToDevice
            transferMode = Hn_lt_Do;                                // (3)
        }
    } else if (hostTransferType == DeviceToHost) {
        if (deviceTransferType == NoTransfer) {
            transferMode = Hi_gt_Dn;                                // (4)
        } else if (deviceTransferType == DeviceToHost) {
            if (hostLength > deviceLength) {
                transferMode = Hi_gt_Di;                            // (5)
            } else if (hostLength == deviceLength) {
                transferMode = Hi_eq_Di;                            // (6)
            } else { // hostLength < deviceLength
                transferMode = Hi_lt_Di;                            // (7)
            }
        } else { // deviceTransferType == HostToDevice
            transferMode = Hi_ne_Do;                                // (8)
        }
    } else { // hostTransferType == HostToDevice
        if (deviceTransferType == NoTransfer) {
            transferMode = Ho_gt_Dn;                                // (9)
        } else if (deviceTransferType == DeviceToHost) {
            transferMode = Ho_ne_Di;                                // (10)
        } else { // deviceTransferType == HostToDevice
            if (hostLength > deviceLength) {
                transferMode = Ho_gt_Do;                            // (11)
            } else if (hostLength == deviceLength) {
                transferMode = Ho_eq_Do;                            // (12)
            } else { // hostLength < deviceLength
                transferMode = Ho_lt_Do;                            // (13)
            }
        }
    }

    s_ProcessState.hostLength = hostLength;
    s_ProcessState.deviceLength = deviceLength;
    s_ProcessState.transferMode = transferMode;

    return TRUE;
}

static uint8_t OneSector[512];

static CSWStatus process_command()
{
    CommandBlockWrapper* cbw = &s_ProcessState.cbw;
//    CommandStatusWrapper* csw = &s_ProcessState.csw;

    CommandDescriptorBlock* cdb = (CommandDescriptorBlock*)cbw->CBWCB;
    switch (cdb->OPERATIONCODE) {
        case OPERATIONCODE_INQUIRY:
            DEBUG(1, "OPERATIONCODE_INQUIRY");
            if (cdb->inquiry.EVPD) {
                if (cdb->inquiry.PAGECODE == 0x80)
                    msc_send((uint8_t*)&s_UNIT_SERIAL_NUMBERdata, s_ProcessState.deviceLength);
            } else {
                msc_send((uint8_t*)&s_INQUIRYdata, s_ProcessState.deviceLength);
            }
            return CommandPassed;
        case OPERATIONCODE_TEST_UNIT_READY:
            DEBUG(1, "OPERATIONCODE_TEST_UNIT_READY");
            // assume all is good
            return CommandPassed;
        case OPERATIONCODE_READ_FORMAT_CAPACITY: {
            DEBUG(1, "OPERATIONCODE_READ_FORMAT_CAPACITY");
            READ_FORMAT_CAPACITYdata data;
            memset(&data,0x00,sizeof(data));
            data.ADDITIONALLENGTH = sizeof(data)-4;
            uint64_t capacity = Card_GetCapacity();
            uint32_t sectorCount = (uint32_t)(capacity / 512);
            WRITE_BE_32B(data.CAP_DESC.NUM_BLOCKS, sectorCount);
            data.CAP_DESC.BLOCK_LENGTH[1] = 0x02;   // 512
            msc_send((uint8_t*)&data, s_ProcessState.deviceLength);
            return CommandPassed;
        }
        case OPERATIONCODE_READ_CAPACITY: {
            DEBUG(1, "OPERATIONCODE_READ_CAPACITY");
            READ_CAPACITYdata data;
            memset(&data,0x00,sizeof(data));
            uint64_t capacity = Card_GetCapacity();
            uint32_t sectorCount = (uint32_t)(capacity / 512);
            WRITE_BE_32B(data.LBA, sectorCount-1);
            WRITE_BE_32B(data.NUMBYTESPERBLOCK, 512);
            msc_send((uint8_t*)&data, s_ProcessState.deviceLength);
            return CommandPassed;
        }
        case OPERATIONCODE_MODE_SENSE_6:
            DEBUG(1, "OPERATIONCODE_MODE_SENSE_6");
            msc_send((uint8_t*)&s_MODE_PARAMETER_HEADERdata, s_ProcessState.deviceLength);
            return CommandPassed;
        case OPERATIONCODE_READ_10: {
            DEBUG(1, "OPERATIONCODE_READ_10");
            uint32_t sectorOffset = READ_BE_32B(cdb->read10.LBA);
            uint32_t numSectors = s_ProcessState.deviceLength / 512;
            for (int i = 0; i < numSectors; ++i) {
                Card_ReadM(OneSector, sectorOffset+i, 1, NULL);
                msc_send(OneSector, sizeof(OneSector));
            }
            return CommandPassed;
        }
        default:
            WARNING("Unknown operation code! OPERATIONCODE = $%02x", cdb->OPERATIONCODE);
            return Unsupported;
    }
}

static void process_status(CSWStatus status)
{
    CommandBlockWrapper* cbw = &s_ProcessState.cbw;
    CommandStatusWrapper* csw = &s_ProcessState.csw;

    csw->dCSWSignature = 0x53425355;
    csw->dCSWTag = cbw->dCBWTag;
    csw->bCSWStatus = status;
    csw->dCSWDataResidue = s_ProcessState.hostLength - s_ProcessState.deviceLength;

    DEBUG(1, "CSW before processing transfer mode");
    DumpBuffer((uint8_t*)csw,sizeof(CommandStatusWrapper));

    uint8_t stall_in = FALSE;
    uint8_t stall_out = FALSE;

    DEBUG(1, "transferMode = %d", s_ProcessState.transferMode);
    switch(s_ProcessState.transferMode) {
        case Hn_eq_Dn:
        case Hi_eq_Di:
        case Ho_eq_Do:
            // -> result, residue = 0
            csw->bCSWStatus = status;
            csw->dCSWDataResidue = 0;
            break;

        case Hn_lt_Di:
        case Hn_lt_Do:
            // -> phase error, residue = 0
            csw->bCSWStatus = PhaseError;
            csw->dCSWDataResidue = 0;
            break;

        case Ho_lt_Do:
        case Hi_lt_Di:
            // -> phase error, residue = hlength
            csw->bCSWStatus = PhaseError;
            csw->dCSWDataResidue = s_ProcessState.hostLength;
            break;

        case Hi_gt_Dn:
        case Hi_gt_Di:
            // -> stall_IN, residue = hlength - dlength (0)
            stall_in = TRUE;
            csw->dCSWDataResidue = s_ProcessState.hostLength - s_ProcessState.deviceLength;
            break;

        case Ho_gt_Dn:
        case Ho_gt_Do:
            // -> stall_OUT, residue = hlength - dlength (0)
            stall_out = TRUE;
            csw->dCSWDataResidue = s_ProcessState.hostLength - s_ProcessState.deviceLength;
            break;

        case Hi_ne_Do:
            // -> stall_IN + phase error, residue = 0
            stall_in = TRUE;
            csw->bCSWStatus = PhaseError;
            csw->dCSWDataResidue = 0;
            break;
        case Ho_ne_Di:
            // -> stall_OUT + phase error, residue = 0
            stall_out = TRUE;
            csw->bCSWStatus = PhaseError;
            csw->dCSWDataResidue = 0;
            break;
        default:
            WARNING("Illegal transfer mode!");
    }

    DEBUG(1, "CSW after processing transfer mode");
    DumpBuffer((uint8_t*)csw,sizeof(CommandStatusWrapper));

    if (status == Unsupported) {
        DEBUG(1, "Command not supported - sending stalls");
        csw->bCSWStatus = CommandFailed;
//        if (cbw->bmCBWFlags.direction)          // signal that there wont be any data
//            stall_in = TRUE;
//        else if (cbw->dCBWDataTransferLength)   // only signal if there would be data to receive
//            stall_out = TRUE;
        csw->dCSWDataResidue += s_ProcessState.deviceLength;

        DumpBuffer((uint8_t*)csw,sizeof(CommandStatusWrapper));
    }

    if (stall_in) {
        DEBUG(1, "stall_IN");
//        usb_send_stall(1);
    }
    if (stall_out) {
        DEBUG(1, "stall_OUT");
//        usb_send_stall(2);
    }


    DEBUG(1, "bCSWStatus = %d", csw->bCSWStatus);
    DEBUG(1, "dCSWDataResidue = %d", csw->dCSWDataResidue);

    msc_send((uint8_t*)csw, sizeof(CommandStatusWrapper));
}
