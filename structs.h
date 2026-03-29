#ifndef STRUCTS_H
#define STRUCTS_H

typedef struct 
{
    int day;
    int month;
    int year;
}DATE;

typedef struct 
{
    int hours;
    int minutes;
    int seconds;
}TIME;

typedef struct
{
    double value;
    DATE date;
    TIME time;
}TEMPERATURE;

typedef struct
{
    double value;
    DATE date;
    TIME time;
}AIRPRESSURE;

typedef struct
{
    double value;
    DATE date;
    TIME time;
}HUMIDITY;

typedef struct
{
    double value;
}BATTERYLEVEL;

typedef struct
{
    TEMPERATURE temperature;
    AIRPRESSURE airPressure;
    HUMIDITY humidity;
    BATTERYLEVEL batteryLevel;
    int spreadingFactor;
}SENSORSDATA;


#endif