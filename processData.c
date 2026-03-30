#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "processData.h"
#include "cJSON.h"

#define BUFFER_SIZE 700

SENSORSDATA *loadJson(const char *fileName, int *countItems)
{
    FILE *f = fopen(fileName, "r");

    if (!f)
    {
        printf("Erro ao abrir arquivo!\n");
        exit(1);
    }

    fseek(f, sizeof(char), SEEK_SET);

    char *str = (char *)malloc(sizeof(char) * BUFFER_SIZE + 1);

    SENSORSDATA *sensorsDataArray = NULL;

    while (fscanf(f, " %[^]],", str) == 1)
    {
        int size = strlen(str);
        str[size] = ']';
        str[size + 1] = '\0';

        cJSON *array = cJSON_Parse(str);
        array = array->child;
        SENSORSDATA sensorsData;

        sensorsData.airPressure.value = -1;
        sensorsData.batteryLevel.value = -1;
        sensorsData.humidity.value = -1;
        sensorsData.temperature.value = -1;
        sensorsData.spreadingFactor = -1;

        while (array)
        {
            cJSON *object = array->child;
            DATE date;
            TIME time;
            if (!strcmp(object->valuestring, "temperature"))
            {
                object = object->next;
                sensorsData.temperature.value = object->valuedouble;
                object = object->next;

                sscanf(object->valuestring, "%d-%d-%dT%d:%d:%d.", &date.year, &date.month, &date.day, &time.hours, &time.minutes, &time.seconds);
                sensorsData.temperature.date = date;
                sensorsData.temperature.time = time;
            }
            else if (!strcmp(object->valuestring, "humidity"))
            {
                object = object->next;
                sensorsData.humidity.value = object->valuedouble;
                object = object->next;

                sscanf(object->valuestring, "%d-%d-%dT%d:%d:%d.", &date.year, &date.month, &date.day, &time.hours, &time.minutes, &time.seconds);
                sensorsData.humidity.date = date;
                sensorsData.humidity.time = time;
            }
            else if (!strcmp(object->valuestring, "airpressure"))
            {
                object = object->next;
                sensorsData.airPressure.value = object->valuedouble;
                object = object->next;

                sscanf(object->valuestring, "%d-%d-%dT%d:%d:%d.", &date.year, &date.month, &date.day, &time.hours, &time.minutes, &time.seconds);
                sensorsData.airPressure.date = date;
                sensorsData.airPressure.time = time;
            }
            else if (!strcmp(object->valuestring, "batterylevel"))
            {
                object = object->next;
                sensorsData.batteryLevel.value = object->valuedouble;
            }
            else
            {
                object = object->next;
                sensorsData.spreadingFactor = object->valueint;
            }
            array = array->next;
        }
        if(sensorsDataArray == NULL)
        {
            sensorsDataArray = (SENSORSDATA *)malloc(sizeof(SENSORSDATA));
        }
        else{
            sensorsDataArray = (SENSORSDATA*)realloc(sensorsDataArray,sizeof(SENSORSDATA)*((*countItems)+1));
        }
        sensorsDataArray[(*countItems)++] = sensorsData;
        fscanf(f, "],");
    }

    //Teste para visualizar dados lidos
    for (int i = 0; i < *countItems; i++)
    {
        printf("%05d - AP: %lf, BL: %lf, H: %lf, T: %lf, SP%d\n", i+1,sensorsDataArray[i].airPressure.value, sensorsDataArray[i].batteryLevel.value, sensorsDataArray[i].humidity.value, sensorsDataArray[i].temperature.value, sensorsDataArray[i].spreadingFactor);
    }

    fclose(f);
    return sensorsDataArray;
}

void *processJsonData(void *args)
{
    char *fileName = (char *)args;
    int countItems = 0;

    loadJson(fileName, &countItems);
}