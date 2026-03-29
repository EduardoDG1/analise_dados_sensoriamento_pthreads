#ifndef PROCESSDATA_H
#define PROCESSDATA_H

#include  "structs.h"

SENSORSDATA **loadJson(const char *fileName, int *countItems);
void *processJsonData(void *);

#endif