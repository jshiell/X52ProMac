//
//  main.c
//  X52ProDaemon
//
//  Created by James Shiell on 04/11/2014.
//  Copyright (c) 2014 Infernus. All rights reserved.
//

#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/IOCFPlugIn.h>
#include <IOKit/usb/IOUSBLib.h>
#include <IOKit/usb/USBSpec.h>
#include <time.h>

#define VENDOR_ID                   0x06A3
#define PRODUCT_ID                  0x0762
#define REQUEST_TYPE                0x91

#define INDEX_UPDATE_PRIMARY_CLOCK  0xc0
#define INDEX_UPDATE_DATE_DAYMONTH  0xc4
#define INDEX_UPDATE_DATE_YEAR      0xc8
#define INDEX_DELETE_LINE1          0xd9
#define INDEX_DELETE_LINE2          0xda
#define INDEX_DELETE_LINE3          0xdc
#define INDEX_APPEND_LINE1          0xd1
#define INDEX_APPEND_LINE2          0xd2
#define INDEX_APPEND_LINE3          0xd4

typedef struct DeviceData {
    io_object_t             notification;
    IOUSBDeviceInterface    **deviceInterface;
    dispatch_source_t       dispatchSource;
} DeviceData;

static IONotificationPortRef    gNotifyPort;
static io_iterator_t            gAddedIter;
static CFRunLoopRef             gRunLoop;

void SendControlRequest(IOUSBDeviceInterface** usbDevice, short wValue, short wIndex) {
    kern_return_t kr;
    
    IOUSBDevRequest request;
    request.bmRequestType = USBmakebmRequestType(kUSBOut, kUSBVendor, kUSBDevice);
    request.bRequest = REQUEST_TYPE;
    request.wValue = wValue;
    request.wIndex = wIndex;
    request.wLength = 0;
    
    kr = (*usbDevice)->DeviceRequest(usbDevice, &request);
    if (kr != kIOReturnSuccess) {
        fprintf(stderr, "DeviceRequest returned 0x%08x.\n", kr);
    }
}

void UpdateDate(IOUSBDeviceInterface **usbDevice, struct tm *localtimeInfo) {
    short field1;
    short field2;
    short field3;
    short dayMonth;
    
    /* TODO multiple date formats */
    field1 = localtimeInfo->tm_mday;
    field2 = localtimeInfo->tm_mon + 1;
    field3 = localtimeInfo->tm_year + 1900;
    
    dayMonth = field1;
    dayMonth &= ~(255 << 8);
    dayMonth |= (field2 << 8);
    
    SendControlRequest(usbDevice, dayMonth, INDEX_UPDATE_DATE_DAYMONTH);
    SendControlRequest(usbDevice, field3, INDEX_UPDATE_DATE_YEAR);
}

void UpdateTime(IOUSBDeviceInterface **usbDevice, struct tm *localtimeInfo) {
    short timeValue;
    
    timeValue = localtimeInfo->tm_min;
    
    timeValue &= ~(255 << 8);
    timeValue |= (localtimeInfo->tm_hour << 8);
    
    timeValue &= (short) ~(1 << 15);
    /* TODO support 12 hour clock */
    timeValue |= (short) (1 << 15);
    
    SendControlRequest(usbDevice, timeValue, INDEX_UPDATE_PRIMARY_CLOCK);
}

void TimeUpdateHandler(void* context) {
    DeviceData *dataRef = (DeviceData*) context;
    
    if (dataRef->deviceInterface) {
        time_t rawtime;
        struct tm *localtimeInfo;
        time(&rawtime);
        localtimeInfo = localtime(&rawtime);
    
        (*dataRef->deviceInterface)->USBDeviceOpen(dataRef->deviceInterface);
        UpdateTime(dataRef->deviceInterface, localtimeInfo);
        UpdateDate(dataRef->deviceInterface, localtimeInfo);
        (*dataRef->deviceInterface)->USBDeviceClose(dataRef->deviceInterface);
    }
}

void DeviceNotification(void *refCon, io_service_t service, natural_t messageType, void *messageArgument)
{
    kern_return_t kr;
    DeviceData *dataRef = (DeviceData *) refCon;
    
    if (messageType == kIOMessageServiceIsTerminated) {
        if (dataRef->dispatchSource) {
            dispatch_source_cancel(dataRef->dispatchSource);
            dispatch_release(dataRef->dispatchSource);
            dataRef->dispatchSource = NULL;
        }
        
        if (dataRef->deviceInterface) {
            kr = (*dataRef->deviceInterface)->Release(dataRef->deviceInterface);
        }
        
        kr = IOObjectRelease(dataRef->notification);
        
        free(dataRef);
    }
}

