#include  <arduino.h>
#include  "System.h"
#include  "StreamIO.h"
/*
 * PJRCのフォーラムでdefragster氏から教えてもらった。
 */
/*
#if defined(__IMXRT1062__) // Get Pointer to FREE ITCM
uint32_t *ptrFreeITCM;  // Set to Usable ITCM free RAM
uint32_t  sizeofFreeITCM; // sizeof free RAM in uint32_t units.
uint32_t  SizeLeft_etext;
FLASHMEM
void   getFreeITCM() { // end of CODE ITCM, skip full 32 bits
  extern unsigned long _stext;
  extern unsigned long _etext;
  SizeLeft_etext = (32 * 1024) - (((uint32_t)&_etext - (uint32_t)&_stext) % (32 * 1024));
  sizeofFreeITCM = SizeLeft_etext - 4;
  sizeofFreeITCM /= sizeof(ptrFreeITCM[0]);
  ptrFreeITCM = (uint32_t *) ( (uint32_t)&_stext + (uint32_t)&_etext + 4 );
  Serial.printf( "Size of Free ITCM in Bytes = %u\n", sizeofFreeITCM*sizeof(ptrFreeITCM[0]) );
  Serial.printf( "Start of Free ITCM = %u [%X] \n", ptrFreeITCM, ptrFreeITCM);
  Serial.printf( "End of Free ITCM = %u [%X] \n", ptrFreeITCM + sizeofFreeITCM, ptrFreeITCM + sizeofFreeITCM);
}
#else
void   getFreeITCM() {}
#endif
 */
uint32_t  ITCM_SIZE = 0;

#if defined(__IMXRT1062__) // Get Pointer to FREE ITCM
uint32_t *ptrFreeITCM;  // Set to Usable ITCM free RAM
uint32_t  sizeofFreeITCM; // sizeof free RAM in uint32_t units.
uint32_t  SizeLeft_etext;
FLASHMEM
void   getFreeITCM() { // end of CODE ITCM, skip full 32 bits
  extern unsigned long _stext;
  extern unsigned long _etext;
  SizeLeft_etext = (32 * 1024) - (((uint32_t)&_etext - (uint32_t)&_stext) % (32 * 1024));
  sizeofFreeITCM = SizeLeft_etext - 4;
  sizeofFreeITCM /= sizeof(ptrFreeITCM[0]);
  ptrFreeITCM = (uint32_t *) ( (uint32_t)&_stext + (uint32_t)&_etext + 4 );
  ITCM_SIZE = (uint32_t)(ptrFreeITCM + sizeofFreeITCM);
}
#else
void   getFreeITCM() {HOST_PORT.println("Undefined: __IMXRT1062__")}
#endif
/*
 * アクセス不可の領域に触るとCPUがロックされるようなので、予防。
 */
boolean isAccessible(void * addr, int size){
  const char *ptr = (const char *)addr;
  if ((ptr >= (const char *)0)&&(ptr <= ((const char *)ITCM_SIZE - size)))             // Code area(RAM)
    return  true;
  if ((ptr >= (const char *)0x20000000)&&(ptr <= ((const char *)(0x20080000 - ITCM_SIZE - size))))    // RAM1 area for variable 
    return  true;
  if ((ptr >= (const char *)0x20200000)&&(ptr <= ((const char *)0x20280000 - size)))    // RAM2 area for malloc etc
    return  true;
  return  false;
}
boolean isAccessibleWithError(void * addr, int size){
  boolean res = isAccessible(addr, size);
  if (res == false){
    HOST_PORT.print("!! PANIC !! - ");
    HOST_PORT.print((int)addr, HEX);
    HOST_PORT.println(": Not accessible");
    HOST_PORT.flush();
  }
  return  res;  
}
/*
 * ワードをダンプするデバッグ用の機能。
 */
char *pack32Bit(unsigned long data){
  static  char pack[10] = "";
  sprintf(pack, "%04X:%04X", (unsigned int)data >> 16, (unsigned int)(data & 0x0000ffff));
  return  pack;
}
char *pack16Bit(short data){
  static  char pack[5] = "";
  sprintf(pack, "%04X", data);
  return  pack;
}
char *pack8Bit(char data){
  static  char pack[3] = "";
  sprintf(pack, "%02X", data);
  return  pack;
}
/*
 * システム日付（'Oct 5 2014'）を "yyyy/mm/dd"の形式に変換する。
 * 桁合わせやゼロパディングのようなことまではしていない。
 */
String  getFormatedDate(const char *org){
  static const char monthNames[] = "JanFebMarAprMayJunJulAugSepOctNovDec"; 
  int   mm, dd, yy;
  char  mmStr[5];
  sscanf(org, "%s %d %d", mmStr, &dd, &yy);
  mm = (strstr(monthNames, mmStr) - monthNames) / 3 + 1;
  return  String(yy)+"/"+String(mm)+"/"+String(dd);  
}
