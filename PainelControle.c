//Importando bilbiotecas importantes
#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "hardware/pwm.h"
#include "hardware/clocks.h"
#include "hardware/timer.h"
#include "hardware/i2c.h"
#include "lib/ssd1306.h"
#include "lib/font.h"
#include "lib/matriz.h"

#include "FreeRTOS.h"
#include "FreeRTOSConfig.h"
#include "task.h"
#include "semphr.h"

uint usuarios_ativos = 0;  //Variável global para os ocupantes pq só duas tasks vão poder modificar ela

//Criando mutex
SemaphoreHandle_t xMutex;

//Criando semaforos para o eventos de entrada e de saida
SemaphoreHandle_t xSemBinario; //semaforo binário
SemaphoreHandle_t xSemContagemE; //semaforo para o evento de entrada
SemaphoreHandle_t xSemContagemS; //semaforo para o evento de saida

BaseType_t xHigerPriorityTaskWoken = pdFALSE;

//botão a e b em poolling?
//pino botão A
#define ButtonA 5

void vTaskEntrada(){

    gpio_init(ButtonA);
    gpio_set_dir(ButtonA, GPIO_IN);
    gpio_pull_up(ButtonA);

    while(true){
        if(!gpio_get(ButtonA)){ //pporque ele fica normalmente em sinal alto
            sleep_ms(300);
            if(!gpio_get(ButtonA)){
                //chama um dos semaforo - o de contagem de entrada
                xHigerPriorityTaskWoken = pdFALSE;
                xSemaphoreGiveFromISR(xSemContagemE, &xHigerPriorityTaskWoken); 
                portYIELD_FROM_ISR(xHigerPriorityTaskWoken);
                //tenta acessar o mutex 
            };
            vTaskDelay(pdMS_TO_TICKS(500));
        }else{
            vTaskDelay(pdMS_TO_TICKS(500));
        };
    };
};

//Pino do botão
#define ButtonB 6
void vTaskSaida(){

    gpio_init(ButtonB);
    gpio_set_dir(ButtonB, GPIO_IN);
    gpio_pull_up(ButtonB);

    while(true){
        if(!gpio_get(ButtonB)){ //pporque ele fica normalmente em sinal alto
            sleep_ms(300);
            if(!gpio_get(ButtonB)){
                //chama um dos semaforo - o de contagem de saida
                xHigerPriorityTaskWoken = pdFALSE;
                xSemaphoreGiveFromISR(xSemContagemS, &xHigerPriorityTaskWoken); 
                portYIELD_FROM_ISR(xHigerPriorityTaskWoken);
            };
            vTaskDelay(pdMS_TO_TICKS(500));
        }else{
            vTaskDelay(pdMS_TO_TICKS(500));
        };
    };
};


//Pino do Joystick
#define ButtonJoy 27
TaskHandle_t xHandleReset;

//vale aqui limpar os semáforos?
void vTaskReset(){
    while(true){
        if(xSemaphoreTake(xSemBinario, portMAX_DELAY)==pdTRUE){
            usuarios_ativos = 0;
        };
    }
};

uint32_t tempo_anterior = 0;
void InterrupcaoBotao(uint gpio, uint32_t events){
    uint32_t tempo_atual = to_ms_since_boot(get_absolute_time());
    if(tempo_atual - tempo_anterior >= 300){
        tempo_anterior = tempo_atual;
        xHigerPriorityTaskWoken = pdFALSE;
        xSemaphoreGiveFromISR(xSemBinario, &xHigerPriorityTaskWoken); 
        portYIELD_FROM_ISR(xHigerPriorityTaskWoken);
    };
};

int main()
{
    stdio_init_all();

    //configurando botão do joystick
    gpio_init(ButtonJoy);
    gpio_set_dir(ButtonJoy, GPIO_IN);
    gpio_pull_up(ButtonJoy);
    
    //Configurar o display aqui
    
    //criar mutexes e semaforos
    xMutex = xSemaphoreCreateMutex();
    xSemBinario = xSemaphoreCreateBinary();
    xSemContagemE = xSemaphoreCreateCounting(100,0);
    xSemContagemS = xSemaphoreCreateCounting(100,0);

    //criando tasks
    xTaskCreate(vTaskEntrada, "Task Entrada", configMINIMAL_STACK_SIZE, NULL, tskIDLE_PRIORITY, NULL);
    xTaskCreate(vTaskSaida, "Task Saida", configMINIMAL_STACK_SIZE, NULL, tskIDLE_PRIORITY, NULL);
    xTaskCreate(vTaskReset, "Task Reset", configMINIMAL_STACK_SIZE, NULL, tskIDLE_PRIORITY, &xHandleReset);

    vTaskStartScheduler();
    panic_unsupported();

    while(true){
        sleep_ms(200);
    };
};
