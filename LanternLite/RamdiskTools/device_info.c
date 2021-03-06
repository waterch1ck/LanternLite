#include <stdio.h>
#include <stdint.h>
#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOKitLib.h>
#include "IOAESAccelerator.h"
#include "AppleEffaceableStorage.h"
#include "bsdcrypto/rijndael.h"
#include "bsdcrypto/key_wrap.h"
#include "registry.h"
#include "util.h"

    uint8_t lockers[960]={0};
    uint8_t lwvm[80]={0};

CFDictionaryRef device_info(int socket, CFDictionaryRef request)
{
    uint8_t dkey[40]={0};
    uint8_t emf[36]={0};

    struct HFSInfos hfsinfos={0};
    
    CFMutableDictionaryRef out  = CFDictionaryCreateMutable(kCFAllocatorDefault,
                                                            0,
                                                            &kCFTypeDictionaryKeyCallBacks,
                                                            &kCFTypeDictionaryValueCallBacks);	
    
    get_device_infos(out);
    
    getHFSInfos(&hfsinfos);
    /*
    printf("NAND block size  : %x\n", hfsinfos.blockSize);
    printf("Data volume UUID : %llx\n", CFSwapInt64BigToHost(hfsinfos.volumeUUID));
    printf("Data volume offset : %x\n", hfsinfos.dataVolumeOffset);
    */
    uint8_t* key835 = IOAES_key835();
    uint8_t* key89B = IOAES_key89B();
    
    if (!AppleEffaceableStorage__getBytes(lockers, 960))
    {
        CFDataRef lockersData = CFDataCreateWithBytesNoCopy(kCFAllocatorDefault, lockers, 960, kCFAllocatorNull);
        CFDictionaryAddValue(out, CFSTR("lockers"), lockersData);
        CFRelease(lockersData);
        
        if (!AppleEffaceableStorage__getLockerFromBytes(LOCKER_DKEY, lockers, 960, dkey, 40))
        {
            aes_key_wrap_ctx ctx;

            aes_key_wrap_set_key(&ctx, key835, 16);

            if(aes_key_unwrap(&ctx, dkey, dkey, 32/8))
                printf("FAIL unwrapping DKey with key 0x835\n");
        }
        if (!AppleEffaceableStorage__getLockerFromBytes(LOCKER_EMF, lockers, 960, emf, 36))
        {
            doAES(&emf[4], &emf[4], 32, kIOAESAcceleratorCustomMask, key89B, NULL, kIOAESAcceleratorDecrypt, 128);
        }
        else if (!AppleEffaceableStorage__getLockerFromBytes(LOCKER_LWVM, lockers, 960, lwvm, 0x50))
        {
            doAES(lwvm, lwvm, 0x50, kIOAESAcceleratorCustomMask, key89B, NULL, kIOAESAcceleratorDecrypt, 128);
            memcpy(&emf[4], &lwvm[32+16], 32);
        }
    }
    
    CFNumberRef n = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &hfsinfos.dataVolumeOffset);
    CFDictionaryAddValue(out, CFSTR("dataVolumeOffset"), n);
    CFRelease(n);
    addHexaString(out, CFSTR("dataVolumeUUID"), (uint8_t*) &hfsinfos.volumeUUID, 8);
    addHexaString(out, CFSTR("key835"), key835, 16);
    addHexaString(out, CFSTR("key89B"), key89B, 16);
    addHexaString(out, CFSTR("EMF"), &emf[4], 32);
    addHexaString(out, CFSTR("DKey"), dkey, 32);
    
    return out;
}