void DeviceAdded(void *refCon, io_iterator_t iterator)
{
    kern_return_t kr;
    io_service_t usbDevice;
    IOCFPlugInInterface **plugInInterface = NULL;
    SInt32 score;
    HRESULT res;
    
    while ((usbDevice = IOIteratorNext(iterator))) {
        DeviceData *dataRef = NULL;
        
        dataRef = malloc(sizeof(DeviceData));
        bzero(dataRef, sizeof(DeviceData));
        
        kr = IOCreatePlugInInterfaceForService(usbDevice,
                                               kIOUSBDeviceUserClientTypeID,
                                               kIOCFPlugInInterfaceID,
                                               &plugInInterface,
                                               &score);
        
        if ((kIOReturnSuccess != kr) || !plugInInterface) {
            fprintf(stderr, "IOCreatePlugInInterfaceForService returned 0x%08x.\n", kr);
            continue;
        }
        
        res = (*plugInInterface)->QueryInterface(plugInInterface, CFUUIDGetUUIDBytes(kIOUSBDeviceInterfaceID),
                                                 (LPVOID*) &dataRef->deviceInterface);
        (*plugInInterface)->Release(plugInInterface);
        
        if (res || dataRef->deviceInterface == NULL) {
            fprintf(stderr, "QueryInterface returned %d.\n", (int) res);
            continue;
        }
        
        dispatch_queue_t backgroundQueue = dispatch_get_global_queue(QOS_CLASS_BACKGROUND, 0);
        dispatch_source_t timerSource = dispatch_source_create(DISPATCH_SOURCE_TYPE_TIMER, 0, 0, backgroundQueue);
        dispatch_retain(timerSource);
        dispatch_source_set_timer(timerSource, dispatch_time(DISPATCH_TIME_NOW, 0), 1.0 * NSEC_PER_SEC, 1.0 * NSEC_PER_SEC);
        dispatch_source_set_event_handler_f(timerSource, TimeUpdateHandler);
        dispatch_set_context(timerSource, dataRef);
        dispatch_resume(timerSource);
        
        dataRef->dispatchSource = timerSource;
        
        kr = IOServiceAddInterestNotification(gNotifyPort,
                                              usbDevice,
                                              kIOGeneralInterest,
                                              DeviceNotification,
                                              dataRef,
                                              &(dataRef->notification));
        
        if (KERN_SUCCESS != kr) {
            printf("IOServiceAddInterestNotification returned 0x%08x.\n", kr);
        }
        
        kr = IOObjectRelease(usbDevice);
    }
}

void SignalHandler(int sigraised)
{
    fprintf(stderr, "\nInterrupted.\n");
    
    exit(0);
}

int main(int argc, const char *argv[])
{
    sig_t oldHandler = signal(SIGINT, SignalHandler);
    if (oldHandler == SIG_ERR) {
        fprintf(stderr, "Could not establish new signal handler.");
    }
    
    CFMutableDictionaryRef matchingDictionary = NULL;
    SInt32 idVendor = VENDOR_ID;
    SInt32 idProduct = PRODUCT_ID;
    CFRunLoopSourceRef runLoopSource;
    kern_return_t kr;
    
    matchingDictionary = IOServiceMatching(kIOUSBDeviceClassName);
    CFDictionaryAddValue(matchingDictionary,
                         CFSTR(kUSBVendorID),
                         CFNumberCreate(kCFAllocatorDefault,
                                        kCFNumberSInt32Type, &idVendor));
    CFDictionaryAddValue(matchingDictionary,
                         CFSTR(kUSBProductID),
                         CFNumberCreate(kCFAllocatorDefault,
                                        kCFNumberSInt32Type, &idProduct));

    gNotifyPort = IONotificationPortCreate(kIOMasterPortDefault);
    runLoopSource = IONotificationPortGetRunLoopSource(gNotifyPort);
    
    gRunLoop = CFRunLoopGetCurrent();
    CFRunLoopAddSource(gRunLoop, runLoopSource, kCFRunLoopDefaultMode);
    
    kr = IOServiceAddMatchingNotification(gNotifyPort,
                                          kIOFirstMatchNotification,
                                          matchingDictionary,
                                          DeviceAdded,
                                          NULL,
                                          &gAddedIter
                                          );
    
    DeviceAdded(NULL, gAddedIter);
    
    CFRunLoopRun();
    
    fprintf(stderr, "Unexpectedly back from CFRunLoopRun()!\n");
    return 0;
}