#include <stdio.h>
#include <time.h>
#include "processData.h"

int main(){

    ARGSCARREGARJSON parametros = {"senzemo_cx_bg.json"};
    ARGSCARREGARJSON parametrosMqtt = {"mqtt_senzemo_cx_bg.json"};

    carregarJson((void *)&parametros);
    carregarJson((void *)&parametrosMqtt);

    

    return 0;
}
