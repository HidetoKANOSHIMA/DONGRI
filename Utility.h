#ifndef __UTILITY__
#define __UTILITY__

void    getFreeITCM();
boolean isAccessible(void * addr, int size);
boolean isAccessibleWithError(void * addr, int size);
char    *pack32Bit(unsigned long data);
char    *pack16Bit(short data);
char    *pack8Bit(char data);
String  getFormatedDate(const char *org);

#endif
