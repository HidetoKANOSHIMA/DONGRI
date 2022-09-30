#ifndef __STREAM_IO__
#define __STREAM_IO__

#define   HOST_PORT       Serial
#define   AUX0_PORT       Serial
#define   HOST_PORT_BAUD  38400
#define   AUX0_PORT_BAUD  38400

void    initHostPort();
void    openingMessageToHostPort();
void    mySerialEvent();
String  readLineFromHostPort();
String  readLineFromAux0Port();
char    readCharFromAux0Port();
int     checkCharFromAux0Port();

#endif
