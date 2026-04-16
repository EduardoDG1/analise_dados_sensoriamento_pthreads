#include <stdio.h>
#include <time.h>
#include <limits.h>

#include "structs.h"
#include "processData.h"

int main(){
    clock_t tempo = clock();

    ARGSCARREGARJSON parametrosCarregarJson = {"senzemo_cx_bg.json"};
    ARGSCARREGARJSON parametrosCarregarJsonMqtt = {"mqtt_senzemo_cx_bg.json"};
    
    //Executar em threads
    carregarJson((void *)&parametrosCarregarJson);
    carregarJson((void *)&parametrosCarregarJsonMqtt);
    
    DADOSTEMPERATURA dadosTemperatura;
    dadosTemperatura.media = 0;
    dadosTemperatura.maxima = INT_MIN;
    dadosTemperatura.minima = INT_MAX;

    DADOSUMIDADE dadosUmidade;
    dadosUmidade.media = 0;
    dadosUmidade.maxima = INT_MIN;
    dadosUmidade.minima = INT_MAX;

    DADOSPRESSAOTMOSFERICA dadosPressaoAtmosferica;
    dadosPressaoAtmosferica.media = 0;
    dadosPressaoAtmosferica.maxima = INT_MIN;
    dadosPressaoAtmosferica.minima = INT_MAX;

    DADOSBATERIA dadosBateria;
    dadosBateria.inicial = 0;
    dadosBateria.final = 10; 

    DADOSSPREADINGFACTOR dadosSpreadingFactors;
    dadosSpreadingFactors.nItens = 0;

    ESTATISTICASCAXIAS estatisticasCaxias;
    estatisticasCaxias.dadosTemperatura = dadosTemperatura;
    estatisticasCaxias.dadosUmidade = dadosUmidade;
    estatisticasCaxias.dadosPressaoAtmosferica = dadosPressaoAtmosferica;
    estatisticasCaxias.dadosBateria = dadosBateria;
    estatisticasCaxias.dadosSpreadingFactors = dadosSpreadingFactors;
    estatisticasCaxias.numeroRegistros = 0;

    ESTATISTICASBENTO estatisticasBento;
    estatisticasBento.dadosTemperatura = dadosTemperatura;
    estatisticasBento.dadosUmidade = dadosUmidade;
    estatisticasBento.dadosPressaoAtmosferica = dadosPressaoAtmosferica;
    estatisticasBento.dadosBateria = dadosBateria;
    estatisticasBento.dadosSpreadingFactors = dadosSpreadingFactors;
    estatisticasBento.numeroRegistros = 0;

    //Executar em threads
    ARGSPROCESSARJSON parametrosProcessarJson;
    parametrosProcessarJson.json = parametrosCarregarJson.json; 
    parametrosProcessarJson.estatisticasCaxias = estatisticasCaxias;
    parametrosProcessarJson.estatisticasBento = estatisticasBento;

    processarJson((void *)&parametrosProcessarJson);

    ARGSPROCESSARJSONMQTT parametrosProcessarJsonMqtt;
    parametrosProcessarJsonMqtt.json = parametrosCarregarJsonMqtt.json; 
    parametrosProcessarJsonMqtt.estatisticasCaxias = estatisticasCaxias;
    parametrosProcessarJsonMqtt.estatisticasBento = estatisticasBento;

    processarJsonMqtt((void*)&parametrosProcessarJsonMqtt);

    double seconds = (double)(clock()-tempo)/CLOCKS_PER_SEC;
    printf("%lfs\n",seconds);

    return 0;
}
